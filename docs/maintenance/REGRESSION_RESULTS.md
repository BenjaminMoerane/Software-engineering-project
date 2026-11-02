# CR1 / v1.1 — Regression test results (Phase 8)

Baseline: tag `v1.0`. Candidate: branch `feature/cr1-class-extensions` (released as tag `v1.1`). Environment: Ubuntu 24.04 container, GCC 13.3, Clang 18.1, CMake 3.28, GoogleTest 1.14.0, Python 3. Raw logs were produced for every row below.

## 1. Regression principle

The v1.0 suite was rerun **unchanged** against the v1.1 code. Since `v1.0`, the test sources changed only by *additions* (two methods on the in-memory fake `FakeCourses`, the new `CR1ClassExtension` system tests) and by the F13 regex fix in the RTM checker. `git diff v1.0 -- tests/unit/test_architecture.cpp tests/system/run_system_tests.py` contains **0 removed lines**, so no v1.0 assertion was weakened to make v1.1 pass.

The full v1.0 suite was also run **before** any CR1 test was written, immediately after the production code changed. All 6 CTest targets passed at that point: the CR1 code does not change v1.0 behaviour when no class extension exists.

## 2. Results

| Run | v1.0 baseline | v1.1 |
|---|---|---|
| CTest targets | 6 / 6 pass | **7 / 7 pass** (new: `lms_rtm_traceability`) |
| GoogleTest unit/integration | 155 run, 154 pass, 1 skipped | **165 run, 164 pass, 1 skipped** (+10 `T_CR1_*`) |
| System/acceptance (console) | 28 / 28 | **30 / 30** (+2 `ST_CR1_*`) |
| Skipped test | `T_FILE_11` (root can read any file; O-5) | same, for the same reason |
| GCC 13 Debug and Release, `-Werror` | pass, 0 warnings | **pass, 0 warnings** |
| Clang 18 Debug and Release, `-Werror` | pass, 0 warnings | **pass, 0 warnings** |
| GCC ASan + UBSan + LSan | 0 reports | **0 reports** |
| clang-tidy / cppcheck / clang-format | 0 / 0 / clean | **0 / 0 / clean** |
| Business-logic line coverage | 99.2% (479/483) at Phase 6 | **99.1% (524/529)**, gate ≥ 70% passes |
| Business-logic branch coverage | 90.6% (484/534) | **91.5% (528/577)** |
| All non-UI code, line coverage | 98.0% (1018/1039) | **98.1% (1079/1100)** |
| RTM check | 155 + 28 IDs traced | **165 + 30 IDs traced, none missing** |

## 2a. Upgrade test with a real v1.0 database

The database created by the **installed v1.0 binary** during the Phase 7 manual walkthrough (one released mark of 52.0, two audit rows) was opened by the installed v1.1 binary (`lms_app --version` → `1.1.0`). Before: 14 tables. After: 15 tables, the same mark and audit rows, `integrity_check` ok. A class extension was then granted through option 18, shown in the list, and an attempt to shorten it was refused. Transcript: `docs/deployment/evidence/v1_1_class_extension_transcript.txt`.

## 3. New tests for CR1

| ID | Verifies | Decision / requirement |
|---|---|---|
| T_CR1_01 | Every enrolled student (including one enrolled *after* the grant) and the lecturer see `classExtensionDeadline`; the individual field is untouched | FR-23, FR-21, CR1-D1 |
| T_CR1_02 | Boundary values around the class deadline: after the original but before the class deadline → OnTime; exactly at it → OnTime; 1 s after → LateWithinWindow, 1 day | BR-11, FR-09 |
| T_CR1_03 | Precedence: an earlier individual extension does not reduce the class extension; a later individual one wins; `deadline_used` snapshot = the later individual date | CR1-D2 |
| T_CR1_04 | A class extension can only move later (earlier and equal rejected); both changes kept as rows with authoriser and reason | CR1-D3 |
| T_CR1_05 | A submission made before the extension keeps its lateness; marking applies the original 10% penalty (80 → 70) | CR1-D4 |
| T_CR1_06 | Administrator, HoD and Student denied; a lecturer not assigned to the course denied; nothing written | FR-02, NFR-05 |
| T_CR1_07 | Invalid input: not later than original, before original, 30 February, wrong format, blank reason, unknown assignment | FR-20 |
| T_CR1_08 | Resubmission is allowed until 1 s before the class deadline and refused 1 s after | FR-10 |
| T_CR1_09 | **Migration:** a database built with the exact v1.0 schema (`tests/fixtures/schema_v1_0.sql` = `git show v1.0:database_schema.sql`) and v1.0 data, opened by v1.1, gains `class_extensions`; 15 tables; data unchanged; `integrity_check` ok; 0 FK violations; re-running the schema is a no-op | FR-19 |
| T_CR1_10 | `applicableDeadline` truth table, including the no-extension case (= v1.0), equal instants and malformed input | BR-11 |
| ST_CR1_01 | Console: lecturer option 18 → both students' lists show `class extension …`, no `your extension`; a submission after the original deadline is `OnTime` with `deadline_used` = class deadline | UC "Grant extension" A-CLASS |
| ST_CR1_02 | Console exception flows: student denied; shortening rejected with a message; history unchanged | A-CLASS exceptions |

## 4. How strong are the new tests? Mutation check

Four deliberate faults were put into the precedence rule, one at a time, in a non-`-Werror` build, and the CR1 tests were run against each:

| Mutant | Fault | Killed by |
|---|---|---|
| M1 | Class-wide extension ignored (v1.0 behaviour) | T_CR1_02, 03, 08, 10 |
| M2 | Class-wide always overrides individual | T_CR1_03, 10 |
| M3 | Earliest deadline wins instead of latest | T_CR1_02, 03, 08, 10 |
| M4 | Individual extensions ignored | T_CR1_03, 10 |

All four were killed (mutation score 4/4 for the rule). After the check the original source was restored, and the suite passes on the unmodified code.

## 5. Findings during Phase 8

| ID | Finding | Action |
|---|---|---|
| F13 (Low) | The RTM checker's regex could not match `T_CR1_*`, so it silently ignored them (it reported 155 unit IDs while GoogleTest ran 165). | Regex fixed; the check now runs as CTest `lms_rtm_traceability`, so CI enforces it. A planted bad ID `T_CR1_99` is now reported. |
| Review | The `--version` flag added in v1.0 had pushed `main()` to CCN 11, just over the threshold the Phase 6 metrics report says it is under. | Extracted into `printVersionIfRequested()`; `main()` CCN 10. |
| Docs | Defect F12 was fixed in v1.0 (`6edd1db`) but was missing from the v1.0 defect log. | Added to `DEFECT_LOG.md` in v1.1. |

## 6. Not covered

Same environment limits as v1.0: the Windows build, target-school hardware and real users are not tested, and the hosted CI run on the v1.1 commit is to be confirmed after the push (`docs/qa/CI_EVIDENCE.md`).
