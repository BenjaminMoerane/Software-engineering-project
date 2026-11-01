# Phase 5 test plan: CS4433 Moderated LMS

Gate 2 status is **NOT APPROVED**. Provisional rules OD-01 to OD-08 (see `docs/GATE2_BLOCKERS.md`) are tested as *current provisional behaviour*, not as approved policy. A test that pins a provisional rule is marked "(provisional)" below and will need updating if the reviewer decides differently.

## 1. Scope and approach

The test suite checks the console program and the service layer against the Phase 2 requirements (FR-01..FR-22, NFR-01..NFR-10, use cases UC-01..UC-03) and the Phase 3 design (schema of 14 tables, layered services, test IDs T-LOGIN, T-AUTHZ, T-SUB-BOUNDARY, T-RESUB, T-PEN, T-EXT, T-MARK-RANGE, T-AUDIT-TX, T-MOD-RELEASE, T-SCOPE, T-PERSIST, T-DB-ERROR).

Framework: GoogleTest v1.14.0 (found with `find_package`, otherwise fetched at a pinned commit) run by CTest. System tests are Python `unittest` scripts that run the real `lms_app` executable. CI (`.github/workflows/ci.yml`) is configured to run both; no hosted CI run has been made (see section 8).

## 2. V-Model: level, artefact, test, evidence

| V-Model level | Artefact verified against | Test level here | Where | Count |
|---|---|---|---|---|
| Requirements (Phase 2 SRS) | FR/NFR/BR list, use cases UC-01..03 | Acceptance tests | `tests/system/run_system_tests.py` classes UC01Submit, UC02Mark, UC03Moderate, Access | 23 |
| System design (Phase 3 SDD, architecture) | Layers, roles, workflow state machine, schema | System tests | same file: Robustness, Persistence, Offline | 5 |
| Detailed design (classes, SQL, transactions) | LmsService, LatePenaltyPolicy, PasswordHasher, Database/Statement/Transaction, User/Role | Integration tests (service + real SQLite) | `tests/unit/test_*.cpp` (fixture creates a real temp database) | 137 integration and unit tests together (one gtest binary) |
| Implementation (source) | Individual functions | Unit tests | `tests/unit/test_domain.cpp`, `test_penalty_policy.cpp`, `test_security_login.cpp` (hasher), `test_persistence_errors.cpp` (wrappers) | included above |
| Legacy Phase 4 checks | Smoke, integration and late-policy executables | Retained unchanged | `tests/smoke_tests.cpp`, `integration_tests.cpp`, `late_policy_tests.cpp` | 3 executables |

Right-hand side of the V (each design level has a test level): see `docs/test/RTM.csv` for requirement to test links.

## 3. Entry and exit criteria

| Stage | Entry criteria | Exit criteria |
|---|---|---|
| Unit / integration | Source builds with 0 warnings (`-Wall -Wextra`); fixture compiles | All tests pass or are skipped with a stated reason; no test depends on another's data |
| System / acceptance | Unit tests pass; `lms_app` built | All ST-* tests pass; every use-case main, alternative and exception flow has a test; no open defect of severity Critical or High |
| Coverage | Unit and system tests pass in a GCC `--coverage` build | At least 70% line coverage of the business-logic layer (`src/services`, `src/domain`); the actual figure is reported |
| Release candidate | All of the above; defect log has no open Critical/High | Test results recorded in `docs/test/TEST_RESULTS.md` with the exact commands and environment |

## 4. Techniques: equivalence partitioning and boundary value analysis

Deadline and lateness (provisional OD-01..OD-04: late day = started 24-hour period after the applicable deadline; at or before the deadline = on time; beyond 3 late days = zero mark)

| Class | Representative values (relative to the applicable deadline) | Expected | Test |
|---|---|---|---|
| On time | well before; 1 s before; exactly at the deadline | OnTime, 0 late days | T_SUB_01, T_RESUB_03, ST_UC01_01 |
| Late, day 1 | +1 s; +24 h | LateWithinWindow, 1 day | T_SUB_01, T_MARK_05 |
| Late, day 2 | +24 h 1 s; +48 h | 2 days | T_SUB_01, ST_UC01_02 (30 h) |
| Late, day 3 | +48 h 1 s; +72 h | 3 days | T_SUB_01, T_MARK_06 |
| Beyond window | +72 h 1 s; days and a year later | BeyondWindow, final mark 0 | T_SUB_01, T_MARK_06, ST_UC01_03 |
| Resubmission | before / at deadline; 1 s after | accepted / accepted / rejected | T_RESUB_01..05, ST_UC01_04..05 |
| With individual extension | the revised deadline replaces the original | lateness from revised deadline | T_EXT_02, T_EXT_03, ST_UC01_06 |

Mark entry (maximum mark M; the maximum itself has boundaries 0 < M)

| Class | Values | Expected | Test |
|---|---|---|---|
| Below range | -1, -0.01 | rejected | T_MARK_02, ST_UC02_02 |
| Lower boundary | 0 | accepted | T_MARK_02, ST_UC02_02 |
| In range | 50 | accepted | T_MARK_01 |
| Upper boundary | M | accepted | T_MARK_02, ST_UC02_02 |
| Above range | M+0.01, M+1 | rejected | T_MARK_02, ST_UC02_02 |
| Not a number (NaN, infinity, text) | | rejected / re-prompted | T_PEN_07, T_MARK_02, ST_UC02_02 |
| Penalty floor | raw 15 with 3 late days; raw 10 with 1 late day (equals the penalty); raw 0 | final mark 0, never negative | T_PEN_04 |

Assignment maximum mark: 0 and negative rejected; smallest positive accepted; large accepted (T_ASG_02). Deadline text: valid UTC; wrong format; impossible calendar dates such as 30 February (T_ASG_04). Password length: 9 characters rejected, 10 accepted (T_ADMIN_11); empty and short passwords rejected (T_ADMIN_02). File size: empty rejected; 20 MiB accepted; 20 MiB + 1 byte rejected (T_FILE_05). File type: allowed extensions in either case accepted; no extension, executable or unknown rejected (T_FILE_02, T_FILE_03, ST_UC01_08).

Roles x operations: 18 service operations x each wrong role = 54 denials, plus the allowed role not denied (T_AUTHZ_01, T_AUTHZ_02); scope rules (own department, own course, own results) in T_SCOPE_01..08.

## 5. Use-case coverage

| Use case | Main flow | Alternative flows | Exception flows |
|---|---|---|---|
| UC-01 Submit | ST_UC01_01; T_SUB_02 | late (ST_UC01_02), beyond window (ST_UC01_03), resubmit (ST_UC01_04), extension (ST_UC01_06) | resubmit after deadline (ST_UC01_05), no file / not enrolled (ST_UC01_07), empty / wrong type (ST_UC01_08), database failure (T_SUB_05, T_RESUB_07, T_FILE_07) |
| UC-02 Mark | ST_UC02_01; T_MARK_01 | correction before moderation (T_MARK_07), late penalty applied (T_MARK_05/06) | out of range (ST_UC02_02), missing feedback/reason (ST_UC02_03), other course (ST_UC02_05), audit write fails (T_AUDIT_04/05) |
| UC-03 Moderate and release | ST_UC03_01 (queue, approve, release, student sees result) | return for correction and re-mark (ST_UC03_03/04) | release without approval (ST_UC03_02), change after approval (ST_UC03_05), after release (ST_UC03_06, provisional OD-08), other department (ST_UC03_07), release failure (T_MOD_12) |

## 6. Environment and how to run

Linux, GCC 13.3 and Clang 18.1, CMake 3.28, Ninja or Make, SQLite 3, OpenSSL 3, Python 3, GoogleTest 1.14.0.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure        # unit, integration, system tests
# coverage (GCC)
cmake -S . -B build-cov -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ -DLMS_ENABLE_COVERAGE=ON
cmake --build build-cov && ctest --test-dir build-cov
gcovr -r . --object-directory build-cov --filter src/ --exclude src/main.cpp --txt
# benchmark (synthetic; OD-06 target hardware not agreed)
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON && cmake --build build-bench
```

Without network access, `-DLMS_ENABLE_GTEST=OFF` skips GoogleTest and builds only the three dependency-free executables.

Unit tests take about a minute because each test creates real salted scrypt password hashes for a small user population.

## 7. Defects

Defects found while testing are recorded in `docs/test/DEFECT_LOG.md` with severity, cause, fix commit and retest.

## 8. Not covered (NOT VERIFIED)

- Windows and macOS builds, MSVC, and the Windows `_getch` password path.
- A hosted CI run and any remote repository (none exists, no URL is claimed).
- Clang AddressSanitizer/UBSan (runtime not installed); GCC sanitizers were run in Phase 4.
- NFR-08 usability with representative users and NFR-10 technician installation: no participants were available.
- NFR-01/NFR-02 against agreed target hardware (OD-06 open): only a synthetic benchmark on this environment was run.
- NFR-05 "100%" is shown for the operation x role matrix exercised at service level, not for every possible input.
- Concurrency (more than one console on one database): no requirement tests were written; not claimed.
