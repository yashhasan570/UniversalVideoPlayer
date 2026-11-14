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

#include "stream.h"

#include <misc/path_utils.h>
#include <player/core.h>

static int open_universalvideoplayer(stream_t *st)
{
    st->demuxer = "universalvideoplayer";
    return STREAM_OK;
}

const stream_info_t stream_info_universalvideoplayer = {
    .name = "universalvideoplayer",
    .open = open_universalvideoplayer,
    .stream_origin = STREAM_ORIGIN_NET,
    .protocols = (const char*const[]){"universalvideoplayer", NULL},
};
