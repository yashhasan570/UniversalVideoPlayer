#!/usr/bin/env python3
"""Re-apply the Universal Video Player rebrand after each upstream sync (idempotent).

Protected anchors (kept intact on every run):
- LICENSE.GPL / LICENSE.LGPL (legal, untouched)
- .github/ and scripts/ are excluded from the sweep entirely
"""
from pathlib import Path
import shutil

SKIP_EXT = {".png",".jpg",".jpeg",".ico",".webp",".woff",".woff2",".ttf",".eot",".otf",".gif",".pdf",".gz",".zip",".mp4"}
KEEP = {"LICENSE.GPL", "LICENSE.LGPL", "migrate.py"}
DELETE = ["RELEASE_NOTES", "Copyright", "SECURITY.md",
          ".github/FUNDING.yml", ".github/ISSUE_TEMPLATE", ".github/PULL_REQUEST_TEMPLATE"]


def sweep(t):
    return (t.replace("mpv-player/mpv", "yashhasan570/UniversalVideoPlayer")
             .replace("mpv-player", "yashhasan570")
             .replace("mpv", "universalvideoplayer")
             .replace("MPV", "UNIVERSALVIDEOPLAYER")
             .replace("Mpv", "UniversalVideoPlayer"))


def display(t):
    return t.replace("UniversalVideoPlayer", "Universal Video Player")


def main():
    for d in DELETE:
        p = Path(d)
        if p.is_dir():
            shutil.rmtree(p, ignore_errors=True)
        elif p.exists():
            p.unlink()
    for p in Path(".").rglob("*"):
        if ".git" in p.parts or not p.is_file() or p.name in KEEP or p.suffix in SKIP_EXT:
            continue
        if str(p).startswith((".github/", "scripts/")):
            continue
        try:
            t = p.read_text()
        except Exception:
            continue
        nt = sweep(t)
        if p.suffix in (".rst", ".md", ".html", ".txt"):
            nt = display(nt)
        if nt != t:
            p.write_text(nt)
    for p in list(Path(".").rglob("*mpv*")) + list(Path(".").rglob("*MPV*")) + list(Path(".").rglob("*Mpv*")):
        if ".git" in p.parts or p.is_dir():
            continue
        nb = (p.name.replace("mpv", "universalvideoplayer")
                    .replace("MPV", "UNIVERSALVIDEOPLAYER")
                    .replace("Mpv", "UniversalVideoPlayer"))
        if nb != p.name:
            p.rename(p.with_name(nb))
    readme = Path("README.md")
    if readme.exists() and not readme.read_text().startswith("# Universal Video Player"):
        print("README drifted - rewrite needed")
    print("migrate done")


if __name__ == "__main__":
    main()
