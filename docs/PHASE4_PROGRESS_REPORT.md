# Phase 4 progress and verification report

## Scope and status

This package is an incremental C++17/SQLite implementation, not a claim that every Phase 4 mark-bearing requirement is complete. The formal Gate 2 status remains **NOT APPROVED** per the Phase 3 validation report. The schema has the same tables as the Phase 3 data dictionary and records provisional policy decisions separately.

> **Note:** the results below were recorded in an earlier session and were not all re-run. The checks actually executed for the final package are in `docs/PHASE4_VERIFICATION_REPORT.md`, which takes precedence.

## Implemented

- CMake build with C++17, SQLite and OpenSSL Crypto.
- RAII SQLite statement wrapper, parameterised queries, transactions, foreign-key enforcement, busy timeout and WAL.
- scrypt password hashing with random salts and constant-time comparison.
- First-run Administrator bootstrap; login; four polymorphic role types; server-side role/session verification.
- Department and user creation/update/deactivation, course creation/update, lecturer assignment and student enrolment.
- Assignment creation, individual extensions, scoped assignment listing, submission/resubmission version records with system-generated UTC timestamp.
- Lecturer marking queue, mark and written feedback persistence, isolated provisional late-penalty policy, mark audit in the same transaction.
- Department-scoped HoD moderation, returned-mark queue, explicit release, student-owned released results and blocked post-release corrections pending OD-08.
- Console menu with masked password input on Windows/POSIX terminals, local file submissions (size/extension validation and random stored filenames), README, Gate 2 blockers register and requirement traceability CSV.

## Actual checks run

Environment: GNU C++ 14.2.0, CMake, SQLite 3.46.1, OpenSSL 3.5.7 in the current execution container.

| Check | Result |
|---|---|
| Clean CMake configure | Passed |
| Clean build (`--clean-first`) | Passed |
| CTest smoke test | Passed |
| CTest integration test | Passed |
| CTest late-policy boundary test | Passed |
| Total tests in current CTest suite | 3/3 passed after file-upload and mark-history changes |
| Authentication valid/invalid password | Passed in integration test |
| Role spoofing and cross-department moderation | Passed in integration test |
| Late penalty arithmetic and boundary cases | Passed under provisional rule: on-time, one day, three-day zero floor, beyond-window zero, invalid ranges, NaN and infinity |
| Unmoderated release denial / returned-mark path | Passed |
| Forced audit insert failure rolls back mark | Passed |
| Invalid calendar timestamp rejected | Passed |
| Console first-run Administrator creation/login | Passed manually; EOF handling corrected |
| AddressSanitizer + UndefinedBehaviorSanitizer build/tests | Passed, 2/2 tests |
| GCC `-fanalyzer` | Build passed with warnings; details and qualification in `STATIC_ANALYSIS_REPORT.md` |
| Fresh extraction of final ZIP | Passed: clean Release configure/build and 3/3 CTest tests |
| SQLite integrity | `PRAGMA integrity_check` returned `ok`; `foreign_key_check` returned zero violations on first-run test DB |

The tests are dependency-free CTest executables, not GoogleTest/Catch2. GoogleTest, Catch2, cppcheck and clang-tidy were not found in the current environment. GCC `-fanalyzer` compilation completed but emitted use-of-uninitialized-value warnings in STL allocation/string paths and captured test-lambda paths; they have not all been independently proven false positives, so this is recorded as an unresolved static-analysis finding in `STATIC_ANALYSIS_REPORT.md`. The current three-test suite does not establish full requirements coverage. The additional late-policy unit test tests the policy helper only; it does not prove end-to-end extension handling, submission-deadline enforcement, or supervisor approval of the policy.

## Not yet verified or incomplete

- Formal Gate 2 approval and decisions OD-01–OD-08.
- Account update and full department/course maintenance operations.
- Dedicated service-level tests for resubmission deadline boundaries, beyond-three-day submission workflow, all mark bounds, multi-student ownership and the full negative permission matrix. File-copy validation has integration-test coverage.
- Full workflow restart test, static analysis, coverage, 100-repeat reliability run, 800-student/100,000-record performance benchmarks, target hardware measurements, usability/installation acceptance tests.
- Hosted Git remote and evidence from a clean hosted checkout. A CI workflow file is present, but no hosted CI execution is claimed.
- File storage is implemented as a local directory copy, but MIME/content validation, malware scanning, OS-level storage hardening, and a dedicated role-checked download interface remain incomplete. Masked password entry is implemented but requires manual verification on the target OS/terminal.
- CR1 class-wide extensions remain out of v1.0.

The requirement-level status and evidence are in `IMPLEMENTATION_TRACEABILITY.csv`. It intentionally distinguishes implementation from verification.
