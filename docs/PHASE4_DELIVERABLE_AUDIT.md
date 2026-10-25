# Phase 4 Deliverable Audit — Updated

## Status

This is a working Phase 4 implementation candidate, not a declaration that every requirement is complete or that Gate 2 has been approved. The formal Gate 2 status remains **NOT APPROVED**. The unresolved decisions listed in `GATE2_BLOCKERS.md` remain unresolved; the implementation does not treat provisional late-penalty assumptions as approved policy.

## Implemented and checked

| Area | Current state | Evidence / qualification |
|---|---|---|
| C++17/CMake application | Implemented | Clean Debug configure/build passed in the current workspace. |
| SQLite persistence | Implemented | Parameterised statements, RAII wrappers, transactions, foreign keys, busy timeout and WAL. |
| Authentication | Implemented | OpenSSL scrypt salted hashes, login verification and basic invalid-password test. |
| Four roles and scoped operations | Partially verified | Role/session checks and representative cross-role/department tests pass; complete permission matrix remains outstanding. |
| Core LMS workflow | Implemented incrementally | Users, departments, courses, lecturer assignment, enrolment, assignments, individual extensions, submission versions, marking, feedback, moderation and release. |
| Local submission file copy | Implemented with limitations | Existing regular file, 1-byte–20-MiB size and extension allowlist are checked; file is copied under a random server-generated name. Integration test confirms stored file exists. This is not a network upload service and does not inspect file content or scan for malware. |
| Late policy | Provisional | Helper and boundary tests pass for the documented assumption. OD-01–OD-04 are still unresolved. |
| Mark audit and history | Implemented for marking actions | `mark_audit` records actor, time, old/new final mark and reason in the same transaction as the mark change (forced-failure rollback tested). Moderation and release events are separate tables. No additional history table exists. |
| Console password entry | Implemented | Password echo is disabled on POSIX terminals and Windows uses `_getch`; target-platform manual verification is still needed. |
| Post-release correction | Deliberately blocked | Remains blocked pending unresolved OD-08; no unapproved correction workflow has been invented. |
| Class-wide extensions | Not in v1.0 | Deferred to CR1/v1.1 as required by the current project baseline. |
| Automated tests | 3/3 pass | Smoke, integration and late-policy tests passed after current changes. This does not prove full requirements coverage. |
| Build guide/package | Included in ZIP | Build guide is copied into the project root and linked from README. ZIP integrity and a fresh extracted build are to be checked before release. |
| Hosted Git/CI | Not evidenced | A CI workflow file is present; no hosted remote or successful hosted CI run is claimed. |

## Known gaps before calling Phase 4 complete

1. Suspected, unresolved: a resubmission after marking but before the deadline leaves the mark tied to the earlier version (needs a policy decision).
2. Add a role-checked file retrieval mechanism and harden the storage directory using operating-system permissions. File extension validation alone is not security validation.
3. Expand negative permission tests and cover resubmission deadline rules, beyond-window submissions, extension authorisation, invalid files and restart/persistence workflows.
4. Review GCC `-fanalyzer` warnings; see `STATIC_ANALYSIS_REPORT.md`. No claim of clean static analysis is made.
5. Resolve Gate 2 decisions OD-01–OD-08 through the approved process. In particular, do not enable post-release corrections until OD-08 is decided.
6. Perform a clean Release build and tests from the final extracted ZIP, then refresh this report with those exact results.
7. A hosted repository, hosted CI run, target-school performance test, accessibility/usability acceptance test and independent security review remain unverified.

## Current verification snapshot

- CMake Debug configure/build: passed.
- CTest: 3/3 passed (smoke, integration, late policy).
- Integration coverage includes file copy, mark-history snapshots, authentication, representative role checks, individual extension display/use, moderation/release and transaction rollback on audit failure.
- These results are from the current local build and are not a substitute for target deployment acceptance testing.
