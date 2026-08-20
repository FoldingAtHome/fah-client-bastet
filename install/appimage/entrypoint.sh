#!/bin/bash

set -e

WS="${WORKSPACE:-/workspace}"

# Remap builder to the host uid/gid (owner of the mounted repo) so output is
# not root-owned. Override with HOSTUID/HOSTGID.
HOSTUID="${HOSTUID:-$(stat -c %u "$WS")}"
HOSTGID="${HOSTGID:-$(stat -c %g "$WS")}"
if [ "$HOSTUID" != 0 ]; then
  groupmod -g "$HOSTGID" builder 2>/dev/null || true
  usermod -u "$HOSTUID" -g "$HOSTGID" builder
fi

CBANG_HOME="${CBANG_HOME:-/tmp/cbang}"
VERSION="${VERSION:-snapshot}"

exec su builder -c "cd '$WS' && CBANG_HOME='$CBANG_HOME' VERSION='$VERSION' bash install/appimage/build-appimage.sh"
