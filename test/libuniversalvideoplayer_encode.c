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

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <universalvideoplayer/client.h>

#include "libuniversalvideoplayer_common.h"

// Temporary output file
static const char *out_path;

static void cleanup(void)
{
    if (ctx)
        universalvideoplayer_destroy(ctx);
    if (out_path && *out_path)
        unlink(out_path);
}

static void wait_done(void)
{
    while (1) {
        universalvideoplayer_event *ev = universalvideoplayer_wait_event(ctx, -1.0);
        if (ev->event_id == UNIVERSALVIDEOPLAYER_EVENT_NONE)
            continue;
        printf("event: %s\n", universalvideoplayer_event_name(ev->event_id));
        if (ev->event_id == UNIVERSALVIDEOPLAYER_EVENT_SHUTDOWN)
            return;
    }
}

static void check_output(FILE *fp)
{
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    if (size < 100)
        fail("did not encode anything\n");

    char magic[4];
    fseek(fp, 0, SEEK_SET);
    size_t ret = fread(magic, sizeof(magic), 1, fp);
    static const uint8_t ebml_magic[] = {26, 69, 223, 163};
    if (ret != 1 || memcmp(magic, ebml_magic, sizeof(magic)) != 0)
        fail("output was not Matroska\n");

    puts("output file ok");
}

int main(int argc, char *argv[])
{
    ctx = universalvideoplayer_create();
    if (!ctx)
        return 1;

    atexit(cleanup);

    out_path = temp_path();
    set_property_string("o", out_path);
    set_property_string("of", "matroska");
    set_property_string("end", "1.5");
    set_property_string("terminal", "yes");
    set_property_string("msg-level", "all=v");

    if (universalvideoplayer_initialize(ctx) != 0)
        return 1;

    set_property_string("idle", "once");

    const char *cmd[] = {"loadfile", "av://lavfi:testsrc", NULL};
    command(cmd);

    wait_done();
    universalvideoplayer_destroy(ctx);
    ctx = NULL;

    FILE *output = fopen(out_path, "rb");
    if (!output)
        fail("output file doesn't exist\n");
    check_output(output);
    fclose(output);

    return 0;
}
