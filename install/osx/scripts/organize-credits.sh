#!/bin/sh -e
# organize existing credit logs by YYYY/MM
# https://github.com/FoldingAtHome/fah-client-bastet/pull/156

CLIENT_HOME="/Library/Application Support/FAHClient"
NAME=nobody

cd "$CLIENT_HOME/credits"

for ITEM in *.json; do
  DIR=$(stat -f %SB -t %Y/%m "./$ITEM")
  # linux guess
  # DIR=$(stat -c %.7w "./$ITEM" | tr - /)
  mkdir -p -m 0755 "$DIR"
  chown $NAME:$NAME "$DIR/.." "$DIR"
  mv -f "./$ITEM" "$DIR/"
done
