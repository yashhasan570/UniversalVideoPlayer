/*
 * This file is part of universalvideoplayer.
 *
 * universalvideoplayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * universalvideoplayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with universalvideoplayer.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdatomic.h>

#include "common.h"

#include "input/input.h"
#include "universalvideoplayer_talloc.h"

static atomic_bool events_pending;

static void wakeup(void *ctx)
{
    atomic_store(&events_pending, true);
}

static bool drain_events(universalvideoplayer_handle *ctx, bool encode)
{
    while (1) {
        universalvideoplayer_event *event = universalvideoplayer_wait_event(ctx, 0);
        if (event->event_id == UNIVERSALVIDEOPLAYER_EVENT_NONE)
            return true;
        if (event->event_id == UNIVERSALVIDEOPLAYER_EVENT_SHUTDOWN)
            return false;
        if (encode)
            talloc_free(mp_json_encode_event(event));
    }
}

static bool playing(universalvideoplayer_handle *ctx)
{
    int idle = 1, pause = 1;
    universalvideoplayer_get_property(ctx, "idle-active", UNIVERSALVIDEOPLAYER_FORMAT_FLAG, &idle);
    universalvideoplayer_get_property(ctx, "pause", UNIVERSALVIDEOPLAYER_FORMAT_FLAG, &pause);
    return !idle && !pause;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > MAX_FUZZ_SIZE)
        return 0;

    set_fontconfig_sysroot();

    universalvideoplayer_handle *ctx = universalvideoplayer_create();
    if (!ctx)
        exit(1);

    check_error(universalvideoplayer_set_option_string(ctx, "vo", "null"));
    check_error(universalvideoplayer_set_option_string(ctx, "ao", "null"));
    check_error(universalvideoplayer_set_option_string(ctx, "ao-null-untimed", "yes"));
    check_error(universalvideoplayer_set_option_string(ctx, "untimed", "yes"));
    check_error(universalvideoplayer_set_option_string(ctx, "pause", "yes"));
    check_error(universalvideoplayer_set_option_string(ctx, "msg-level", "all=trace"));
    check_error(universalvideoplayer_set_option_string(ctx, "network-timeout", "1"));
    check_error(universalvideoplayer_set_option_string(ctx, "input-builtin-bindings", "no"));

    initialize(ctx);

    // The IPC server reads events only when it gets woken up.
    universalvideoplayer_handle *client = universalvideoplayer_create_client(ctx, "ipc");
    if (!client)
        exit(1);
    universalvideoplayer_set_wakeup_callback(client, wakeup, NULL);
    universalvideoplayer_set_wakeup_callback(ctx, wakeup, NULL);

    const char *cmd[] = {"loadfile", "av://lavfi:yuvtestsrc=d=1[out0];sine=d=1[out1]", NULL};
    check_error(universalvideoplayer_command(ctx, cmd));

    while (1) {
        universalvideoplayer_event *event = universalvideoplayer_wait_event(ctx, -1);
        if (event->event_id == UNIVERSALVIDEOPLAYER_EVENT_PLAYBACK_RESTART ||
            event->event_id == UNIVERSALVIDEOPLAYER_EVENT_END_FILE)
            break;
    }

    // The IPC server only runs newline terminated commands.
    bool active = true;
    bstr input = {(unsigned char *)data, size};
    while (active && bstrchr(input, '\n') >= 0) {
        bstr buf = bstrdup(NULL, bstr_getline(input, &input));
        talloc_free(mp_ipc_consume_next_command(client, NULL, &buf));
        talloc_free(buf.start);
        if (atomic_exchange(&events_pending, false))
            active = drain_events(client, true) && drain_events(ctx, false);
    }

    for (int n = 0; active && n < EVENTS_LIMIT && playing(ctx); n++) {
        universalvideoplayer_event *event = universalvideoplayer_wait_event(ctx, PLAYBACK_TIME_LIMIT);
        active = event->event_id != UNIVERSALVIDEOPLAYER_EVENT_NONE &&
                 event->event_id != UNIVERSALVIDEOPLAYER_EVENT_SHUTDOWN;
    }

    universalvideoplayer_destroy(client);
    destroy(ctx);

    return 0;
}
