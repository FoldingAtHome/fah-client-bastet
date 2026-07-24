#!/bin/sh

# Build a portable "AnyLinux" AppImage of fah-client (sharun + uruntime +
# DwarFS). Bundles libc + linker, so it runs on any Linux (musl, old glibc).
# Runs on an Arch base; see README.md. Output lands in <repo>/out.

set -eux

ARCH="$(uname -m)"
VERSION="${VERSION:-snapshot}"
SHARUN="https://raw.githubusercontent.com/pkgforge-dev/Anylinux-AppImages/refs/heads/main/useful-tools/quick-sharun.sh"

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
test -f "$SOURCE_ROOT/SConstruct"

APPDIR="${APPDIR:-/tmp/fah-client-AppDir}"
BUILDDIR="${BUILDDIR:-/tmp/fah-client-appimage}"
OUTPATH="$SOURCE_ROOT/out"
rm -rf "$APPDIR" "$BUILDDIR"
mkdir -p "$APPDIR" "$BUILDDIR" "$OUTPATH"

# cbang is fah-client's library and has no system package; build from source.
# Pin its ref to the client version for release builds, else track master.
if [ -z "${CBANG_REF:-}" ]; then
  case "$VERSION" in
    snapshot | "") CBANG_REF=master ;;
    v*) CBANG_REF="bastet-$VERSION" ;;
    *) CBANG_REF="bastet-v$VERSION" ;;
  esac
fi

CBANG_HOME="${CBANG_HOME:-$SOURCE_ROOT/../cbang}"
if [ ! -d "$CBANG_HOME/.git" ]; then
  git clone https://github.com/cauldrondevelopmentllc/cbang "$CBANG_HOME"
fi
export CBANG_HOME

git -C "$CBANG_HOME" fetch --tags --force origin
if ! git -C "$CBANG_HOME" checkout -f "$CBANG_REF" 2>/dev/null; then
  echo "cbang ref '$CBANG_REF' not found; falling back to master" >&2
  git -C "$CBANG_HOME" checkout -f master
fi

scons -C "$CBANG_HOME"
scons -C "$SOURCE_ROOT"
test -x "$SOURCE_ROOT/fah-client"

export APPDIR
export ICON="$SOURCE_ROOT/images/fahlogo.png"
export DESKTOP="$SCRIPT_DIR/fah-client.desktop"
export OUTPATH
export VERSION
export OUTNAME="fah-client-$VERSION-$ARCH.AppImage"

cd "$BUILDDIR"
wget --retry-connrefused --tries=30 "$SHARUN" -O "$BUILDDIR/quick-sharun"
chmod +x "$BUILDDIR/quick-sharun"

./quick-sharun "$SOURCE_ROOT/fah-client"
./quick-sharun --make-appimage

ls -lh "$OUTPATH"
