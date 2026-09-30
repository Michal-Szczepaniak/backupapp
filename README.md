# backupapp

A backup and restore app for **Sailfish OS**. It streams your home directory, your whole rootfs, or a raw partition
straight to a WebDAV server (Nextcloud), with no full-size temporary archive stored on the phone.

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
- **Progress & ETA** in the UI; the device is kept awake while a backup runs
- **Restore from WebDAV**
  - *Partial (home) backups* are streamed and extracted live into `/`
  - *Full rootfs backups* of a hybris-style install are extracted into `/data/.stowaways/sailfishos-backup`

## Requirements

- Sailfish OS device (full-rootfs backup and restore expect a hybris-style install with the rootfs at `/data/.stowaways/sailfishos`)
- A Nextcloud (or compatible) WebDAV server with chunked-upload support
- Runtime tools (pulled in by the RPM): `tar`, `gzip`, `coreutils`, `util-linux`, `curl`

## Building

Build it like any Sailfish SDK qmake project:

```sh
sfdk build
```

or open `backupapp.pro` in the Sailfish IDE. `qwebdavlib` is bundled and compiled into the app.

## Configuration

The app has no settings UI (yet). Profiles are INI-style files in `/etc/backupapp/*.conf`, edited as root.
Profiles with an empty `name=` are hidden from the app.

The package installs a few examples:

| File                   | Purpose                                                  |
|------------------------|----------------------------------------------------------|
| `profile.conf.example` | Every key, with explanations                             |
| `home-webdav.conf`     | Home directory → WebDAV, keeps 7 backups                 |
| `full-webdav.conf`     | Full hybris rootfs → WebDAV, keeps 3 backups             |
| `home-rsync.conf`      | Home directory → rsync over SSH (*planned, not yet supported*) |

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
| `block`           | Device path, e.g. `/dev/sda12` (for `block`)                                                 |
| `destinationType` | `webdav` (the only one supported right now)                                                  |
| `webdavType`      | `http` or `https`                                                                            |
| `webdavHost`      | Server hostname                                                                              |
| `webdavRoot`      | DAV root; empty defaults to `/remote.php/dav`                                                |
| `webdavUser`      | Login name                                                                                   |
| `webdavPassword`  | Password (an app password is recommended)                                                    |
| `webdavUserId`    | Nextcloud user ID used in DAV paths; can be different from `webdavUser`                      |
| `webdavPath`      | Remote directory for this profile's backups; use a separate one for each profile             |
| `keepBackups`     | Number of backups to keep; empty or `0` keeps everything                                     |

Backups are named `backup-<profile-slug>-YYYY-MM-DD_HH-mm-ss.tar.gz` (or `.img.gz` for block devices).

## Usage

1. Create a profile in `/etc/backupapp/`.
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

## Security notes

- The binary is installed **setuid root** so it can read other users' data and raw partitions.
- **SailJail is disabled** in `backupapp.desktop`. Setuid alone doesn't get past the sandbox.
- WebDAV passwords are stored **in plain text** in `/etc/backupapp/*.conf`. Use a Nextcloud app password and keep
  the files readable by root only.
- Restoring a partial backup extracts into `/` as root and overwrites existing files.

## Status / roadmap

- [x] Directory and block-device backup to WebDAV (Nextcloud chunked upload)
- [x] Retention (`keepBackups`)
- [x] Restore of directory backups from WebDAV
- [ ] rsync
- [ ] Scheduled backups
- [ ] Settings UI

## Credits

- [qwebdavlib](https://github.com/mhaller/qwebdavlib) by Martin Haller, LGPL-2.1, bundled in `qwebdavlib/`
