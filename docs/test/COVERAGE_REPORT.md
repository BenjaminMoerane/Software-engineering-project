# Coverage report (Phase 5)

Requirement (brief): at least 70% line coverage of the business-logic layer, with the actual figure reported.

**Result: 98.6% line coverage (582 of 590 lines) and 94.1% branch coverage (791 of 841 branches) of the business-logic layer** (`src/services` and `src/domain`). The requirement is met.

Method: GCC 13.3, Debug, `-O0 --coverage` (`-DLMS_ENABLE_COVERAGE=ON`); all five CTest targets run (138 GoogleTest tests, 28 system tests, 3 earlier executables); `gcovr 8.8`. Branch figures exclude compiler-generated exception and unreachable branches (`--exclude-throw-branches --exclude-unreachable-branches`). Raw output: `docs/test/coverage_summary.txt`. The figure depends on the build and tool versions; it is a measurement of this run, not a guarantee.

| File | Layer | Lines | Executed | Line % | Branch % |
|---|---|---|---|---|---|
| `src/services/LmsService.cpp` | business logic | 529 | 521 | 98% | 93% |
| `src/services/LatePenaltyPolicy.hpp` | business logic | 10 | 10 | 100% | 100% |
| `src/domain/User.hpp` | business logic | 51 | 51 | 100% | 100% |
| **Business-logic layer** | | **590** | **582** | **98.6%** | **94.1%** |
| `src/data/Database.cpp` | data access | 133 | 127 | 95% | 78% |
| `src/security/PasswordHasher.cpp` | security | 52 | 42 | 80% | 68% |
| `src/main.cpp` (console I/O, not part of the figure) | presentation | 251 | 216 | 86% | n/a |

The business-logic layer is defined here as the service and domain code. The Phase 3 design shows separate repository classes, which do not exist (SQL is inside `LmsService`; see O-3 in the defect log), so there is no separate data-access layer to leave out of the figure other than `Database.cpp`.

## Lines not executed

| Lines | What | Reason / action |
|---|---|---|
| `LmsService.cpp` 86-92 | `departmentForCourse` | Never called by any code (observation O-2). |
| `LmsService.cpp` 412 | Error branch when copying the stored file fails | Needs an operating-system copy failure after the earlier checks succeed; not simulated. |
| `Database.cpp` 95-98, 113, 150 | Error branches of SQLite open/step failure paths | Need injected SQLite failures. |
| `PasswordHasher.cpp` 29, 35-37, 47, 51, 56, 58, 63, 66 | OpenSSL failure branches (allocation/derivation errors) and some error paths | Cannot be triggered without faking OpenSSL. |
| `main.cpp` 50-53, 59, 63, 68, 96, 100, 146, 271-296 | Windows-only code is not compiled here; terminal error branches; menu options 14-17 (deactivate user, update user/department/course) | Options 14-17 are covered at service level (T_ADMIN_05..08) but not through the console. Windows path NOT VERIFIED. |

## How to reproduce

```
cmake -S . -B build-cov -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ -DLMS_ENABLE_COVERAGE=ON
cmake --build build-cov
ctest --test-dir build-cov
gcovr -r . --object-directory build-cov --filter src/ --exclude src/main.cpp --txt --txt-metric line
```
