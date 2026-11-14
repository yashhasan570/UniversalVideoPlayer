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

#pragma once

#include <stdbool.h>

struct MPContext;

// Handles the installation of universalvideoplayer on Windows.
// Return true if universalvideoplayer should exit.
bool mp_w32_handle_register(struct MPContext *mpctx);
