# Static analysis report

- `cppcheck`: unavailable in the execution environment; not run.
- `clang-tidy`: unavailable in the execution environment; not run.
- GCC `-fanalyzer`: the full project compiled successfully with the flag. The analyzer emitted 37 warnings, all reported as `-Wanalyzer-use-of-uninitialized-value`. Locations include string/vector allocation in `PasswordHasher.cpp`, SQLite text conversion in `Database.cpp`, role conversion in `User.hpp`, input parsing in `main.cpp`, and the captured test clock lambda in `integration_tests.cpp`.

Several warnings appear to originate from GCC analyzer reasoning through standard-library allocation/string internals, where objects are initialized by their constructors, but the report does not assert that all warnings are false positives. They remain findings for review with another analyzer/compiler and a human code review. The normal warning-enabled build and AddressSanitizer/UndefinedBehaviorSanitizer test build passed.

No static-analysis-clean claim is made.
