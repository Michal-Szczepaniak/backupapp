#!/bin/sh
OLD=/data/.stowaways/sailfishos-old

[ -d "$OLD" ] || exit 0
[ "$(stat -c %d:%i /)" = "$(stat -c %d:%i "$OLD")" ] && exit 1

nice -n 19 rm -rf "$OLD"
