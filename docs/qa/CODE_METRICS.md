# Code metrics (Phase 6)

Tool: `lizard` 1.25.0 (NLOC = non-comment lines of code; CCN = McCabe cyclomatic complexity, i.e. the number of linearly independent paths through a function, equal to decision points + 1). Thresholds: CCN > 10 (McCabe's original recommended limit) or NLOC > 60 per function. Full output: `evidence/lizard-full.txt`.

## 1. Evolution across the three QA points

| Measure (`src/` only) | Phase 5 baseline `1965a3a` | After repository refactor `6bfc2fb` | After code review `f9cad4c` |
|---|---|---|---|
| Total NLOC | 1,382 | 2,186 | 2,220 |
| Functions | 94 | 190 | 211 |
| Mean NLOC per function | 12.0 | 7.6 | **6.9** |
| Mean CCN per function | 3.5 | 2.4 | **2.2** |
| Functions over a threshold | 4 | 4 | **3** |
| Largest function | `main`, 193 NLOC / CCN 31 | `main`, 191 / 31 | `askSecret`, 47 / 17 |
| `markSubmission` | 90 NLOC / CCN 11 | 34 / 8 | 34 / 8 |

The refactor added about 800 NLOC, mostly interface declarations, repository mapping code and per-use-case services. The SDD named this cost in advance ("additional mapping code and a risk of over-abstraction"). In return, mean function size fell by 43% and mean CCN by 37%: the logic is no longer in fewer, larger functions, but spread across small units that can be tested in isolation (T_ARCH tests use in-memory fakes).

## 2. Size by layer (current)

| Layer | Files | NLOC | Functions | Mean CCN |
|---|---|---|---|---|
| Application services + policy (`src/services`) | 14 | 816 | 81 | 3.3 |
| Data access (`src/data`, `src/repositories`) | 5 | 814 | 76 | 1.6 |
| Domain (`src/domain`) | 2 | 187 | 21 | 1.6 |
| Security adapter (`src/security`) | 2 | 86 | 6 | 3.7 |
| Presentation (`src/main.cpp`) | 1 | 317 | 27 | 3.1 |
| **Production total** | **24** | **2,220** | **211** | **2.2** |
| Tests (`tests/`, C++ and Python) | 19 | 2,576 | 263 | 1.4 |

The test-to-production NLOC ratio is about 1.16 : 1. Mean CCN for services is higher than the overall mean because the two date parsers (§3) are there.

## 3. Functions over threshold (current) and decision

| Function | NLOC | CCN | Decision |
|---|---|---|---|
| `validation::isoUtc` (`Validation.cpp`) | 17 | 25 | **Accepted.** CCN comes from a flat sequence of independent character and range checks (`v[4] != '-' || …`) with one throw each, not from nested logic. Splitting it would scatter one format rule across several functions. It is covered by equivalence-class tests (T_ASG_04 deadline format and calendar validation). |
| `DeadlineEvaluator::toEpochSeconds` | 18 | 23 | **Accepted**, same reason: a linear parser (tests T_ARCH_10..13, T_ARCH_15). Code review found a real defect here (F11) and fixed it, which shows the high CCN was hiding a *missing* check rather than excess complexity. |
| `askSecret` (`main.cpp`) | 47 | 17 | **Accepted.** About half the CCN comes from the Windows (`_getch`) and POSIX (`termios`) branches of one `#ifdef`; only one branch is compiled on each platform, so the complexity a reader faces on one platform is roughly half the reported figure. |
| `main` | — | — | **Fixed (CR-03).** It was 193 NLOC / CCN 31. It is now a short loop calling `firstRunSetup` and `runSession`, with one handler per menu option, and no longer appears in the list. |

## 4. Interpretation

CCN measures the number of paths, and so the minimum number of tests for full path coverage; it does not measure how hard code is to understand. That is why the two parsers are accepted at CCN 23–25 while `main` was not: `main` combined many unrelated responsibilities, which is the actual maintainability problem CCN usually signals. The metric is used here as a pointer for review, not as a pass/fail rule, so CI prints the over-threshold list but does not fail on it.
