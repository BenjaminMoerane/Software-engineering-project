# Self code review record (Phase 6)

Reviewer: the developer (individual project; no second reviewer was available, which is a limitation; see §4). Code reviewed: all of `src/` at commit `6bfc2fb` (after the repository refactor), about 2,200 NLOC. Method: one pass per checklist section, reading against `docs/CODING_STANDARD.md` and the Phase 3 SDD, with the static-analysis and metrics output open beside the code. Each finding was fixed on branch `qa/phase6-config-qa`, retested, and merged only after all CI jobs passed locally.

## 1. Checklist

| # | Question | Result |
|---|---|---|
| A. Design fidelity |||
| A1 | Do class, interface and table names match the Phase 3 class diagram and data dictionary? | Pass. All five repository interfaces, `IPenaltyStrategy`, `ProvisionalLinearPenaltyStrategy`, `DeadlineEvaluator` and the eleven services named in SDD §4 exist under those names; the 14 tables match the data dictionary by table name (a column-level comparison has still not been run; see README). |
| A2 | Is SQL confined to the data-access layer? | Pass. Enforced by `lms_architecture_sql_isolation`. |
| A3 | Do dependencies point inward (no service includes a SQLite type)? | Pass (`grep sqlite3 src/services` is empty). |
| A4 | Is each provisional rule implemented in exactly one place? | Pass. Penalty arithmetic only in `ProvisionalLinearPenaltyStrategy`; lateness only in `DeadlineEvaluator`. |
| B. Correctness |||
| B1 | Are boundary conditions right (deadline equality, 3-day window, 0 and maximum mark)? | Pass (T_PEN_*, T_ARCH_07, T_ARCH_10 tables). |
| B2 | Is all external input validated, and do two validators of the same input agree? | **Fail → CR-04 / F11.** `isoUtc` rejected 2026-02-30 but `DeadlineEvaluator::toEpochSeconds` accepted it. |
| B3 | Are multi-row writes atomic, and is failure handled? | Pass (T_AUDIT_05, T_ARCH_08). |
| B4 | Is arithmetic free of overflow and narrowing? | Pass after clang-tidy fixes (64-bit epoch arithmetic). |
| C. Security |||
| C1 | No plaintext password stored, printed or logged? | Pass. |
| C2 | Is authorisation re-checked against stored state on every operation? | Pass (`requireValidSession`; T_ARCH_04). |
| C3 | Are all SQL statements parameterised? | Pass (every statement in `SqliteRepositories.cpp` and `Database.cpp` read). |
| C4 | Are file paths and names from the user never used to name stored files? | Pass (random server name). |
| D. Robustness |||
| D1 | Does every exception reach a handler that keeps the program running? | Pass (`runSession` catch; `main` top-level catch). |
| D2 | Is every resource owned by an RAII object? | Pass. |
| E. Maintainability |||
| E1 | Any function over CCN 10 / 60 NLOC without justification? | **Fail → CR-03.** `main` had CCN 31, 191 NLOC. |
| E2 | Read-only parameters, locals and functions `const`; variables in the narrowest scope? | **Fail → CR-01, CR-02** (cppcheck). |
| E3 | Dead code? | Pass. `departmentForCourse` (O-2) was removed in the refactor. |
| E4 | Formatting consistent with `.clang-format`? | **Fail → CR-05.** The refactored files had never been formatted. |
| E5 | Are warnings enforced on every project target? | **Fail → CR-06.** `lms_app` (`main.cpp`) was built without warning flags. |
| F. Tests and documentation |||
| F1 | Does every changed behaviour have a test that fails without the change? | Pass after CR-04 (T_ARCH_15 shown failing before the fix). |
| F2 | Do documents that state facts about the code still match it? | **Fail → CR-07.** README still said "no hosted Git remote or CI"; RTM pointed every requirement at `LmsService.cpp`. |

## 2. Findings and changes made

| ID | Finding | Change | Commit | Verification |
|---|---|---|---|---|
| CR-01 | `value` in `askSecret` declared outside the `try` that uses it (cppcheck `variableScope`). | Moved into the `try`. | `6c8da2a` | cppcheck clean; ST_ACC_* pass. |
| CR-02 | Four read-only references and loop variables not `const` (cppcheck). | Made `const`. | `6c8da2a` | cppcheck clean. |
| CR-03 | `main()` 191 NLOC / CCN 31, combining setup, login and 17 menu cases. Adding the CR1 option would have made it worse. | One handler function per option behind a `std::map<int, Handler>` dispatch table; `firstRunSetup()` and `runSession()` extracted. Every prompt and message is byte-identical (checked by script). | `6c8da2a` | 28/28 system tests pass; `main` is no longer over threshold. |
| CR-04 | **Defect F11 (Low, latent).** `toEpochSeconds` checked only `day <= 31`, so `2026-02-30` was silently read as 2 March. Current callers validate first, so no wrong penalty was possible yet, but the class that decides lateness would accept a corrupt date read from the database. | One shared `validation::daysInMonth()` used by both parsers. | `6c8da2a` | New T_ARCH_15 (Feb 30, Apr 31, 2025-02-29 rejected; 2028 and 2000 leap days accepted; 2100 rejected). It **fails before the fix and passes after**. |
| CR-05 | Refactored sources not formatted. | `clang-format -i` (formatting-only commit). | `f9cad4c` | `clang-format --dry-run --Werror` clean; added to CI. |
| CR-06 | `lms_app` compiled without warning flags; no way to make warnings fatal. | `LMS_WARNINGS_AS_ERRORS` option; same flags on `lms_app`. | `6c8da2a` | GCC and Clang, Debug and Release, `-Werror`: 0 warnings. |
| CR-07 | README and RTM out of date after the refactor. | README architecture section and remote/CI statement corrected; RTM rebuilt with Classes / Diagrams / Tables / Source columns. | `6bfc2fb` | `check_rtm.py`: 181 IDs traced, none missing. |

## 3. Findings considered and not changed

| Item | Reason |
|---|---|
| Swappable adjacent parameters (`login`, `submitFile`) | Covered by tests that would fail on a swap; strong types judged not worth the signature churn (see `STATIC_ANALYSIS_REPORT.md` §1). |
| `isoUtc` / `toEpochSeconds` CCN 23–25 | Linear validation sequences; see `CODE_METRICS.md` §3. |
| O-1 (`PasswordHasher::verify` throws on a corrupt stored hash) | Changing the exception contract is a design decision; still cannot grant access. Left open. |
| O-6 (resubmission after marking keeps the earlier mark) | Requires a policy decision that the supervisor has not made (FR-10 conflict). Not invented here. |

## 4. Limitations of a self-review

The author reviewing their own code is likely to share the blind spots that produced the defect. This was mitigated, not removed, by (a) a written checklist applied section by section, (b) tool output read before the human pass, and (c) a test-first rule for every fix. CR-04 is evidence that the human pass found something no tool did. For v1.1 the CR1 branch was reviewed against the same checklist (see `docs/maintenance/CR1_IMPACT_ANALYSIS.md`).
