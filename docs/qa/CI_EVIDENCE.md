# Continuous integration evidence

Workflow: `.github/workflows/ci.yml` ("C++ LMS CI"). Trigger: every `push` to any branch and every `pull_request`.

## Jobs

| Job | What it proves | Fails when |
|---|---|---|
| `build-and-test` (matrix: GCC, Clang × Debug, Release) | Portable build; all 6 CTest targets: smoke, integration, late-policy, GoogleTest unit/integration, Python system/acceptance, SQL-isolation rule | Any compiler warning (`-Werror`), build error or test failure |
| `sanitizers` | No memory errors, leaks or undefined behaviour on any test path | Any ASan, UBSan or LSan report |
| `coverage` | Business-logic layer line coverage meets the brief's 70% | Coverage < 70% (`gcovr --fail-under-line 70`); uploads text and HTML reports |
| `static-analysis` | Formatting, clang-tidy and cppcheck clean; publishes lizard metrics | Any formatting difference or analyser finding |

The GitHub runner is a normal (non-root) user, so `T_FILE_11` (unreadable submission file), which is skipped in root containers, actually runs there.

## Hosted run history

| Commit | Run | Result | Source |
|---|---|---|---|
| `42a0b64` (Phase 4, original single-job workflow) | [41441950587](https://github.com/BenjaminMoerane/Software-engineering-project/actions/runs/41441950587) | Success, about 20 s, job `build-and-test` | GitHub Actions page |
| `6bfc2fb` `main` (Phase 5; still the single-job workflow) | C++ LMS CI #4 | Success, 2 min 8 s | GitHub Actions page, 3 Nov 2026 |
| `6bfc2fb` tag `phase5-verification` | #9 | Success, 1 min 59 s | same |
| `2f353ab` `refactor/repository-layer` | #8 | Success, 2 min 22 s | same |
| `daeab08` `feature/phase5-verification` | #7 | Success, 2 min 7 s | same |
| `469b7bb` `fix/f9-hod-moderation-queue` | #6 | **Failure**: system-test script not yet in that commit (defect F15) | same |
| `fec4c33` `fix/f10-mark-message-order` | #5 | **Failure**: same cause (F15) | same |
| Phase 6–8 commits (four-job workflow) | *to be recorded after push* | — | — |

**After pushing**, open the Actions tab, confirm all four jobs are green for the commits tagged `phase6-config-qa`, `v1.0` and `v1.1`, take a screenshot for the report appendix, and add the run IDs to the table above. Until then, the four-job workflow has been verified by running each job's exact commands locally (GCC 13.3 / Clang 18.1 on Ubuntu 24.04, the same image family as `ubuntu-24.04`), not by a hosted run. The two places where the hosted run can still differ are listed so a failure there is quick to diagnose:

1. Ubuntu's packaged `cppcheck` is older (2.13) than the version used locally (2.22) and may report a slightly different set of style findings.
2. GoogleTest is fetched from GitHub at configure time (`FetchContent`, pinned v1.14.0).
