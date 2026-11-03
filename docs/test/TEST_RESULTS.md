# Test results

Gate 2 is NOT APPROVED; results for provisional rules (OD-01..OD-08, CR1-D1..D4) show the current provisional behaviour only.

## Current results: v1.1.2 (commit `72e7f38`, 3 Nov 2026)

| Run | Result |
|---|---|
| Hosted CI, GitHub Actions, Ubuntu 24.04: GCC and Clang × Debug and Release with `-Werror`, sanitizers, coverage gate, static analysis | All 7 jobs passed on `6b14dd6` (runs #23 `main` and #24 tag `v1.1.2`) and on later `main` commits; see `docs/qa/CI_EVIDENCE.md`. |
| GCC 13.3 coverage build, all 7 CTest targets, run locally as a non-root user | 7 of 7 passed. GoogleTest: 165 run, 165 passed. System tests: 31 of 31 passed. |
| Same build run as root | 7 of 7 passed; GoogleTest 164 passed, 1 skipped (`T_FILE_11`, see below). |
| Coverage, business-logic layer | 99.2% lines (525/529), 92.2% branches (532/577); see `COVERAGE_REPORT.md`. |
| RTM check (`lms_rtm_traceability`) | 165 unit/integration and 31 system test IDs; 196 traced; no RTM ID missing from the tests; no test untraced. |
| Skipped test | `T_FILE_11` (unreadable source file) skips under root, because root can read any file (O-5). It runs on the non-root GitHub Actions runner and in the non-root local run above. |

GoogleTest tests by suite: Admin 11; ArchitectureTest 9; Assignments 13; Authz 14; ClassExtensions 9; DataAccess 2; DeadlineEvaluator 5; DeadlineResolver 1; Domain 4; LmsFixture 9; Marking 14; Moderation 21; Packaging 1; PasswordHasher 3; PenaltyPolicy 7; PenaltyStrategy 1; Persistence 8; RawDb 9; Submission 24 = 165. System tests by class: UC01Submit 8, UC02Mark 5, UC03Moderate 7, Access 3, Robustness 3, Persistence 1, Offline 1, CR1ClassExtension 2, Deployment 1 = 31.

Growth since Phase 5: +10 GoogleTest and +2 system tests for CR1 (v1.1, `docs/maintenance/REGRESSION_RESULTS.md`), +2 GoogleTest tests for F11/F12 and +1 system test (`ST_DEP_01`) for F14.

## Phase 5 results (historical, after the repository refactor, commit `b1fcffc`)

Environment: Linux container, 2 CPU cores, Intel Xeon 2.10 GHz; GCC 13.3.0, Clang 18.1.3, CMake 3.28.3, Python 3.13, OpenSSL 3.0.13, GoogleTest 1.14.0.

| Run | Result |
|---|---|
| GCC 13.3 Debug, `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`: `ctest` (6 targets) | 6 of 6 passed, 0 compiler warnings. GoogleTest: 153 run, 152 passed, 1 skipped. System tests: 28 of 28 passed. |
| Clang 18.1 Debug, same warning flags: `ctest` | 6 of 6 passed, 0 compiler warnings. |
| GCC 13.3 with AddressSanitizer + UndefinedBehaviorSanitizer (+ LeakSanitizer), `-fno-sanitize-recover=all` | 6 of 6 passed; 0 sanitizer reports. |
| Clang sanitizers | NOT RUN: the Clang sanitizer runtime (`libclang_rt.asan`) is not installed in this environment. |
| GCC 13 coverage build | 6 of 6 passed; 99.2% line / 90.6% branch on the business-logic layer (see `COVERAGE_REPORT.md`). |
| Architecture check `lms_architecture_sql_isolation` (CTest) | Passed: no SQL text, `sqlite3_*` call, `Statement`/`Transaction` use or `handle()` call outside `src/data/`. |
| Skipped test | `T_FILE_11` (unreadable source file) skips under root, because root can read any file (O-5). It runs on the non-root GitHub Actions runner. |
| Full workflow, 100 consecutive runs of ST_PERS_01 (Phase 5 baseline, pre-refactor) | 100 of 100 passed (NFR-07). Not repeated after the refactor; ST_PERS_01 itself passes in every run above. |
| Offline workflow ST_OFF_01 in an isolated network namespace (Phase 5 baseline) | Passed (NFR-03). |
| RTM check (`tests/system/check_rtm.py`) | 153 unit/integration and 28 system test IDs; 181 traced; no RTM ID missing from the tests; no test untraced. |

Test counts by area: login and passwords 12; authorisation and scope 14 (the first includes 54 denied role/operation pairs); administration 11; assignments and extensions 13; submission, resubmission and files 24; penalty policy and domain 11; marking and audit 14; moderation, queue and release 21; persistence and database errors 19; architecture (fakes, strategy, deadline evaluator) 14 = 153 GoogleTest tests; plus 28 system/acceptance tests and 3 earlier executables.

## Synthetic performance benchmark (NFR-01, NFR-02)

Workload: 800 students, 801 users, 100,000 assignments in one SQLite database. Release build, 20 runs, 2-core container, not the target school hardware (OD-06 open), single user.

| Measurement | Minimum | Median | Maximum | Requirement |
|---|---|---|---|---|
| One login (scrypt verify) | 44.8 ms | 49.5 ms | 62.4 ms | under 2 s for 95% of logins |
| List 100,000 assignments | 87.2 ms | 93.1 ms | 127.8 ms | under 2 s |

These numbers are comfortably inside the stated limits on this machine, but they are **not** acceptance evidence: the target hardware and concurrency are not agreed, and only one user was measured.

## Defects found by testing (Phase 5)

F9 (no HoD moderation queue, High) and F10 (misleading partial message, Low) were found and fixed in this phase; see `DEFECT_LOG.md`. O-2 and O-3 were closed by the refactor; O-1, O-4, O-5 and O-6 remain open; O-6 (resubmission after marking vs FR-10) needs a policy decision.

## NOT VERIFIED

Windows/macOS/MSVC builds and the Windows password prompt (hosted CI is Ubuntu only); Clang sanitizers; usability (NFR-08) and technician installation (NFR-10) with people; target-hardware performance; concurrent use.
