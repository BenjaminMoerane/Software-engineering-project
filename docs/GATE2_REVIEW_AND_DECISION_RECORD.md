# Gate 2 review and decision record

**Current formal status: NOT APPROVED / approval evidence not present in the supplied project files.**

The assignment brief states that implementation must not begin until design has passed Gate 2. Existing Phase 3 validation records Gate 2 as not approved and OD-01–OD-08 as pending/provisional. Automated tests and code changes cannot substitute for the supervisor/HoD's formal decision. This record is prepared to support that review; it does not represent approval.

## Materials the reviewer should inspect

- Phase 3 Software Design Document and its architecture/design rationale.
- Component, class, state-machine, activity and sequence diagrams.
- ERD, data dictionary and `CS4433_LMS_schema.sql`.
- Phase 2 requirements and use-case specifications.
- Phase 3 validation report and traceability matrix.
- Phase 4 `docs/IMPLEMENTATION_TRACEABILITY.csv`, `docs/GATE2_BLOCKERS.md`, `docs/PHASE4_DELIVERABLE_AUDIT.md`, and `docs/STATIC_ANALYSIS_REPORT.md`.

## Decisions requiring written confirmation

| ID | Decision to record | Required reviewer response |
|---|---|---|
| OD-01 | Late day means calendar day or elapsed 24-hour period; exact boundaries | Approved interpretation / defer with owner and due date |
| OD-02 | Whether the 10% deduction is based on maximum available mark or raw earned mark | Approved formula / defer with owner and due date |
| OD-03 | Mark and penalty rounding precision and stage | Approved rounding rule / defer with owner and due date |
| OD-04 | Authoritative timezone and exactly-at-deadline behavior | Approved timezone and boundary rule / defer with owner and due date |
| OD-05 | Whether submissions beyond three days are retained or refused; recorded status and displayed outcome | Approved behavior / defer with owner and due date |
| OD-06 | Benchmark hardware, workload, concurrency and measurement method | Approved test environment and thresholds / defer with owner and due date |
| OD-07 | Whether an individual extension replaces or otherwise modifies the original deadline and penalty calculation | Approved precedence rule / defer with owner and due date |
| OD-08 | Correction, audit, moderation and re-release process after result release | Approved correction workflow / defer with owner and due date |

## Reviewer decision

- [ ] Gate 2 approved.
- [ ] Gate 2 conditionally approved, with conditions listed below.
- [ ] Gate 2 not approved; required revisions listed below.

**Design version reviewed:** ____________________

**Decision date:** ____________________

**Reviewer name and role:** ____________________

**Conditions / required changes / deferred decisions:**

____________________________________________________________________

____________________________________________________________________

**Reviewer confirmation/signature or institutional approval reference:**

____________________________________________________________________

## Implementation boundary

Until the reviewer supplies a dated approval or conditional-approval record, this project must continue to report Gate 2 as not approved. Do not present provisional late-penalty, deadline, beyond-window, extension, or post-release-correction behavior as formally accepted policy. Any change that depends on these decisions should remain blocked or clearly provisional.
