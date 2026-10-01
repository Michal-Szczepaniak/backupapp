#!/bin/sh
for loader in /backup/lib64/ld-linux*.so.* /backup/lib/ld-linux*.so.*; do
    [ -x "$loader" ] && break
done
[ -x "$loader" ] || exit 1

set --
for entry in /backup/* /backup/.[!.]*; do
    [ -e "$entry" ] && set -- "$@" "/${entry#/backup/}"
done
[ $# -gt 0 ] || exit 1

rm -rf "$@"

"$loader" --library-path /backup/lib64:/backup/usr/lib64:/backup/lib:/backup/usr/lib /backup/bin/mv /backup/* /backup/.[!.]* / 2>/dev/null

rmdir /backup
