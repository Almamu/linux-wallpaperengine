"""Entry point for the graphical app and background service."""

from __future__ import annotations

import sys


def main() -> None:
    if sys.argv[1:] == ["--daemon"]:
        from .daemon import run

        run()
    elif not sys.argv[1:]:
        from .gui import main as gui_main

        gui_main()
    else:
        raise SystemExit("Uso: python3 -m wallpaper_engine_app [--daemon]")


if __name__ == "__main__":
    main()
