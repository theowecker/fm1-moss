#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""After ./build.sh --web: the manual and the tape deck beside the emulator, and the manual shown on its page.

  tools/emu_extras.py PYTHON"""
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EMU = ROOT / "app/build/emu"
MARK = "<!-- moss manual -->"
SECTION = MARK + """
<section style="margin-top:2rem">
  <iframe src="manual/" title="MOSS-1 manual" style="width:100%;border:0;display:block;min-height:600px"
    onload="this.style.height = this.contentDocument.documentElement.scrollHeight + 'px'"></iframe>
</section>
"""


def main(python):
    manual = EMU / "manual"
    manual.mkdir(parents=True, exist_ok=True)
    subprocess.run([python, str(ROOT / "base/tools/manual_html.py"), str(ROOT / "app/docs/MANUAL.md"),
                    str(manual / "index.html"), "--name", "MOSS-1",
                    "--tagline", "lofi, nature sounds and ambient loops for the FM-1"],
                   check=True, stdout=subprocess.DEVNULL)
    shutil.copytree(ROOT / "app/docs/img", manual / "img", dirs_exist_ok=True)
    shutil.copy(ROOT / "web/tape.html", EMU / "tape.html")
    page = EMU / "index.html"
    html = page.read_text(encoding="utf-8")
    if MARK not in html:
        # the emulator page's own <main> ends after the Screen Reader panel: the manual goes just inside it
        html = html.replace("</main>", SECTION + "</main>", 1)
        page.write_text(html, encoding="utf-8")
    print(f"extras   {manual / 'index.html'} (on the emulator page), {EMU / 'tape.html'}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "python3")
