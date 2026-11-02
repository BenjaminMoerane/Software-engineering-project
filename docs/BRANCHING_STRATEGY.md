# Branching, commit and release strategy

Repository: https://github.com/BenjaminMoerane/Software-engineering-project

## 1. Model

A simplified **GitHub Flow with release tags**, chosen for a single developer on a Waterfall schedule:

- `main` is always buildable and always passes CI. Nobody commits directly to `main`.
- All work is done on short-lived branches and merged with `--no-ff` (or a GitHub pull request), so the merge commit records which phase or fix the work belonged to.
- Each phase gate and release is an **annotated tag** on `main`.

Rejected alternative: **Git Flow** (separate `develop`, `release/*` and `hotfix/*` branches). It suits teams shipping parallel versions, but here there is one developer and one supported release at a time, so the extra long-lived branch would add merge overhead and a second place for `main` to drift without any benefit. Trunk-based development without branches was also rejected, because the brief asks for reviewable feature branches and the phase history has to be visible in the repository.

## 2. Branch names

| Prefix | Use | Example in this repository |
|---|---|---|
| `feature/` | New behaviour or a phase deliverable | `feature/phase5-verification`, `feature/cr1-class-extensions` |
| `fix/` | A logged defect (`F<n>`) | `fix/f9-hod-moderation-queue`, `fix/f10-mark-message-order` |
| `refactor/` | Structure change with no behaviour change | `refactor/repository-layer` |
| `qa/` | Quality-assurance changes (analysis findings, CI, review) | `qa/phase6-config-qa` |
| `release/` | Release preparation (docs, version bump) | `release/v1.0` |

A fix found while a feature branch is open is branched from that feature branch and merged back into it (F9 and F10 were merged into `feature/phase5-verification` this way, then the whole phase was merged into `main`).

## 3. Commits

- One logical change per commit; the build and tests pass at every commit on `main`.
- Subject line: imperative, at most about 72 characters, prefixed with the area or phase (`Phase 5: …`, `F9: …`, `Refactor: …`).
- The body explains **why** and cites requirement, defect or decision IDs (`FR-10`, `F11`, `OD-08`, `CR1`), and states the test evidence for behaviour changes.
- Formatting-only changes are separate commits (`8be1fa9`, `f9cad4c`) so reviewers can skip them.

## 4. Merge rule (definition of done)

A branch may be merged into `main` only when:

1. All CI jobs pass: GCC and Clang in Debug and Release with warnings as errors, sanitizers, the coverage gate (business-logic layer ≥ 70% line coverage), clang-tidy, cppcheck and the formatting check (`.github/workflows/ci.yml`).
2. The self-review checklist in `docs/qa/CODE_REVIEW_RECORD.md` has been applied and any finding recorded.
3. `docs/test/RTM.csv` and any document that states facts about changed code are updated in the same branch.

On GitHub, enable branch protection on `main`: *Require a pull request before merging* and *Require status checks to pass* (select the four CI jobs). This turns rule 1 into an enforced gate rather than a convention.

## 5. Tags and releases

Annotated tags (`git tag -a`) mark gates and releases; tags are never moved or deleted once pushed.

| Tag | Meaning |
|---|---|
| `phase4-implementation` | End of Phase 4 (already on GitHub) |
| `phase5-verification` | Gate 3 evidence: test plan, suites, coverage, defect log |
| `phase6-config-qa` | Configuration management and QA deliverables |
| `v1.0` | First release (Phase 7): deployment diagram, installation guide, user manuals, release notes |
| `v1.1` | Maintenance release for CR1 (Phase 8) with regression results |

Versioning follows Semantic Versioning: v1.1 adds backwards-compatible functionality (class-wide extensions); a schema change is shipped with a migration step in the release notes.

## 6. Pushing this history

```sh
git push origin --all      # main and every feature/fix/refactor/qa/release branch
git push origin --tags     # annotated tags
```

Branches stay on the remote after merging as evidence of the workflow for assessment; they can be deleted after marking.
