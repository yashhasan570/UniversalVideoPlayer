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

#include <string.h>

#include "options/path.h"
#include "osdep/threads.h"
#include "path.h"

#include "config.h"

static mp_once path_init_once = MP_STATIC_ONCE_INITIALIZER;

static char universalvideoplayer_home[512];
static char old_home[512];
static char universalvideoplayer_cache[512];
static char old_cache[512];

static void path_init(void)
{
    char *home = getenv("HOME");
    char *xdg_config = getenv("XDG_CONFIG_HOME");

    if (xdg_config && xdg_config[0] == '/') {
        snprintf(universalvideoplayer_home, sizeof(universalvideoplayer_home), "%s/universalvideoplayer", xdg_config);
    } else if (home && home[0]) {
        snprintf(universalvideoplayer_home, sizeof(universalvideoplayer_home), "%s/.config/universalvideoplayer", home);
    }

    // Maintain compatibility with old ~/.universalvideoplayer
    if (home && home[0]) {
        snprintf(old_home, sizeof(old_home), "%s/.universalvideoplayer", home);
        snprintf(old_cache, sizeof(old_cache), "%s/.universalvideoplayer/cache", home);
    }

    if (home && home[0])
        snprintf(universalvideoplayer_cache, sizeof(universalvideoplayer_cache), "%s/Library/Caches/io.universalvideoplayer", home);

    // If the old ~/.universalvideoplayer exists, and the XDG config dir doesn't, use the old
    // config dir only.
    if (mp_path_exists(old_home) && !mp_path_exists(universalvideoplayer_home)) {
        snprintf(universalvideoplayer_home, sizeof(universalvideoplayer_home), "%s", old_home);
        snprintf(universalvideoplayer_cache, sizeof(universalvideoplayer_cache), "%s", old_cache);
        old_home[0] = '\0';
        old_cache[0] = '\0';
    }
}

const char *mp_get_platform_path_darwin(void *talloc_ctx, const char *type)
{
    mp_exec_once(&path_init_once, path_init);
    if (strcmp(type, "home") == 0)
        return universalvideoplayer_home;
    if (strcmp(type, "old_home") == 0)
        return old_home;
    if (strcmp(type, "cache") == 0)
        return universalvideoplayer_cache;
    if (strcmp(type, "global") == 0)
        return UNIVERSALVIDEOPLAYER_CONFDIR;
    if (strcmp(type, "desktop") == 0)
        return getenv("HOME");
    return NULL;
}
