# Risk register (Phase 1, updated at project close)

Scale: likelihood (L) and impact (I) from 1 (low) to 5 (high); exposure = L × I (maximum 25). Initial scores are from the Phase 1 project management plan. Closing scores (3 November 2026, after release v1.1.2) are the author's assessment from the evidence named, **proposed for supervisor review**. Owner: the student, who holds every project role, unless stated.

**Limitation.** The brief asks for the register to be updated at each phase gate. Only the Phase 1 scores and these closing scores were recorded; the intermediate Gate 1–3 snapshots were not kept, so the materialisation dates below come from the defect log and Git history, not from a dated register.

| ID | Category | Risk (cause → consequence) | Initial L×I | Closing L×I | Mitigation | Evidence and status |
|---|---|---|---|---|---|---|
| R1 | Project | Stakeholder conflicts unresolved → rework after design | 3×5 = 15 | 3×4 = 12 | Resolve at Gate 1; isolate proposals in code | OD-01..OD-08 and CR1-D1..D4 still provisional (`docs/GATE2_BLOCKERS.md`). **Open** |
| R2 | People | One person performs all roles to a fixed deadline → testing time compressed | 4×5 = 20 | 3×4 = 12 | Prioritise Must requirements; protect the gates | All eight phases delivered; report and viva remain. **Open** |
| R3 | Technical | Class diagram and ERD drift apart → persistence rework | 3×5 = 15 | 1×4 = 4 | Shared names; RTM with class and table columns | RTM checked by CTest on every build. **Mitigated** (column-level schema comparison not run) |
| R4 | Technical | Security left late → exposure of learner data | 2×5 = 10 | 1×5 = 5 | Salted hashing; central authorisation; negative tests | scrypt hashes; authorisation on every call; 54-denial matrix (T_AUTHZ_01). **Mitigated**; no independent security review |
| R5 | Technical | Deadline, extension, resubmission and penalty rules interact → wrong marks | 3×5 = 15 | 2×5 = 10 | Decision rules; boundary-value tests | One resolver; mutation-tested precedence (4/4); calendar-day vs 24-hour rule (OD-01) and O-6 open. **Partly mitigated** |
| R6 | Technical | SQL mixed into business logic → inconsistent records, design mismatch | 3×4 = 12 | 1×4 = 4 | Repository layer; parameterised SQL | **Materialised** in Phase 4 (O-3); refactored; CTest `lms_architecture_sql_isolation` prevents recurrence. **Closed** |
| R7 | Project | Coding consumes test time → Gate 3 failure | 3×5 = 15 | 1×5 = 5 | Tests written during implementation; ≥ 70% coverage | 165 GoogleTest + 31 system tests; 99.2% line coverage. **Closed** |
| R8 | Project | CI and static analysis postponed → missing evidence | 2×3 = 6 | 1×3 = 3 | CI on every push | **Materialised**: hosted CI first ran after Phase 4; runs #5–#21 red (F15, F16). Green 7/7 from run #22; `main` protected with 7 required checks since 3 Nov 2026 (PR #1). **Closed** |
| R9 | Technical | Deployment environment differs → v1.0 cannot be demonstrated | 3×4 = 12 | 2×4 = 8 | Clean-install test before Week 6 | **Materialised** twice (F12, F14), both fixed; Windows build not verified. **Partly mitigated** |
| R10 | Technical | CR1 affects many records → v1.1 breaks individual extensions | 3×4 = 12 | 1×4 = 4 | Impact analysis before coding; full regression | v1.0 suite unchanged and passing; real v1.0 database upgraded. **Closed** |
| R11 (new) | Project | Provisional rules reach real users before approval | — | 3×4 = 12 | Label provisional in code, manuals, release notes | Gate 2 NOT APPROVED. **Open**; owner: supervisor/HoD for the decisions |
| R12 (new) | Technical | File-level access bypasses the application on the shared computer | — | 2×4 = 8 | Staff-only data folder; backups | Database not encrypted at rest. **Open**; owner: school IT officer at deployment |
| R13 (new) | People | Author cannot explain code produced with tool assistance → viva marks withheld | — | 2×5 = 10 | Study key functions before the viva; declare AI assistance | Viva not yet held. **Open** |
