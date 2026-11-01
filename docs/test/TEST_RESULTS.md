# Test results (Phase 5)

Environment: Linux container, 2 CPU cores, Intel Xeon 2.10 GHz; GCC 13.3.0, Clang 18.1.3, CMake 3.28.3, Python 3.13, OpenSSL 3.0.13, GoogleTest 1.14.0. Gate 2 is NOT APPROVED; results for provisional rules (OD-01..OD-08) show the current provisional behaviour only.

## Automated suites

| Run | Result |
|---|---|
| GCC 13 Debug with coverage: `ctest` (5 targets) | 5 of 5 passed. GoogleTest: 138 passed, 1 skipped. System tests: 28 of 28 passed. |
| Clang 18 Release, Ninja, `-Werror`: `ctest` | 5 of 5 passed, 0 compiler warnings or errors. |
| Skipped test | `T_FILE_11` (unreadable source file) skips under root, because root can read any file (observation O-5). |
| Full workflow, 100 consecutive runs of system test ST_PERS_01 (new program process per role, Release, GCC) | 100 of 100 passed (NFR-07 on this environment). |
| Offline workflow ST_OFF_01 in an isolated network namespace (`unshare -rn`) | Passed (NFR-03 for the console workflow). |
| RTM check (`tests/system/check_rtm.py`) | 139 unit/integration and 28 system test IDs; every test ID named in `RTM.csv` exists; every test is traced to a requirement. |

Test counts by area: login and passwords 12; authorisation and scope 14 (the first includes 54 denied role/operation pairs); administration 11; assignments and extensions 13; submission, resubmission and files 24; penalty policy and domain 11; marking and audit 14; moderation, queue and release 21; persistence and database errors 19; system and acceptance 28; plus the 3 earlier executables. These add up to 139 GoogleTest tests.

## Synthetic performance benchmark (NFR-01, NFR-02)

Workload: 800 students, 801 users, 100,000 assignments in one SQLite database. Release build, 20 runs, 2-core container, not the target school hardware (OD-06 open), single user.

| Measurement | Minimum | Median | Maximum | Requirement |
|---|---|---|---|---|
| One login (scrypt verify) | 44.8 ms | 49.5 ms | 62.4 ms | under 2 s for 95% of logins |
| List 100,000 assignments | 87.2 ms | 93.1 ms | 127.8 ms | under 2 s |

These numbers are comfortably inside the stated limits on this machine, but they are **not** acceptance evidence: the target hardware and concurrency are not agreed, and only one user was measured.

## Defects found by testing

F9 (no HoD moderation queue, High) and F10 (misleading partial message, Low) were found and fixed in this phase; see `DEFECT_LOG.md`. Observations O-1 to O-6 remain open for decisions; O-6 (resubmission after marking vs FR-10) needs a policy decision.

## NOT VERIFIED

Windows/macOS/MSVC builds and the Windows password prompt; a hosted CI run; Clang sanitizers; usability (NFR-08) and technician installation (NFR-10) with people; target-hardware performance; concurrent use.
