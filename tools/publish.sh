#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Publish the site to GitHub Pages: fm1-base's site (landing page, installer, emulator, manual), plus the tape deck.
#   tools/publish.sh VERSION [PACKAGE]     PACKAGE: the release's attested .fwsc (gh release download)
set -e
cd "$(dirname "$0")/.."
VERSION="$1"
PACKAGE="$2"
[ -n "$VERSION" ] || { echo "usage: tools/publish.sh VERSION [PACKAGE]"; exit 2; }
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}" JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export PYTHON="${PYTHON:-$PWD/.venv/bin/python}"
PATH="$PWD/.venv/bin:$PATH" sh base/tools/publish_site.sh app "$VERSION" $PACKAGE

# fm1-base's site has no place for extra pages, so the tape deck goes onto gh-pages after it
TMP="$(mktemp -d)"
trap 'git worktree remove --force "$TMP" >/dev/null 2>&1 || true' EXIT
git fetch -q origin gh-pages
git worktree add -q "$TMP" origin/gh-pages
mkdir -p "$TMP/tape"
cp web/tape.html "$TMP/tape/index.html"
git -C "$TMP" add tape
git -C "$TMP" commit -q -m "Add the tape deck" || true
git -C "$TMP" push -q origin HEAD:gh-pages
echo "published $VERSION with the tape deck"
