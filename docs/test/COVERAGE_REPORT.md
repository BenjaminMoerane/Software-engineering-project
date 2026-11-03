# Coverage report (v1.1.2)

Requirement (brief): at least 70% line coverage of the business-logic layer, with the actual figure reported.

**Result at v1.1.2 (commit `72e7f38`): 99.2% line coverage (525 of 529 lines) and 92.2% branch coverage (532 of 577 branches) of the business-logic layer** (`src/services` and `src/domain`). The requirement is met with a margin of 29.2 percentage points. CI enforces the 70% line threshold on every push (`coverage` job, `--fail-under-line 70`).

Method: GCC 13.3, Debug, `-O0 --coverage` (`-DLMS_ENABLE_COVERAGE=ON`); all 7 CTest targets run (165 GoogleTest tests, 31 system tests, 3 earlier executables, SQL-isolation check, RTM check); `gcovr 8.8`. Branch figures exclude compiler-generated exception and unreachable branches (`--exclude-throw-branches --exclude-unreachable-branches`). Raw output: `coverage_summary.txt` (line) and `coverage_branch_summary.txt` (branch). Measured locally on 3 Nov 2026 as a **non-root** user, the same condition as the GitHub Actions runner. Each hosted run publishes its own figures in its `coverage-report` artifact (`coverage-line.txt`, `coverage-branch.txt`). This is a measurement of one run on one tool chain, not a guarantee.

**Root versus non-root.** Run as root, `T_FILE_11` (unreadable submission file) is skipped, because root can read any file (O-5). The file-copy failure path it exercises is then not executed, and the business-logic figures fall to 99.1% lines (524/529) and 91.5% branches (528/577). Those are the figures in the v1.1 release notes, which were measured in a root container. Both are correct for their condition; the non-root figure is the one CI produces.

## Definition of the measured population

The layers are physically separate, so the population is unambiguous:

| Layer | Files | In the 70% target? |
|---|---|---|
| Business logic | `src/services/*`, `src/domain/*` | Yes |
| Data access | `src/data/*`, `src/repositories/Repositories.hpp` | Reported separately |
| Security adapter | `src/security/PasswordHasher.cpp` | Reported separately |
| Presentation | `src/main.cpp` | Excluded (console I/O; covered by system tests) |

## Per-file results

| File | Layer | Lines | Executed | Line % | Branch % |
|---|---|---|---|---|---|
| `src/services/AccessServices.cpp` | business | 45 | 45 | 100% | **85%** |
| `src/services/AdministrationServices.cpp` | business | 73 | 72 | 98% | 100% |
| `src/services/AssessmentServices.cpp` | business | 96 | 96 | 100% | 100% |
| `src/services/DeadlineEvaluator.cpp` | business | 51 | 50 | 98% | **74%** |
| `src/services/MarkingServices.cpp` | business | 33 | 33 | 100% | 100% |
| `src/services/ModerationServices.cpp` | business | 36 | 35 | 97% | 100% |
| `src/services/LmsService.cpp` (facade) | business | 67 | 67 | 100% | 100% |
| `src/services/Validation.cpp` | business | 38 | 37 | 97% | 90% |
| `src/services/PenaltyStrategy.hpp`, `LatePenaltyPolicy.hpp`, `Services.hpp` | business | 39 | 39 | 100% | 97% |
| `src/domain/User.hpp` | business | 51 | 51 | 100% | 100% |
| **Business-logic layer** | | **529** | **525** | **99.2%** | **92.2%** |
| `src/data/SqliteRepositories.cpp/.hpp` | data access | 377 | 370 | 98% | 93% |
| `src/data/Database.cpp` | data access | 128 | 123 | 96% | 80% |
| `src/repositories/Repositories.hpp` | data access | 14 | 14 | 100% | n/a |
| `src/security/PasswordHasher.cpp` | security | 52 | 48 | 92% | 88% |
| **All non-presentation code** | | **1,100** | **1,080** | **98.2%** | **91.2%** |

## Interpretation

Line coverage says which statements ran; it does not say the assertions were strong. Three checks against over-reading the figure:

1. **Branch coverage is the weaker metric (92.2%)**, and `DeadlineEvaluator.cpp` is the weakest file at 74%. Its untaken branches are mostly defensive parsing paths (malformed field widths, out-of-range components) that T_ARCH_13 only samples. Because the late-penalty rule depends on this class, it is the first place to add equivalence-class tests.
2. The suite contains explicit boundary-value tables (T_PEN_*, T_ARCH_07, T_ARCH_10) and negative tests (54 denied role/operation pairs in T_AUTHZ_01), so the coverage is not obtained only by happy-path execution. The CR1 precedence rule was also mutation-checked (4 of 4 mutants killed, `docs/maintenance/REGRESSION_RESULTS.md`).
3. Coverage is a census of one deterministic test run, not a sample, so it has no sampling error; but it is conditional on the environment (see root versus non-root above).

## Lines not executed

| Lines | What | Reason |
|---|---|---|
| `AdministrationServices.cpp` 63 | `createUser` without a department | Low-risk path (O-4). |
| `DeadlineEvaluator.cpp` 13 | Non-digit or short field inside the timestamp parser | Rejected earlier by the format check; defensive. |
| `ModerationServices.cpp` 20 | Defensive guard on an empty queue lookup | Not reachable with consistent data. |
| `Validation.cpp` 36 | Month outside 1–12 in `daysInMonth` | Callers validate the month first; defensive. |
| `SqliteRepositories.cpp` 126–132 | SQLite error branch on a constraint race | Needs an injected SQLite failure. |
| `Database.cpp` 92–95, 110 | Commit failure, foreign-key pragma failure | Need injected SQLite failures. |
| `PasswordHasher.cpp` 29, 56, 58, 66 | OpenSSL failure paths | Cannot be triggered without faking OpenSSL. |

## History

| Point | Business-logic lines | Branches | Note |
|---|---|---|---|
| Phase 5, before the repository refactor | 98.6% (582/590) | — | SQL lived in `LmsService.cpp`, so persistence code was in the denominator; not comparable with later rows. |
| Phase 5, after the refactor (`b1fcffc`, root) | 99.2% (479/483) | 90.6% (484/534) | |
| v1.1 / v1.1.1 release notes (root) | 99.1% (524/529) | 91.5% (528/577) | |
| **v1.1.2 (`72e7f38`, non-root)** | **99.2% (525/529)** | **92.2% (532/577)** | Current |

## How to reproduce

```
cmake -S . -B build-cov -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ -DLMS_ENABLE_COVERAGE=ON
cmake --build build-cov
ctest --test-dir build-cov          # run as a non-root user to include T_FILE_11
gcovr -r . --object-directory build-cov --filter src/services/ --filter src/domain/ --txt --txt-metric line
gcovr -r . --object-directory build-cov --filter src/services/ --filter src/domain/ --exclude-throw-branches --exclude-unreachable-branches --txt --txt-metric branch
```
