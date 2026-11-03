# Static analysis report (Phase 6)

Brief requirement: "run a static analyser and address its findings". Four static tools and two dynamic checkers were used, because each family finds different defect classes and no single tool is complete. Raw output is in `docs/qa/evidence/`.

| Tool | Version | Scope | Findings at start | Findings now | CI gate |
|---|---|---|---|---|---|
| clang-tidy (`.clang-tidy`) | 18.1.3 | `src/` | 19 | **0** | Fails on any finding |
| cppcheck (warning, style, performance, portability) | 2.22 and 2.13 (Ubuntu 24.04, as in CI) local; 2.13 in CI | `src/` | 5 | **0** (2 justified inline suppressions) | Fails on any finding |
| GCC `-fanalyzer` | 13.3.0 | `src/` | 88 | 88, all one false-positive class (§3) | Not gated |
| Compiler warnings, `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` | GCC 13.3, Clang 18.1 | project targets | 0 | **0** | `-Werror` |
| ASan + UBSan + LSan (dynamic) | GCC 13.3 | all CTest targets (6 at Phase 6, 7 from v1.1) | 0 | **0** | Fails on any report |
| Valgrind memcheck (dynamic) | 3.22.0 | smoke, late-policy, integration, 28 unit tests | 0 | **0** | Not gated (slow) |

Baseline: at Phase 4 (commit `42a0b64`) neither clang-tidy nor cppcheck was available, and the 37 `-fanalyzer` warnings were recorded as "remaining under review". That open item is closed in §3.

## 1. clang-tidy

Enabled families: `bugprone-*`, `cert-*`, `clang-analyzer-*`, `performance-*` and selected `cppcoreguidelines-*`, `modernize-*`, `misc-*` and `readability-*` checks (see `.clang-tidy`). The first run over the refactored code (`evidence/clang-tidy-first-pass.txt`) found:

| Check | Count | Where | Action |
|---|---|---|---|
| `cppcoreguidelines-special-member-functions` | 11 | Repository interfaces, `IPenaltyStrategy`, `ITransaction` | Copy/move explicitly deleted on every polymorphic interface (rule of five; prevents slicing). Done with a small macro in `Repositories.hpp`. |
| `bugprone-macro-parentheses` | 4 | That macro | Rewritten so the argument is only ever used as a type name; finding cleared. |
| `bugprone-implicit-widening-of-multiplication-result` | 3 | `DeadlineEvaluator` epoch arithmetic | Operands widened to `std::int64_t` **before** multiplying. This is a real latent overflow class: `hour * 3600` in `int` is safe, but `days * 86400` is not for distant dates. |
| `performance-enum-size` | 1 | `Role` | Underlying type set to `std::uint8_t`. |

All 19 were fixed before the refactor was merged. The current run reports 0.

### Checks deliberately disabled

| Check | Reason |
|---|---|
| `bugprone-easily-swappable-parameters` | Fires on any two adjacent parameters of the same type. Enabled once for this report, it flagged 3 sites: `login(username, password)`, `submitFile(sourceFile, storageDirectory)` and the private helper `requireActiveTarget(const char*, const char*)` (two message strings). A swap in the first two would fail every login test (T_LOGIN_*) and file-submission test (T_FILE_*); a swap in the third only changes an error message. The fix the check wants (distinct strong types) would change public signatures for little defect-finding value at this size, so it is recorded as a known trade-off. |
| `cert-err58-cpp` | Flags static objects whose constructors can throw (e.g. `static const std::map` of menu handlers). They are built on first use inside `main`'s `try`, so an exception is still caught. |
| `bugprone-exception-escape` | Reports `main` because `std::cerr <<` can throw in theory. The program already catches `std::exception` at top level. |

## 2. cppcheck

Five style findings, all in `src/main.cpp` (`evidence/cppcheck-before.txt`): one `variableScope` (`value` in `askSecret` declared outside the `try` that uses it) and four `constParameterReference`/`constVariableReference` (display helpers and loops that only read). All five were fixed (code review items CR-01, CR-02).

Splitting `main()` (CR-03) created two `constParameterCallback` notes (named `constParameterReference` by cppcheck 2.13; both IDs are suppressed, see F16): `listAssignments` and `viewResults` only read the service, but every menu handler must share one function-pointer type. Making those two `const` would need a cast, which is worse than the finding. Both are suppressed inline with the reason written next to them, as the coding standard (§9) requires.

## 3. GCC `-fanalyzer`: 88 warnings, analysed rather than dismissed

All 88 are `-Wanalyzer-use-of-uninitialized-value` (CWE-457) and report `'<unknown>'` as the uninitialised object. 44 have no source location (`cc1plus`); the rest point at `return "Administrator";` style lines in `User.hpp`, string building in `main.cpp` and `PasswordHasher.cpp`, and calls that pass `std::string` temporaries. Earlier reports labelled these "probably false positives" without evidence. Three independent checks now support that label:

1. **Minimal reproduction.** `evidence/fanalyzer_false_positive_repro.cpp` is a two-line function, `std::string f(int r) { switch (r) { case 0: return "Administrator"; default: return "Student"; } }`, with no project code. GCC 13.3 `-fanalyzer` emits the same warning **twice** on it. The warning therefore comes from the analyzer's model of `std::string` construction (small-string buffer initialisation), not from this codebase.
2. **Dynamic evidence.** MemorySanitizer is not available with GCC, so Valgrind memcheck, which tracks the definedness of every byte at run time, was used on the real binaries. It reported 0 uses of uninitialised values across the smoke, late-policy and integration executables and 28 unit tests covering every flagged function (`evidence/valgrind-summary.txt`). ASan and UBSan are also clean on all six CTest targets.
3. **Tool status.** In GCC 13, `-fanalyzer` is designed and tested mainly for C; its C++ support (constructors, standard-library internals) is incomplete. That is consistent with every finding sitting on `std::string` construction.

Conclusion: the 88 warnings are classified as **false positives of one class**, with the evidence above. They are not gated in CI, because a gate would only be met by suppressing the analyzer entirely. The analyzer was still useful as a cross-check, and it will be re-run when the tool chain moves to a GCC with mature C++ support.

## 4. Residual risk

Static analysis cannot show the absence of logic errors in business rules. Those are covered by the boundary-value tests (`docs/test/TEST_PLAN.md`) and by the defect found in code review rather than by any tool: F11, `DeadlineEvaluator` accepting 30 February (see `CODE_REVIEW_RECORD.md`). No tool flagged F11, which is the main argument for keeping a human review step alongside the analysers.

## Reproduce

```sh
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p build --quiet --warnings-as-errors='*' $(git ls-files 'src/*.cpp')
cppcheck --enable=warning,style,performance,portability --std=c++17 --error-exitcode=1 --inline-suppr \
  --suppress=missingIncludeSystem -I src --project=build/compile_commands.json --file-filter='src/*' -q   # relative pattern: absolute fails on cppcheck 2.13 (F16)
for f in $(git ls-files 'src/*.cpp'); do g++ -std=c++17 -fanalyzer -Isrc -c $f -o /dev/null; done
g++ -std=c++17 -fanalyzer -c docs/qa/evidence/fanalyzer_false_positive_repro.cpp -o /dev/null
```
