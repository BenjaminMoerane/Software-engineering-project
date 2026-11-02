# Installation guide — CS4433 Moderated LMS v1.0

**Audience:** the school's IT technician. **Time:** about 30 minutes for a first installation (NFR-10). This target has not yet been timed with a technician; please note your time in §8.

The LMS is a single console program, `lms_app`, that keeps everything in one SQLite database file plus a folder of submitted files. It needs no network, server or internet access once installed (NFR-03).

> **Verification status.** Every Linux step in this guide was run on Ubuntu 24.04 (GCC 13, CMake 3.28) for this release. The Windows steps follow the same CMake build but have **not** been run by the developer; follow them and report any difference.

## 1. Before you start

| Item | Requirement |
|---|---|
| Computer | One x86-64 PC that staff and students can sit at in turn. All users share this one computer (§9 explains why). |
| Operating system | Ubuntu 22.04 or later (verified on 24.04), or Windows 10/11 (not verified) |
| Disk | About 50 MB for the program and libraries, plus room for submissions (each file is at most 20 MiB) |
| Accounts | One OS account for running the LMS. Students must **not** have OS access to the data folder (§4). |
| You will need | The release package or source, about 30 minutes, and a USB drive or share for backups |

## 2. Get the program

### Option A — Linux, from source (verified)

```bash
sudo apt update
sudo apt install -y build-essential cmake git libsqlite3-dev libssl-dev python3
git clone https://github.com/BenjaminMoerane/Software-engineering-project.git lms-src
cd lms-src
git checkout v1.0
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure        # optional but recommended: all 6 test groups should pass (about 1 minute)
sudo cmake --install build --prefix /opt/lms
```

This installs:

```
/opt/lms/bin/lms_app             the program
/opt/lms/bin/lms-start.sh        launcher (runs the program from the data folder)
/opt/lms/share/doc/CS4433_Moderated_LMS/   README, release notes, schema, these guides
```

The database schema is built into the program, so after installation you may delete `lms-src`.

### Option B — Linux, from the release package

If you were given `cs4433-moderated-lms-1.0.0-Linux.tar.gz` (made with `cpack` on the build machine), install the runtime libraries and unpack it:

```bash
sudo apt install -y libsqlite3-0 libssl3t64      # Ubuntu 24.04; on 22.04 use libssl3
sudo mkdir -p /opt/lms && sudo tar -xzf cs4433-moderated-lms-1.0.0-Linux.tar.gz -C /opt/lms --strip-components=1
```

### Option C — Windows (not verified)

1. Install Visual Studio 2022 Build Tools (workload "Desktop development with C++"), CMake and Git.
2. Install the dependencies with vcpkg: `vcpkg install sqlite3:x64-windows openssl:x64-windows`.
3. In a "x64 Native Tools" prompt:
   ```powershell
   git clone https://github.com/BenjaminMoerane/Software-engineering-project.git lms-src
   cd lms-src; git checkout v1.0
   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
   cmake --build build --config Release
   ctest --test-dir build -C Release --output-on-failure
   cmake --install build --config Release --prefix C:\LMS\app
   ```
4. Copy `sqlite3.dll`, `libcrypto-3-x64.dll` (from `build\Release\` or the vcpkg `installed\x64-windows\bin` folder) next to `C:\LMS\app\bin\lms_app.exe`.

## 3. Check the installation

```bash
/opt/lms/bin/lms_app --version          # prints: lms_app 1.0.0
```

On Windows: `C:\LMS\app\bin\lms_app.exe --version`.

If you see an error about a missing `libsqlite3` / `libcrypto` (or a missing DLL on Windows), the runtime libraries from §2 are not installed.

## 4. Create and protect the data folder

All school data lives in one folder: the database `school_lms.sqlite3` (plus `-wal` and `-shm` files while the program runs) and `submission_files/`. Students' work and marks are personal data (see §10), so only the account that runs the LMS may read this folder.

**Linux**

```bash
sudo useradd --system --create-home lms              # an account that owns the data (skip if you use an existing staff account)
sudo mkdir -p /srv/lms
sudo chown lms:lms /srv/lms
sudo chmod 700 /srv/lms
```

**Windows:** create `C:\LMS\data`. In *Properties → Security → Advanced*, disable inheritance and grant *Full control* only to *Administrators* and the account that runs the LMS. Remove *Users*.

## 5. First start and the initial Administrator

Always start the LMS with the launcher, so the database and `submission_files/` are created in the data folder rather than wherever the terminal happens to be:

```bash
sudo -u lms /opt/lms/bin/lms-start.sh /srv/lms
```

Windows: `C:\LMS\app\bin\lms-start.bat C:\LMS\data` (create a desktop shortcut to this command).

On an empty database the program asks for the **initial Administrator**: username, password (at least 10 characters; typing is hidden) and full name. Give these credentials to the school's LMS administrator in a sealed note or in person, not by email. This setup screen appears only once.

## 6. Acceptance check (5 minutes)

Log in as the Administrator and confirm each step works:

| # | Action (menu number) | Expected |
|---|---|---|
| 1 | `2` Create department "Test" | `Created department 1` |
| 2 | `3` Create user, role `Student`, department `0` | `Created user 2` |
| 3 | `0` Logout, then `quit` | `Goodbye.` |
| 4 | Start again with the launcher | No setup screen; login prompt appears; data still there |
| 5 | `ls -l /srv/lms` (or Explorer) | `school_lms.sqlite3` is present; only the LMS account has access |

You can deactivate the test student afterwards with option `14`.

## 7. Backups (weekly, and before any upgrade)

SQLite keeps recent changes in the `-wal` file until the program closes, so **close the LMS first** (everyone logs out, then `quit`) and copy the whole data folder:

```bash
sudo tar -czf /media/backup/lms-$(date +%F).tar.gz -C /srv lms
```

Windows: copy `C:\LMS\data` to the backup drive as `LMS-YYYY-MM-DD`.

**Restore:** close the LMS, replace the data folder with the backup copy, start the LMS and log in. Test a restore once per term; an untested backup is not a backup. Encrypt the backup drive (it holds learners' personal data) and keep it locked away from the computer.

## 8. Upgrading, uninstalling and recording the installation

- **Upgrade:** back up (§7), install the new version over the old one (§2), then read that version's release notes for any database step before first use.
- **Uninstall:** `sudo rm -rf /opt/lms` (or delete `C:\LMS\app`). Removing the program does **not** remove `/srv/lms`; archive or securely delete it according to the school's retention policy.
- Please record: date, technician, operating system, time taken, and any step that differed from this guide. This is the evidence for NFR-10.

## 9. Design notes for the technician

- **One computer, one user at a time.** SQLite is designed for a database on the local disk. Putting the database on a network share and opening it from several PCs is unsafe, because file locking over SMB/NFS is not reliable and can corrupt the database. This is why v1.0 is deployed on a single machine (Phase 3 SDD: local, offline deployment). Several PCs would need a client–server database, which the brief rules out of scope.
- **No passwords are stored**, only salted scrypt hashes, so a lost password cannot be recovered. v1.0 has **no password-reset function** (a known gap, see release notes): the Administrator can only deactivate the account (`14`) and create a new one (`3`). The new account does not inherit the old account's submissions or marks, so ask users to keep their passwords safe.
- Every mark change is written to an audit table, together with who made it, when, and why.

## 10. Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `error while loading shared libraries: libsqlite3.so.0` / missing DLL | Install the runtime libraries (§2). |
| `Startup failed: Could not open database: unable to open database file` | The account running the LMS cannot write to the data folder. Check ownership and permissions (§4). |
| `Operation failed: … database is locked` | A second copy of the LMS (or a backup tool) is writing to the same database. Run only one copy at a time and never back up while it is open. |
| Submitted files appear in a strange folder | The program was started without the launcher. Always use `lms-start.sh` / `lms-start.bat`. |
| `Login failed: Invalid username or password` for everyone | Caps Lock is on, or the wrong database file is open. The first line printed shows `Database: <path>`. |
| Setup screen appears again unexpectedly | A new, empty database was opened. Check the path in the `Database:` line, then restore from backup if needed. |
