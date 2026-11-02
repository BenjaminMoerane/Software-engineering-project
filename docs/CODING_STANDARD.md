# Coding standard — CS4433 Moderated LMS

Status: in force from Phase 6 (commit history before `8be1fa9` predates automated formatting). Scope: all C++ under `src/` and `tests/`, plus the Python test drivers under `tests/`. Every rule marked **[auto]** is checked by a tool in CI, so a breach fails the build; the others are checked by the self-review checklist (`docs/qa/CODE_REVIEW_RECORD.md`).

The standard is adapted from the C++ Core Guidelines (Stroustrup and Sutter) and the LLVM style, reduced to the rules that matter for this codebase. Where a rule is not stated here, the Core Guidelines apply.

## 1. Language and build

| Rule | Check |
|---|---|
| 1.1 ISO C++17, no compiler extensions (`CMAKE_CXX_EXTENSIONS OFF`). | [auto] CMake |
| 1.2 The project builds with no warnings under `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` on GCC and Clang (MSVC `/W4`). | [auto] CI `-DLMS_WARNINGS_AS_ERRORS=ON` |
| 1.3 New source files are added to `CMakeLists.txt` explicitly (no globbing), so the build is reproducible. | Review |

## 2. Layout and naming

| Rule | Check |
|---|---|
| 2.1 Formatting follows `.clang-format` (LLVM base, 4-space indent, 120 columns, left pointer alignment). | [auto] `clang-format --dry-run --Werror` |
| 2.2 Types, classes and enumerators: `PascalCase` (`SubmissionService`, `Role::HeadOfDepartment`). Interfaces start with `I` (`ISubmissionRepository`, `IPenaltyStrategy`), matching the Phase 3 class diagram. | Review |
| 2.3 Functions and variables: `camelCase`. Private data members end in `_` (`users_`). Compile-time constants: `kPascalCase` (`kWindowDays`). | Review |
| 2.4 Class, interface and table names used in code must be the names in the Phase 3 SDD and data dictionary, so traceability holds (brief section 6). | Review + RTM check |
| 2.5 Test names carry the Phase 3/5 test ID: `T_AREA_NN_description` or `ST_AREA_NN_description`. | [auto] `tests/system/check_rtm.py` |

## 3. Architecture rules

| Rule | Check |
|---|---|
| 3.1 **SQL, `<sqlite3.h>`, `sqlite3_*` calls and `data::Statement`/`Transaction` appear only in `src/data/`.** Services depend on the interfaces in `src/repositories/Repositories.hpp`. | [auto] CTest `lms_architecture_sql_isolation` |
| 3.2 Every SQL statement is parameterised (`?1`, `?2` …). String concatenation of user input into SQL is forbidden. | Review (all SQL is in one file) |
| 3.3 Presentation (`src/main.cpp`) makes no authorisation or workflow decision; every rule is enforced in a service (SDD: "hiding a menu item is not a security boundary"). | Review + T_AUTHZ tests |
| 3.4 Business rules that are still provisional (OD-01 … OD-08) live behind one named class (`ProvisionalLinearPenaltyStrategy`, `DeadlineEvaluator`) and are never duplicated. | Review |
| 3.5 Dependencies point inward: presentation → services → domain/repository interfaces ← SQLite implementations. | Review |

## 4. Resource management and types

| Rule | Check |
|---|---|
| 4.1 RAII for every resource: SQLite handles, statements, transactions and terminal state are owned by objects whose destructors release them. No naked `new`/`delete`. | Review, clang-tidy |
| 4.2 A class that declares a destructor declares or deletes all five special members (rule of five); polymorphic bases delete copy/move to prevent slicing. | [auto] clang-tidy `cppcoreguidelines-special-member-functions`, `-slicing` |
| 4.3 Every variable is initialised at declaration. | [auto] clang-tidy `cppcoreguidelines-init-variables` |
| 4.4 Use `override` on overriding functions, `final` on leaf classes, `nullptr` not `NULL`/`0`. | [auto] clang-tidy `modernize-*` |
| 4.5 Narrowing conversions are explicit (`static_cast`), including widening before multiplication. | [auto] `-Wconversion`, clang-tidy `bugprone-implicit-widening-*` |
| 4.6 Pass non-trivial types by `const&`; mark read-only parameters, locals and member functions `const`. | [auto] cppcheck `constParameter*`, clang-tidy |

## 5. Errors and input

| Rule | Check |
|---|---|
| 5.1 Errors are reported with exceptions (`std::invalid_argument` for bad input, `std::runtime_error` for refused operations or persistence failure). Exceptions are caught at the application boundary (`main.cpp`) and printed without database or credential detail (FR-20). | Review + T_DB_ERROR tests |
| 5.2 All external input (console, file paths, timestamps, marks) is validated in a service before use; validation helpers live in `services/Validation.*`. | Review |
| 5.3 A multi-row change (mark + audit, submission + version) runs in one transaction; failure leaves the previous state intact. | Review + T_AUDIT_05, T_ARCH_08 |
| 5.4 `catch (...)` is allowed only to restore state and rethrow, or in the console prompt helpers that must survive bad input. | Review |

## 6. Security

| Rule | Check |
|---|---|
| 6.1 Passwords are never stored, logged or printed; only salted scrypt hashes via `PasswordHasher`. Hash comparison is constant-time. | Review + T_PASSWORD tests |
| 6.2 Authorisation re-reads stored account state on every call (role, active flag, department); a session object alone is not trusted. | T_ARCH_04, T_AUTHZ tests |
| 6.3 Server-generated values (timestamps, stored file names) are never taken from the user. | Review |

## 7. Comments and documentation

| Rule | Check |
|---|---|
| 7.1 Comments say *why*, cite the requirement or decision (`FR-14`, `OD-04`, `CR-03`) and are kept true when code changes. | Review |
| 7.2 A provisional rule is labelled provisional in the code where it is implemented. | Review |
| 7.3 Documentation that states a fact about the code (counts, coverage, file names) is updated in the same commit as the code. | Review |

## 8. Tests

| Rule | Check |
|---|---|
| 8.1 Every functional requirement has at least one automated test, traced in `docs/test/RTM.csv`. | [auto] `check_rtm.py` |
| 8.2 A defect fix adds a test that fails before the fix and passes after it. | Review (see F11 / T_ARCH_15) |
| 8.3 Tests are independent: each uses its own temporary database. | Review |
| 8.4 Boundary tables use equivalence partitions and boundary values (deadline equality, 1 s late, 3 days, 3 days + 1 s). | Review |

## 9. Allowed exceptions

A tool finding may be suppressed only inline (`// NOLINT(check)` or `// cppcheck-suppress id`) with a one-line reason next to it. Current suppressions: two `cppcheck-suppress constParameterCallback` in `src/main.cpp` (menu handlers share one function-pointer signature). Checks disabled in `.clang-tidy` are listed with reasons in `docs/qa/STATIC_ANALYSIS_REPORT.md`.
