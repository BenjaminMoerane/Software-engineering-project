# Coverage report (Phase 5, re-measured after the repository refactor)

Requirement (brief): at least 70% line coverage of the business-logic layer, with the actual figure reported.

**Result: 99.2% line coverage (479 of 483 lines) and 90.6% branch coverage (484 of 534 branches) of the business-logic layer** (`src/services` and `src/domain`). The requirement is met with a margin of 29.2 percentage points.

Method: GCC 13.3, Debug, `-O0 --coverage` (`-DLMS_ENABLE_COVERAGE=ON`); all six CTest targets run (153 GoogleTest tests with 1 skipped, 28 system tests, 3 earlier executables, SQL-isolation check); `gcovr 8.8`. Branch figures exclude compiler-generated exception and unreachable branches (`--exclude-throw-branches --exclude-unreachable-branches`). Raw output: `coverage_summary.txt` (line) and `coverage_branch_summary.txt` (branch). This is a measurement of one run on one tool chain, not a guarantee.

## Definition of the measured population

After the refactor the layers are physically separate, so the population is unambiguous:

| Layer | Files | In the 70% target? |
|---|---|---|
| Business logic | `src/services/*`, `src/domain/*` | Yes |
| Data access | `src/data/*`, `src/repositories/Repositories.hpp` | Reported separately |
| Security adapter | `src/security/PasswordHasher.cpp` | Reported separately |
| Presentation | `src/main.cpp` | Excluded (console I/O; covered by system tests) |

The previous figure (98.6%, 582/590) was taken when SQL lived inside `LmsService.cpp`, so it mixed persistence code into the business-logic denominator. The two figures are therefore **not directly comparable**; the new one measures a cleaner population.

## Per-file results

| File | Layer | Lines | Executed | Line % | Branch % |
|---|---|---|---|---|---|
| `src/services/AccessServices.cpp` | business | 44 | 44 | 100% | 85% |
| `src/services/AdministrationServices.cpp` | business | 73 | 72 | 98% | 100% |
| `src/services/AssessmentServices.cpp` | business | 79 | 78 | 98% | 95% |
| `src/services/DeadlineEvaluator.cpp` | business | 39 | 38 | 97% | **70%** |
| `src/services/MarkingServices.cpp` | business | 34 | 34 | 100% | 100% |
| `src/services/ModerationServices.cpp` | business | 36 | 35 | 97% | 100% |
| `src/services/LmsService.cpp` (facade) | business | 53 | 53 | 100% | 100% |
| `src/services/Validation.cpp` | business | 35 | 35 | 100% | 89% |
| `src/services/PenaltyStrategy.hpp`, `LatePenaltyPolicy.hpp`, `Services.hpp` | business | 39 | 39 | 100% | 97% |
| `src/domain/User.hpp` | business | 51 | 51 | 100% | 100% |
| **Business-logic layer** | | **483** | **479** | **99.2%** | **90.6%** |
| `src/data/SqliteRepositories.cpp/.hpp` | data access | 357 | 350 | 98% | 93% |
| `src/data/Database.cpp` | data access | 133 | 127 | 95% | 78% |
| `src/repositories/Repositories.hpp` | data access | 14 | 14 | 100% | n/a |
| `src/security/PasswordHasher.cpp` | security | 52 | 48 | 92% | 88% |
| **All non-presentation code** | | **1039** | **1018** | **98.0%** | **89.9%** |

## Interpretation

Line coverage says which statements ran; it does not say the assertions were strong. Two checks against over-reading the figure:

1. **Branch coverage is the weaker metric here (90.6%)**, and `DeadlineEvaluator.cpp` is the weakest file at 70%. Its untaken branches are mostly defensive parsing paths (malformed field widths, out-of-range components) that T_ARCH_13 only samples. Because the late-penalty rule depends on this class, it is the first place to add equivalence-class tests if marks are lost on V&V depth.
2. The suite contains explicit boundary-value tables (T_PENALTY_*, T_ARCH_07, T_ARCH_10) and negative tests (54 denied role/operation pairs in T_AUTHZ_01), so the coverage is not obtained only by happy-path execution.

## Lines not executed

| Lines | What | Reason |
|---|---|---|
| `AdministrationServices.cpp` 63 | `createUser` without a department | Low-risk path (O-4). |
| `AssessmentServices.cpp` 103 | Operating-system file-copy failure after the earlier checks pass | Needs an injected I/O failure (O-4). |
| `DeadlineEvaluator.cpp` 12 | Defensive return for an impossible field length | Not reachable through the public parser. |
| `ModerationServices.cpp` 20 | Defensive guard on an empty queue lookup | Not reachable with consistent data. |
| `SqliteRepositories.cpp` 124-130 | SQLite error branch on a constraint race | Needs an injected SQLite failure. |
| `Database.cpp` 95-98, 113, 150 | Commit failure, foreign-key pragma failure, schema file unreadable | Needs injected SQLite failures. |
| `PasswordHasher.cpp` 29, 56, 58, 66 | OpenSSL failure paths | Cannot be triggered without faking OpenSSL. |

## How to reproduce

```
cmake -S . -B build-cov -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ -DLMS_ENABLE_COVERAGE=ON
cmake --build build-cov
ctest --test-dir build-cov
gcovr -r . --object-directory build-cov --filter src/services/ --filter src/domain/ --txt --txt-metric line
gcovr -r . --object-directory build-cov --filter src/services/ --filter src/domain/ --exclude-throw-branches --exclude-unreachable-branches --txt --txt-metric branch
```
