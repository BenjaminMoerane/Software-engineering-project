# Phase 4 repair: traceability and verification report

> Phase 5 update: the "not verified" items about GoogleTest, coverage, the benchmark, the 100-run reliability run, the offline test and the permission matrix are addressed in `docs/test/` (test plan, RTM, defect log, coverage report, results). Findings F9 and F10 and observations O-1 to O-6 are in `docs/test/DEFECT_LOG.md`. The text below is the Phase 4 record and is kept unchanged.

Gate 2 status is **NOT APPROVED** (per the Phase 3 validation report). Nothing here changes that, and no provisional rule (OD-01 to OD-08) has been promoted to approved policy.

Repair basis: the supplied `Phase_4_Implementation_Updated.zip`. It contained no `.git` directory, so history in this repository starts with a baseline commit of that snapshot (`af63ca4`, unmodified). Earlier history, if it exists elsewhere, was not available.

Authority order used: assignment brief, Phase 2 SRS/use cases, Phase 3 SDD and data dictionary, existing code.

## 1. Defects found and fixed

| ID | Evidence | Requirement / design reference | Fix (commit) | Test evidence |
|---|---|---|---|---|
| F2 | `lms_app` on a new database with stdin at EOF repeated "Setup failed: Password input ended" forever (reproduced). | FR-20 (invalid input must not hang or crash) | `src/main.cpp`: exit with status 1 when input ends during first-run setup (`462be16`) | Manual: exit status 1 on empty stdin (observed). Not an automated test. |
| F3 | `markSubmission` changed a mark the HoD had approved but not released; the old "Approved" decision then still allowed release. | SDD Moderate and release: a mark changed after review needs a fresh decision. FR-17. | `src/services/LmsService.cpp`: refuse the change until the HoD returns the mark (`949f483`) | `tests/integration_tests.cpp`. Confirmed that the new assertion fails with the guard removed and passes with it. |
| F4 | `submit()` accepted a resubmission after the applicable deadline. | Brief and BR-04/FR-10: resubmit before the deadline. SDD: reject a resubmission that violates the deadline rule. | `src/services/LmsService.cpp`: reject when `version>1` and lateness > 0 (`949f483`) | `tests/integration_tests.cpp`: first late submission accepted; late resubmission rejected; at-deadline accepted; one second after rejected; history and single current version unchanged. Confirmed failing before the fix. |
| F5 | Extra `mark_history` table not in the Phase 3 data dictionary or SDD. `mark_audit` already holds actor, time, old, new and reason. | Brief audit-log rule, FR-14, NFR-06; Phase 3 schema authoritative | Removed table, index, insert and test assertion (`1a618cb`) | Schema tables compared with the Phase 3 data dictionary: identical 14-table set. Fresh and repeated initialisation keep data; `integrity_check` ok; 0 foreign-key violations. |
| F8 | Menu options 5 (Assign lecturer) and 6 (Enrol student) passed two prompt reads as arguments of one call. C++ leaves that evaluation order unspecified: GCC 13 asked for the user ID first, Clang 18 asked for the course ID first (reproduced). | FR-22 (consistent console flow); portability | `src/main.cpp`: read the course ID first, then the user ID, into locals (`63e7d53`) | Same scripted console workflow now passes on GCC 13 and Clang 18 (section 3b). Manual/scripted, not part of CTest. |
| F1 | `BUILD_GUIDE.md` said file bytes are not managed and passwords are not masked; the code does both. README contradicted itself. | NFR-09 | Corrected guide and README (`4db7570`) | Masking observed in a real pseudo-terminal (see section 3). |
| F6 | Stale statements: "byte-for-byte" schema claim, `mark_history` references, traceability rows for FR-07/FR-10/FR-14/NFR-09, "EOF handling corrected". | Accuracy of artefacts | Corrected the false statements only (`4db7570`) | Inspection. |

## 2. Left unchanged, and why

| Item | Reason |
|---|---|
| Late-penalty arithmetic (10 points of the maximum per late day, three-day cap, 24-hour rounding, at-deadline = on time) | Provisional under OD-01 to OD-04. No new percentages, grace periods or thresholds were introduced. |
| Beyond-window submissions kept and marked `BeyondWindow` | OD-05 open (retain vs refuse). |
| Post-release marking and moderation blocked | OD-08 open. No override or correction workflow was invented. |
| Local file copy into `submission_files/` with extension and size checks | The Phase 3 SDD stores a file reference per version and says the storage convention is not yet agreed. The code stores a reference to a managed copy and needs no schema change. Documented as unapproved. |
| Class-wide extensions | CR1 / v1.1. |
| Roles, hashing (scrypt, salted), parameterised SQL, transactions, foreign keys, WAL | Working and consistent with the requirements. No change. |

## 3. Verification actually run (final commit, clean `git archive` extraction)

Environment: GCC (Linux container), CMake, SQLite, OpenSSL as installed there.

| Check | Result |
|---|---|
| Clean Release configure and build | Passed, 0 compiler warnings or errors |
| Clean Debug configure and build | Passed, 0 compiler warnings or errors |
| CTest Release / Debug | 3/3 passed in each (smoke, integration, late-policy) |
| AddressSanitizer + UBSan Debug build, CTest | 3/3 passed, no sanitizer reports |
| Baseline (supplied ZIP) before changes | Release and Debug built, 3/3 passed |
| Password masking, real pseudo-terminal (POSIX `termios` path) | Password text did not appear in terminal output; login succeeded |
| Database file inspection | Hash and salt stored; the plaintext password does not appear in the file |
| `PRAGMA integrity_check`, `foreign_key_check` | `ok`; 0 violations |
| Schema re-initialisation on a populated database | Existing row preserved |
| Database created by the supplied version (with `mark_history`) opened by the repaired build | Login worked; data kept; integrity ok |
| Restart persistence (log in again on an existing database) | Passed |

## 3b. Extended testing (final code, package extracted to a path containing spaces, `&` and non-ASCII characters)

| Check | Result |
|---|---|
| GCC 13.3 Release (Make), Debug (Ninja), `-O3 -Werror`, RelWithDebInfo, MinSizeRel | Built, 0 warnings, 3/3 tests passed each |
| Clang 18.1 Release, Debug (Ninja), `-Werror` | Built, 0 warnings, 3/3 tests passed each |
| GCC 13 ASan + UBSan (`-fno-sanitize-recover`) | 3/3 passed, no reports |
| Valgrind memcheck on each test executable (Debug) | No errors, no definite leaks |
| 100 consecutive CTest runs per compiler (GCC, Clang) | 100/100 passed each |
| 100 scripted end-to-end console workflows per compiler (admin creates department, HoD, lecturer, student, course; student submits a file; lecturer marks; HoD approves and releases; student sees the result; wrong-password rejection included), fresh directory and database each run | 100/100 passed each. Each run checked: exit 0, empty stderr, one stored file, `integrity_check` ok, 0 foreign-key violations, one audit row, one release record, `OnTime`, no plaintext passwords in the database file. |
| Failure modes: missing directory, directory as database, corrupt file, random-bytes file, no arguments | Clean error message and exit status 1 each; no crash |
| Random-garbage console input, 150 runs per compiler (fresh and populated database alternating) | 0 crashes or hangs |
| `clang-tidy` bugprone + analyzer checks on all four `.cpp` files | 1 warning (widening of `20U*1024U*1024U`), a false positive: the value fits in `unsigned int` |
| Source directory moved after building | Clean "Cannot read schema file" error, exit 1; documented in the build guide |

The end-to-end script is a verification aid and is not part of the package or CTest.

## 3a. NOT VERIFIED

- Windows `_getch` password path (not run; no Windows environment).
- Hosted Git remote and hosted CI run (none created; no URL claimed).
- Clang ASan/UBSan (the sanitizer runtime is not installed in this environment; GCC ASan/UBSan was run instead), MSVC, macOS, 32-bit builds and any Windows build.
- Static analysis: cppcheck not available (clang-tidy was run, see 3b); GCC `-fanalyzer` warnings from the earlier session are not re-evaluated (see `STATIC_ANALYSIS_REPORT.md`).
- Line coverage, the 800-student / 100,000-record benchmark (NFR-01/02), the 100-run reliability run (NFR-07), usability and installation acceptance (NFR-08/10), offline end-to-end test (NFR-03), a full negative permission matrix (NFR-05).
- GoogleTest/Catch2: the tests are dependency-free CTest executables. The brief names GoogleTest or Catch2 for Phase 5; that is not met here.
- Results in `PHASE4_PROGRESS_REPORT.md` from the earlier session that are not listed above were not re-run.

## 4. Unresolved blockers and open items

1. Gate 2 approval and OD-01 to OD-08 (see `GATE2_BLOCKERS.md`).
2. **Numbering conflict:** the OD numbering in `GATE2_BLOCKERS.md` and `GATE2_REVIEW_AND_DECISION_RECORD.md` differs from the SDD open-decision register. The validation report was not supplied; neither numbering was changed.
3. Suspected, not fixed: a resubmission after marking but before the deadline leaves the mark tied to the earlier version (FR-10 "latest valid version is the one marked"). Needs a policy decision.
4. Pre-existing and untouched: `IMPLEMENTATION_TRACEABILITY.csv` row NFR-02 has an unquoted comma (6 columns instead of 5).
5. Storage convention for submitted files not approved; the storage directory needs operating-system permissions and backup arrangements; no role-checked download interface exists.
6. The compiled-in schema path (`LMS_SCHEMA_PATH`) points to the source tree at build time, so the built program expects `database_schema.sql` to remain where it was built.

## 5. Files changed from the supplied snapshot

| File | Why |
|---|---|
| `src/main.cpp` | F2 |
| `src/services/LmsService.cpp` | F3, F4, F5 (removed history insert) |
| `database_schema.sql` | F5 |
| `tests/integration_tests.cpp` | F3/F4 regression tests; F5 assertion removed |
| `README.md`, `BUILD_GUIDE.md` | F1, F5, F6 |
| `docs/PHASE4_DELIVERABLE_AUDIT.md`, `docs/PHASE4_PROGRESS_REPORT.md`, `docs/IMPLEMENTATION_TRACEABILITY.csv`, `docs/GATE2_BLOCKERS.md` | F5, F6, numbering note |
| `docs/PHASE4_VERIFICATION_REPORT.md` | this report (new) |

No dependency, CMake, CI or architecture changes.

The tests were added to the existing `tests/integration_tests.cpp` rather than a new file. The assertion about `mark_history` was removed together with the table it tested, not to make a test pass.
