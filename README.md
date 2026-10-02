# backupapp

A backup and restore app for **Sailfish OS**. It streams your home directory, your whole system, or a raw partition
straight to a WebDAV server (Nextcloud), with no full-size temporary archive stored on the phone, or syncs directories
to a server over SSH with `rsync`.

> [!WARNING]
> Parts of this app were written with the help of an LLM (Anthropic's Claude). LLM-generated code can contain subtle
> bugs, and this app runs as **root** with **SailJail sandboxing disabled**. Review the code, read the
> [Security notes](#security-notes), and test your backups before you rely on them.

## Features

- **Profile-based**: each backup job is a plain-text `.conf` file in `/etc/backupapp/`
- **Two source types**
  - `directory`: one or more paths, archived with `tar` (keeps xattrs and ACLs) into `.tar.gz`
  - `block`: a raw block device / partition, compressed with `gzip` into `.img.gz`
- **Streaming chunked upload**: the archive goes through `split` into 100 MB chunks and is uploaded with the
  Nextcloud chunked-upload API, so the phone only needs a few hundred MB of free space in `/tmp`, however big the backup is
- **Resilient**: each failed chunk is retried up to 3 times, with increasing delays
- **Retention**: `keepBackups=N` keeps the N newest backups per profile and deletes the older ones
- **Progress & ETA** in the UI; the device is kept awake while a backup or restore runs
- **Restore from WebDAV**
  - *Partial (home) backups* are streamed and extracted live into `/`
  - *Full system backups* (`fullSystemBackup=true`) are extracted into `/backup` and swapped into `/` by a preinit
    oneshot on the next boot, see [Full system restore](#full-system-restore)
- **rsync over SSH**: `directory` sources can be mirrored to a server with `rsync` instead, see [rsync](#rsync)

## Requirements

- Sailfish OS device
- A Nextcloud (or compatible) WebDAV server with chunked-upload support
- Runtime tools (pulled in by the RPM): `tar`, `gzip`, `coreutils`, `util-linux`, `curl`
- For rsync profiles: `rsync` (3.1 or newer) and an SSH client on the phone (not pulled in by the RPM), and `rsync`
  on the server

## Building

Build it like any Sailfish SDK qmake project:

```sh
sfdk build
```

or open `backupapp.pro` in the Sailfish IDE. `qwebdavlib` is bundled and compiled into the app.

## Configuration

Profiles are INI-style files in `/etc/backupapp/*.conf`. They can be edited by hand as root, or in the app under
pull down → **Settings**, where you can also add profiles (pull down → **Add profile**) and delete them (long press →
**Delete**). Profiles with an empty `name=` are hidden from the app, including the Settings page.

> [!NOTE]
> Saving a profile from the app rewrites the file through `QSettings`, which drops all `;` comments and may reorder
> keys.

The package installs a few examples:

| File                   | Purpose                                                  |
|------------------------|----------------------------------------------------------|
| `profile.conf.example` | Every key, with explanations                             |
| `home-webdav.conf`     | Home directory → WebDAV, keeps 7 backups                 |
| `full-webdav.conf`     | Full system → WebDAV, keeps 3 backups                    |
| `home-rsync.conf`      | Home directory → rsync over SSH                          |

Minimal example:

```ini
name=Home

sourceType=directory
directory=/home/defaultuser

destinationType=webdav
webdavType=https
webdavHost=cloud.example.com
webdavRoot=
webdavUser=alice
webdavPassword=secret
webdavUserId=alice
webdavPath=/backupapp/home

keepBackups=7
```

### Keys

| Key               | Description                                                                                  |
|-------------------|----------------------------------------------------------------------------------------------|
| `name`            | Name shown in the profile picker; also used in the backup file names                        |
| `sourceType`      | `directory` or `block`                                                                       |
| `directory`       | Comma-separated list of paths (for `directory`)                                              |
| `fullSystemBackup`| `true` if `directory` lists the whole system; enables [full system restore](#full-system-restore) |
| `block`           | Device path, e.g. `/dev/sda12` (for `block`)                                                 |
| `destinationType` | `webdav` or `rsync`                                                                          |
| `webdavType`      | `http` or `https`                                                                            |
| `webdavHost`      | Server hostname                                                                              |
| `webdavRoot`      | DAV root; empty defaults to `/remote.php/dav`                                                |
| `webdavUser`      | Login name                                                                                   |
| `webdavPassword`  | Password (an app password is recommended)                                                    |
| `webdavUserId`    | Nextcloud user ID used in DAV paths; can be different from `webdavUser`                      |
| `webdavPath`      | Remote directory for this profile's backups; use a separate one for each profile             |
| `keepBackups`     | Number of backups to keep; empty or `0` keeps everything (WebDAV only)                       |
| `rsyncHost`       | SSH host; empty syncs to a local `rsyncPath` instead (e.g. an SD card)                       |
| `rsyncUser`       | SSH login; empty leaves out the `user@` part                                                 |
| `rsyncPort`       | SSH port; empty defaults to `22`                                                             |
| `rsyncPath`       | Remote directory to sync into; use a separate one for each profile                           |
| `rsyncKey`        | Private key used to log in; must not require a passphrase                                    |
| `rsyncBackupOptions`  | Extra `rsync` arguments for backups, separated by spaces                                 |
| `rsyncRestoreOptions` | Extra `rsync` arguments for restores, separated by spaces                                |

Backups are named `backup-<profile-slug>-YYYY-MM-DD_HH-mm-ss.tar.gz` (or `.img.gz` for block devices).

## Usage

1. Create a profile in **Settings** or by hand in `/etc/backupapp/`.
2. Open **backupapp**, pick the profile, and tap **Backup now!**
3. To restore, pull down → **Restore**, pick the profile and a backup file, and tap **Restore backup**.

## How it works

```
du / blockdev  →  size estimate for progress
tar -czf - …   ─┐
  or gzip -c    ├─→  split -b 100M  →  /tmp/backupapp-chunks/chunk-NNNN
                ┘                          │
                                           ▼
                        MKCOL  /uploads/<user>/backupapp-<ts>
                        PUT    …/1, …/2, …      (one chunk at a time)
                        MOVE   …/.file  →  /files/<user>/<webdavPath>/<backup>
                        PROPFIND + DELETE       (retention)
```

`split` is paused (`SIGSTOP`) when 3 chunks are waiting and resumed once the upload catches up. This keeps disk
usage on the phone low.

## rsync

With `destinationType=rsync`, the directories in `directory=` are synced to `rsyncUser@rsyncHost:rsyncPath`
(`rsyncHost:rsyncPath` without a user, or just the local `rsyncPath` without a host, in which case no SSH is used) with:

```sh
rsync -aAXHR --info=progress2 --no-inc-recursive -e "ssh -p <rsyncPort> -i <rsyncKey> -o BatchMode=yes" <rsyncBackupOptions> <directories> <destination>
```

- The server keeps a single mirror, not dated backups. `keepBackups` doesn't apply.
- `-R` keeps full paths, so `/home/defaultuser` ends up in `rsyncPath/home/defaultuser`.
- Nothing is deleted on the server unless you add `--delete` (and `--delete-excluded`) to `rsyncBackupOptions`.
- `rsync` exit code 24 (files vanished during the transfer) counts as success.
- Options are split on spaces with no shell quoting, so `--rsync-path="rsync --fake-super"` doesn't work. Use
  `-M--fake-super` instead, in both `rsyncBackupOptions` and `rsyncRestoreOptions`.
- SSH runs in batch mode and can't ask questions. Log in to the server once by hand as root first, so its host key
  is in `known_hosts`.

**Restore** pulls back only the directories listed in `directory=` (as `rsyncPath/./<directory>`) with
`rsync -aAXHR --numeric-ids` and `rsyncRestoreOptions`, into `/`, or into `/backup` for full system profiles, which
then continue as in [Full system restore](#full-system-restore). The target directory itself is never synced, so `/`
and `/backup` keep their own owner, permissions and SELinux label. The restore page shows `rsyncPath` as the only
backup to pick. `rsyncRestoreOptions` is separate from `rsyncBackupOptions` so you can choose whether a restore deletes
files that aren't in the backup.

## Full system restore

The running root can't be replaced in place, so a full system restore happens in two steps.

**Backup.** List the system's top-level directories in `directory=`, without mount points:

```ini
fullSystemBackup=true
directory=/bin,/boot,/etc,/home,/lib,/media,/opt,/root,/sbin,/srv,/usr,/var
```

Add `/lib64` on aarch64. Leave out `/home` if it's mounted from an SD card, and back it up with a separate profile.
Don't list `/proc`, `/sys`, `/dev`, `/run`, `/tmp`, `/data`, `/mnt` or Android partitions like `/system` or `/vendor`.

**Restore.** The app:

1. extracts the backup into `/backup` (it must not exist or must be empty)
2. queues each executable in `/backup/var/lib/platform-updates` (e.g. `flash-bootimg.sh`) as a preinit oneshot of
   the restored system
3. queues `/usr/lib/oneshot.d/backupapp-swap-rootfs.sh` as a preinit oneshot of the running system
4. asks you to reboot

**On the next boots:**

1. The running system's preinit runs the swap script. It `rm -rf`s every top-level directory that exists in
   `/backup`, then moves the ones from `/backup` into `/`. Anything not in the backup (e.g. `/home` on an SD card) is
   left alone. The move uses the backup's own `mv` and dynamic loader, because the old ones are already deleted
   at that point. preinit then reboots.
2. The restored system's preinit runs the platform updates and reboots.
3. The restored system boots normally.

**Things to know:**

- `/backup` is on the root partition, so you need free space for a second copy of the system during the restore.
- There is no rollback. If the swap fails halfway, fix it from recovery.
- `/etc` comes from the backup. If the source device mounts something (like an SD card on `/home`) that the target
  doesn't have, remove or `nofail` that mount in `/backup/etc` before rebooting.
- Before rebooting, you can check that the swap won't depend on the old system:
  ```sh
  LD_DEBUG=libs /backup/lib/ld-linux-armhf.so.3 --library-path /backup/lib:/backup/usr/lib /backup/bin/mv --version 2>&1 | grep 'calling init'
  ```
  Every path should start with `/backup/`.

## Security notes

- The binary is installed **setuid root** so it can read other users' data and raw partitions. Install it with the
  RPM; copying the files by hand loses the setuid bit.
- **SailJail is disabled** in `backupapp.desktop`. Setuid alone doesn't get past the sandbox.
- `backupapp.desktop` has no `X-Nemo-Application-Type`, so the launcher uses the generic invoker type. With
  `silica-qt5`, the app is loaded into a booster process that isn't root, and `setuid(0)` fails at startup.
- WebDAV passwords are stored **in plain text** in `/etc/backupapp/*.conf`. Use a Nextcloud app password and keep
  the files readable by root only.
- Restoring a partial backup extracts into `/` as root and overwrites existing files.
- The SSH key for rsync profiles has no passphrase. Anyone who can read it can log in to the server, so keep it
  readable by root only.

## Status / roadmap

- [x] Directory and block-device backup to WebDAV (Nextcloud chunked upload)
- [x] Retention (`keepBackups`)
- [x] Restore of directory backups from WebDAV
- [x] Full system restore via a preinit swap
- [x] rsync over SSH, backup and restore
- [ ] Scheduled backups
- [x] Settings UI for creating, editing and deleting profiles

## Credits

- [qwebdavlib](https://github.com/mhaller/qwebdavlib) by Martin Haller, LGPL-2.1, bundled in `qwebdavlib/`
