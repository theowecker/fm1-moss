# MOSS-1

MOSS-1 is ambient firmware for the M-VAVE FM-1. It starts playing when you turn it on: lofi chords on a slow beat,
nature scenes with a quiet pad, or slow generative pads and bells. Play the keys and the music follows you, loops
what you play and lets it fade and change.

![MOSS-1's screen: a dusk lake with reeds and fireflies](manual/img/drift-dusk.png "MOSS-1 on the FM-1's screen")

This is version {{VERSION}}. You install it at your own risk, and the installer can put M-VAVE's own firmware back.

- [Try it in the browser](emu/). It runs the same code as the FM-1, with sound.
- [Install](install/) it from Chrome or Edge, with the FM-1 connected by USB.
- The [Manual](manual/) explains every control.
- [Download {{PKG}}](firmware/{{PKG}}) to use the command-line installer: `python3 tools/fm1_install.py {{PKG}}`.
- The [Source](https://github.com/theowecker/fm1-moss) is under MPL-2.0. It's built on fm1-base and felucca-bsp,
  which come from Felucca by Leo Kuroshita, Hügelton Instruments.

M-VAVE and FM-1 are trademarks of their owners. MOSS-1 is not affiliated with M-VAVE.
