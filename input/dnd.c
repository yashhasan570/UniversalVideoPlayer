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

#include "dnd.h"
#include "common/msg.h"
#include "misc/bstr.h"
#include "misc/node.h"
#include "misc/path_utils.h"
#include "osdep/threads.h"
#include "player/client.h"

static bool might_be_subtitle_file(universalvideoplayer_node *sub_exts, char *file)
{
    for (int i = 0; i < sub_exts->u.list->num; i++) {
         universalvideoplayer_node sub_ext = sub_exts->u.list->values[i];
         if (sub_ext.format != UNIVERSALVIDEOPLAYER_FORMAT_STRING)
             continue;
         if (!bstrcasecmp0(mp_get_ext(bstr0(file)), sub_ext.u.string))
             return true;
    }
    return false;
}

static void handle_dnd(universalvideoplayer_handle *universalvideoplayer, universalvideoplayer_node *files, char *action)
{
    universalvideoplayer_node sub_exts = {0};
    universalvideoplayer_node drop_type = {0};
    if (universalvideoplayer_get_property(universalvideoplayer, "sub-auto-exts", UNIVERSALVIDEOPLAYER_FORMAT_NODE, &sub_exts) != UNIVERSALVIDEOPLAYER_ERROR_SUCCESS ||
        sub_exts.format != UNIVERSALVIDEOPLAYER_FORMAT_NODE_ARRAY)
        goto end;
    if (universalvideoplayer_get_property(universalvideoplayer, "drag-and-drop", UNIVERSALVIDEOPLAYER_FORMAT_NODE, &drop_type) != UNIVERSALVIDEOPLAYER_ERROR_SUCCESS ||
        drop_type.format != UNIVERSALVIDEOPLAYER_FORMAT_STRING)
        goto end;
    if (!strcmp(drop_type.u.string, "no"))
        goto end;
    if (strcmp(drop_type.u.string, "auto"))
        action = drop_type.u.string;

    struct universalvideoplayer_node_list *list = files->u.list;
    for (int i = 0; i < list->num; i++) {
          universalvideoplayer_node file = list->values[i];
          if (file.format != UNIVERSALVIDEOPLAYER_FORMAT_STRING)
              goto end;
    }

    bool all_sub = true;
    for (int i = 0; i < list->num; i++)
        all_sub &= might_be_subtitle_file(&sub_exts, list->values[i].u.string);

    if (all_sub) {
        for (int i = 0; i < list->num; i++) {
            const char *cmd[] = {
                "osd-auto",
                "sub-add",
                list->values[i].u.string,
                NULL
            };
            universalvideoplayer_command(universalvideoplayer, cmd);
        }
    } else if (!strcmp(action, "insert-next")) {
        /* To insert the entries in the correct order, we iterate over them
           backwards */
        for (int i = list->num - 1; i >= 0; i--) {
            const char *cmd[] = {
                "osd-auto",
                "loadfile",
                list->values[i].u.string,
                /* Since we're inserting in reverse, wait til the final item
                   is added to start playing */
                (i > 0) ? "insert-next" : "insert-next-play",
                NULL
            };
            universalvideoplayer_command(universalvideoplayer, cmd);
        }
    } else {
        for (int i = 0; i < list->num; i++) {
            const char *cmd[] = {
                "osd-auto",
                "loadfile",
                list->values[i].u.string,
                /* Either start playing the dropped files right away
                   or add them to the end of the current playlist */
                (i == 0 && !strcmp(action, "replace")) ? "replace" : "append-play",
                NULL
            };
            universalvideoplayer_command(universalvideoplayer, cmd);
        }
    }

end:
    universalvideoplayer_free_node_contents(&sub_exts);
    universalvideoplayer_free_node_contents(&drop_type);
}

static MP_THREAD_VOID universalvideoplayer_event_loop_fn(void *arg)
{
    mp_thread_set_name("dnd");
    universalvideoplayer_handle *universalvideoplayer = arg;
    bool enabled = false;
    universalvideoplayer_observe_property(universalvideoplayer, 0, "dropped-files", UNIVERSALVIDEOPLAYER_FORMAT_NODE);
    universalvideoplayer_observe_property(universalvideoplayer, 0, "input-builtin-drag-and-drop", UNIVERSALVIDEOPLAYER_FORMAT_FLAG);

    int enabled_flag;
    if (universalvideoplayer_get_property(universalvideoplayer, "input-builtin-drag-and-drop", UNIVERSALVIDEOPLAYER_FORMAT_FLAG, &enabled_flag) == UNIVERSALVIDEOPLAYER_ERROR_SUCCESS)
        enabled = enabled_flag;

    while (1) {
        universalvideoplayer_event *event = universalvideoplayer_wait_event(universalvideoplayer, -1);
        if (event->event_id == UNIVERSALVIDEOPLAYER_EVENT_SHUTDOWN)
            break;
        if (event->event_id == UNIVERSALVIDEOPLAYER_EVENT_PROPERTY_CHANGE) {
            universalvideoplayer_event_property *prop = event->data;
            if (enabled && !strcmp(prop->name, "dropped-files") && prop->format == UNIVERSALVIDEOPLAYER_FORMAT_NODE) {
                universalvideoplayer_node *node = prop->data;
                universalvideoplayer_node *action = node_map_get(node, "action");
                if (!action || action->format != UNIVERSALVIDEOPLAYER_FORMAT_STRING)
                    continue;

                universalvideoplayer_node *files = node_map_get(node, "files");
                if (!files || files->format != UNIVERSALVIDEOPLAYER_FORMAT_NODE_ARRAY)
                    continue;
                handle_dnd(universalvideoplayer, files, action->u.string);
            }
            if (!strcmp(prop->name, "input-builtin-drag-and-drop") && prop->format == UNIVERSALVIDEOPLAYER_FORMAT_FLAG)
                enabled = *(int *)prop->data;
        }
    }

    universalvideoplayer_destroy(universalvideoplayer);
    MP_THREAD_RETURN();
}

void mp_dnd_init(universalvideoplayer_handle *universalvideoplayer)
{
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // Nothing drops files here, the thread only makes coverage timing dependent.
    universalvideoplayer_destroy(universalvideoplayer);
    return;
#endif
    mp_thread universalvideoplayer_event_loop;
    if (!mp_thread_create(&universalvideoplayer_event_loop, universalvideoplayer_event_loop_fn, universalvideoplayer))
        mp_thread_detach(universalvideoplayer_event_loop);
    else
        universalvideoplayer_destroy(universalvideoplayer);
}
