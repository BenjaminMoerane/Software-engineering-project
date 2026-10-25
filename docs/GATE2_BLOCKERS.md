# Gate 2 blockers and provisional implementation decisions

**Formal status: NOT APPROVED.** The Phase 3 validation report records Gate 2 as not passed and OD-01–OD-08 as pending/provisional. Coding and automated tests in this package do not constitute formal design approval.

| Decision | Affected behavior | Current implementation | Approval still required |
|---|---|---|---|
| OD-01 | Meaning of a late day | Provisional elapsed 24-hour periods, rounded up | Confirm calendar day vs rolling 24-hour period and exact boundaries |
| OD-02 | Late penalty arithmetic | Provisional deduction of 10% of assignment maximum per late day, up to three days; beyond-window mark is zero | HoD/supervisor to confirm interpretation of “10% of available mark” |
| OD-03 | Rounding | No explicit rounding; uses floating-point values and clamps below zero | Approve whether and when marks/penalties are rounded and to how many decimals |
| OD-04 | Deadline boundary/time zone | Timestamps are stored/compared as UTC ISO-8601 strings; system generates submission time | Confirm UTC/local school time and exact-at-deadline behavior |
| OD-05 | Work beyond three-day window | Submission version is retained and marked `BeyondWindow`; mark policy gives zero | Confirm retention/refusal behavior and user-facing status |
| OD-06 | Performance/concurrency assumptions | No compliance claim | Agree target hardware, concurrency and benchmark workload |
| OD-07 | Extension deadline semantics | Individual revised deadline replaces original deadline for that student | Confirm precedence and interactions with late calculations |
| OD-08 | Corrections after release | Marking and moderation of a released mark are blocked | Approve correction, audit, moderation and re-release workflow before enabling it |

## Gate 2 review checklist

- [ ] Supervisor/HoD formally reviews and approves the Phase 3 SDD, diagrams, schema, data dictionary and traceability matrix.
- [ ] OD-01–OD-08 are resolved, explicitly deferred, or approved in writing by the responsible reviewer.
- [ ] All Phase 2 requirement priorities and acceptance criteria are reconciled with source files and tests.
- [ ] The reviewer accepts the implementation's mark/audit treatment against the schema's uniqueness constraints.
- [ ] Performance hardware/workload and non-functional test conditions are defined.
- [ ] Approval date, reviewer, version and conditions are recorded.

This register is not approval evidence.
