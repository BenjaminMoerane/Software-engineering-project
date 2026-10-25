# Phase 2 requirements reconciliation — implementation review

## Source basis

This review uses the available consolidated Phase 2 Requirements Engineering document, `CS4433_Phase_2_Requirements_Engineering_Complete.docx`, alongside the official assignment brief, Phase 3 Software Design Document, Phase 3 validation report, schema, and current Phase 4 source/tests. The uploaded `Phase 2.rar` archive was not extracted in this environment; the consolidated Phase 2 DOCX already present in the project files was used instead. If the archive contains different or newer versions of the SRS, stakeholder analysis, or use-case model, those must be compared before final sign-off.

## Requirements that are explicit in the Phase 2 source

- The initial system is an offline C++17/SQLite application for a school computer or local network.
- The four application roles are Administrator, Lecturer, Head of Department (HoD), and Student. A student representative is a stakeholder, not a fifth application role.
- Administrator responsibilities include account, department, course, lecturer assignment, and enrolment management.
- Lecturer responsibilities include assignment creation, individual extensions, marking, and written feedback.
- Students may resubmit before the applicable deadline.
- Late work loses 10% of the available mark per late day for up to three days; after that it receives zero. The source explicitly says the timing and arithmetic interpretation require validation, so the exact operational formula is not settled by the source alone.
- HoD moderation and release are separate actions. Approval alone must not expose a result to students.
- Mark changes require actor, time, old mark, new mark, and reason; audit failure must not leave an unaudited mark update.
- Passwords must not be stored in plaintext; salted hashes are required.
- Class-wide extensions are CR1/v1.1, not v1.0.
- The assignment's process constraint prohibits implementation before design passes Gate 2. The supplied Phase 3 validation report records that gate as not approved.

## Source-derived use-case requirements to retain in testing

### UC-01 — Submit assignment

Test enrolment checks; the applicable deadline and individual extension display; on-time, late and exact-deadline behavior; permitted resubmission and version history; invalid/missing content; beyond-window behavior once decided; and transaction failure without false success or partial data.

### UC-02 — Mark submission and provide feedback

Test assigned-course access; mark range including zero and maximum; written feedback; hidden results before release; mark correction reason; and atomic rollback when the audit write fails.

### UC-03 — Moderate and release marks

Test department scope; approve versus return; returned-mark reason; approval not automatically releasing results; release validation; student-only result access; and state consistency when release fails or the mark changes after review.

## Current implementation review

The source implements meaningful core paths for authentication, role checks, basic administration, course/lecturer/enrolment setup, assignments, submissions/version records, marking/audit transactions, moderation, explicit release, and released-result access. The latest clean Debug build passed all three CTest targets (smoke, integration, late-policy helper tests).

That evidence does **not** prove all Phase 2 requirements are complete. The late-policy tests exercise the policy helper and do not resolve its approval status or fully test the end-to-end submission workflow. Remaining service-level tests include extension authorisation and display, cross-course access, deadline-boundary/resubmission behavior, invalid marks across all paths, multi-student result isolation, complete role/action denial coverage, and persistence of a full workflow across restart. NFR performance, reliability, usability, installation, and target-hardware evidence also remain incomplete.

## Gate 2 status

**NOT APPROVED / approval evidence not present.** Neither code, passing tests, this reconciliation, nor this document constitutes supervisor approval. The supervisor/HoD must review the actual Phase 3 design artifacts and resolve or formally defer OD-01–OD-08 with a dated decision record. Until then, all related policy behavior must remain labelled provisional.

## Archive caveat

The separate uploaded RAR archive was not unpacked here. This reconciliation is grounded in the consolidated Phase 2 DOCX already available in the project workspace. Before final submission, confirm whether the RAR contains newer or different individual documents and reconcile any differences explicitly.
