#!/bin/sh
# Runs from backupapp-swap-home.service. Old entries are renamed out of the way instead of deleted, since other
# services may write into /home/defaultuser while this runs, and a rename does not care what the directory holds.
# If moving the old entries out fails, nothing restored has been moved yet, so it exits and leaves the service in
# place to retry on the next boot. Once it starts moving restored entries in it always removes the service and
# itself, so a half-done swap never runs again over the entries it already moved.

old=/home/.backupapp-old.$$

if [ -d /home/.backup ]; then
    mkdir "$old" || exit 1

    for entry in /home/* /home/.[!.]*; do
        [ -e "$entry" ] || [ -L "$entry" ] || continue

        case "$entry" in
            /home/.backup|/home/.backupapp-old.*|/home/lost+found|/home/aquota.*) ;;
            *) mv "$entry" "$old/" || { echo "could not move $entry aside, retrying on next boot" >&2; exit 1; } ;;
        esac
    done

    for entry in /home/.backup/* /home/.backup/.[!.]*; do
        [ -e "$entry" ] || [ -L "$entry" ] || continue

        name=${entry#/home/.backup/}

        case "$name" in
            lost+found|aquota.*) rm -rf "$entry"; continue ;;
        esac

        # something may have recreated the target since it was moved aside
        if [ -e "/home/$name" ] || [ -L "/home/$name" ]; then
            mkdir -p "$old/recreated" && mv "/home/$name" "$old/recreated/"
        fi

        mv "$entry" /home/ || echo "could not move $entry into /home" >&2
    done

    rmdir /home/.backup
fi

rm -f /etc/systemd/system/multi-user.target.wants/backupapp-swap-home.service \
      /etc/systemd/system/backupapp-swap-home.service \
      /usr/libexec/backupapp-swap-home.sh

rm -rf /home/.backupapp-old.*

exit 0
