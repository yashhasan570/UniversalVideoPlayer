#!/usr/bin/env python3

# Modify X-KDE-Protocols in the universalvideoplayer.desktop file based on output from
# universalvideoplayer --list-protocols.

#
# This file is part of universalvideoplayer.
#
# universalvideoplayer is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# universalvideoplayer is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with universalvideoplayer.  If not, see <http://www.gnu.org/licenses/>.
#

import sys

if __name__ == "__main__":
    with open(sys.argv[1], encoding="UTF-8") as f:
        next(f)
        universalvideoplayer_desktop = dict([line.split("=", 1) for line in f])

    if not universalvideoplayer_desktop["X-KDE-Protocols"]:
        raise ValueError("Missing X-KDE-Protocols entry in universalvideoplayer.desktop file")

    with open(sys.argv[2], encoding="UTF-8") as universalvideoplayer_protocols_f:
        universalvideoplayer_protocols = {
            line.strip(" :/")
            for line in universalvideoplayer_protocols_f.read().splitlines()
            if "://" in line
        }

    if len(universalvideoplayer_protocols) == 0:
        raise ValueError("Unable to parse any protocols from universalvideoplayer '--list-protocols'")

    protocol_list = set(universalvideoplayer_desktop["X-KDE-Protocols"].strip().split(","))
    compatible_protocols = sorted(universalvideoplayer_protocols & protocol_list)
    universalvideoplayer_desktop["X-KDE-Protocols"] = ",".join(compatible_protocols) + "\n"

    with open(sys.argv[3], "w", encoding="UTF-8") as f:
        f.write("[Desktop Entry]" + "\n")
        for key, value in universalvideoplayer_desktop.items():
            f.write(f"{key}={value}")
