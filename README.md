# MOSS-1

Ambient firmware for the M-VAVE FM-1: lofi, nature scenes and slow generative loops, with a looper that listens to
what you play. Built on [fm1-base](https://github.com/charlesvestal/fm1-base).

How to use it: [app/docs/MANUAL.md](app/docs/MANUAL.md).

## Try it without an FM-1

```
./build.sh --web
cd app/build/emu && python3 -m http.server 8053
```

Open http://127.0.0.1:8053 in Chrome.

## Build

You need Docker, the JieLi toolchain and the AC79 SDK (see `base/README.md`), and a `.venv` with Pillow built
against libraqm.

```
git submodule update --init --recursive
sh tests/run_tests.sh      # renders every scene, stress tests every space, runs the simulator tests
./build.sh                 # makes app/build/moss.fwsc
```

## Install

```
.venv/bin/python base/tools/fm1_install.py app/build/moss.fwsc
```

To go back to the stock firmware, see the end of the manual.
