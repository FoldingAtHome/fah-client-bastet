#!/bin/sh -e
# organize existing credit logs by YYYY/MM
# https://github.com/FoldingAtHome/fah-client-bastet/pull/156

CLIENT_HOME="/Library/Application Support/FAHClient"
CREDITS_DIR="$CLIENT_HOME/credits"

[ -d "$CREDITS_DIR" ] || exit 0
cd "$CREDITS_DIR"

OWNER_GROUP=$(stat -f '%Su:%Sg' .)
for ITEM in *.json; do
  [ -f "$ITEM" ] || continue
  DIR=$(stat -f %SB -t %Y/%m "./$ITEM")
  # linux guess
  # DIR=$(stat -c %.7w "./$ITEM" | tr - /)
  mkdir -p -m 0755 "$DIR"
  chown "$OWNER_GROUP" "${DIR%/*}" "$DIR"
  mv -f "./$ITEM" "$DIR/"
done
