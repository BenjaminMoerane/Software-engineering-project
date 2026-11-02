# Deployment (Phase 7, release v1.0)

| Deliverable | File |
|---|---|
| UML deployment diagram | `deployment_diagram.svg` / `.png` (source `deployment_diagram.dot`, Graphviz) |
| Installation guide for a school technician | `INSTALLATION_GUIDE.md` |
| User manuals | `USER_MANUAL_COMMON.md` plus one per role: Administrator, Lecturer, HoD, Student |
| Release notes | `../../RELEASE_NOTES.md` |
| Walkthrough evidence | `evidence/manual_walkthrough_*.txt`: every manual flow run on the installed v1.0 binary |

## Deployment decision

**Chosen:** one school computer runs `lms_app` against a local SQLite database in a permission-restricted data folder; users take turns at the console; backups go to a separate encrypted medium. This meets the IT officer's constraints (old computers, unreliable internet → offline, NFR-03) with the fewest moving parts.

**Rejected:**

- **Database on a network share, program on several PCs.** It looks like an easy way to give the "local network" option in the brief, but SQLite's own documentation warns that file locking on network filesystems is unreliable and can corrupt the database. The risk falls on assessment records, which the school must be able to trust.
- **Client–server (database server + clients).** This would support many simultaneous users, but needs a server, a network and administration that the IT officer's statement suggests the school cannot sustain. It is also a larger change than the Phase 3 architecture allows. It is the natural path if concurrent use becomes a requirement.

**Consequence:** NFR-01 ("responds quickly even with all 800 students on it") is met in the sense of 800 *stored* accounts (synthetic benchmark: login median 49.5 ms), not 800 *simultaneous* users. That interpretation is recorded as an open decision (OD-06) rather than claimed as met.

**Packaging:** CPack builds `cs4433-moderated-lms-1.0.0-Linux.tar.gz` / `.zip` (`cpack --config build/CPackConfig.cmake`). The database schema is compiled into the program (defect F12 fix), so the package needs no source tree. Verified by installing the Release build, moving the source tree away and starting on a fresh database: 14 tables, `integrity_check` ok.
