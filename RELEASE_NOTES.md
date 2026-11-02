# Release notes

## v1.1.1 — patch release (corrective)

- **F14 fixed:** on Linux, the `lms-start.sh` launcher failed with `lms_app: not found` when it was started by a relative path (for example `bin/lms-start.sh`). It now works from any path and always opens `<data folder>/school_lms.sqlite3`. Found while verifying a fresh install of the v1.1 package. New system test `ST_DEP_01`.
- No database or behaviour change; upgrade by installing over v1.1 (or v1.0).

---

## v1.1.0 — maintenance release for change request CR1 (Phase 8)

**Perfective maintenance:** class-wide extensions. Analysis: `docs/maintenance/CR1_IMPACT_ANALYSIS.md`; design updates: `docs/maintenance/CR1_DESIGN_UPDATES.md`; regression evidence: `docs/maintenance/REGRESSION_RESULTS.md`.

### New

- **Class-wide extensions (FR-23).** Lecturer menu option `18` extends an assignment's deadline for every student enrolled in the course, with a reason. A class extension can be extended again but never brought forward; every change is kept with who, when and why.
- **Shown on the assignment list (FR-21 extended).** Students and lecturers see `class extension <date>` next to the deadline, as well as `your extension <date>` for an individual extension.
- **Applicable deadline (BR-11).** Submission timeliness and resubmission use the latest of the original deadline, the class-wide extension and the student's individual extension.

### Provisional (awaiting HoD/supervisor approval)

CR1-D1 the class means every enrolled student, including later enrolments · CR1-D2 the latest deadline wins · CR1-D3 class extensions only move later · CR1-D4 work already submitted keeps the lateness recorded at submission. Each is isolated in one place in the code.

### Upgrade from v1.0

Back up the data folder, install v1.1 over v1.0, start as normal. **No manual database step:** on first start v1.1 adds the `class_extensions` table to a v1.0 database and changes nothing else (verified by T_CR1_09 on a real v1.0-schema database). `lms_app --version` prints `1.1.0`.

### Quality

165 GoogleTest tests (+10) and 30 system tests (+2). The complete v1.0 suite passes unchanged on GCC and Clang, Debug and Release (`-Werror`), and under ASan/UBSan. Business-logic coverage is 99.1% lines / 91.5% branches; clang-tidy and cppcheck report 0 findings; mutation check of the precedence rule 4/4. Traceability is now checked by CTest (`lms_rtm_traceability`).

### Fixed

F13: the traceability checker silently ignored test IDs containing digits (e.g. `T_CR1_01`). Documentation: F12 (fixed in v1.0) added to the defect log.

### Known limitations

Unchanged from v1.0 (below), except that class-wide extensions are now available.

---

## v1.0.0 — first release (Phase 7)

**Moderated Learning Management System**: an offline, single-computer console application in C++17 with SQLite, for a secondary school with four roles: Administrator, Lecturer, Head of Department and Student.

Install: `docs/deployment/INSTALLATION_GUIDE.md`. Users: `docs/deployment/USER_MANUAL_*.md`.

### What is included

| Area | Requirements | Summary |
|---|---|---|
| Accounts and security | FR-01–FR-03, NFR-04, NFR-05 | Login with salted scrypt password hashes (OpenSSL), constant-time comparison; four polymorphic role types; every operation re-checks role, active state and department against the database. |
| Administration | FR-03–FR-05 | Departments, users, courses, lecturer assignment, enrolment; update and deactivate. |
| Assignments and extensions | FR-06–FR-08, FR-21 | Lecturers create assignments for their own courses and grant individual extensions; students see their extension in the assignment list. |
| Submission | FR-09, FR-10 | File submission (PDF, DOC/DOCX, TXT, ZIP, PNG, JPG/JPEG; 1 B–20 MiB) copied under a random name; server-generated UTC timestamps; resubmission before the applicable deadline. |
| Marking and late penalty | FR-11–FR-14 | Raw mark, penalty and final mark stored separately; penalty computed once by `IPenaltyStrategy`; every mark change audited (who, when, old, new, reason) in the same transaction. |
| Moderation and release | FR-15–FR-18 | Department moderation queue; approve or return; release only after the latest decision is an approval; students see only their own released results. |
| Persistence and robustness | FR-19, FR-20, NFR-03, NFR-06 | SQLite (WAL, foreign keys, parameterised SQL through a repository layer); invalid input and database errors never crash the program; works with no network. |

### Quality evidence for this release

- 155 GoogleTest unit/integration tests and 28 system/acceptance tests; all pass on GCC 13 and Clang 18, Debug and Release, with warnings treated as errors (1 test skips when run as root).
- Business-logic coverage: 99.2% of lines, 90.6% of branches (`docs/test/COVERAGE_REPORT.md`).
- clang-tidy and cppcheck: 0 findings; ASan/UBSan and Valgrind: 0 errors (`docs/qa/`).
- Every user-manual flow was run on the installed binary (`docs/deployment/evidence/`).

### Defects fixed since Phase 4 (`docs/test/DEFECT_LOG.md`)

F9 HoD moderation queue missing (High) · F10 misleading partial message after an invalid mark · F11 impossible calendar dates accepted by the deadline parser (latent) · **F12 an installed program could not start without the source tree.** The schema is now built into the program.

### Provisional business rules (not yet approved at Gate 2)

These are implemented as specified in the Phase 2/3 documents but **are not stakeholder-approved policy**:

- Late penalty: each started 24-hour period after the applicable deadline is one late day; 10% of the maximum mark per day, at most 3 days; more than 3 days late gives 0; the final mark is never below 0 (OD-01–OD-04). *Note:* the SDD's provisional wording favours school-local calendar days; the code counts 24-hour periods from the UTC deadline. This difference is open for the HoD's decision.
- An individual extension replaces the deadline for that student (OD-07).
- Corrections after release are blocked (OD-08).

### Known limitations

- **No password reset.** A forgotten password can only be handled by deactivating the account and creating a new one.
- No list screens for users or courses; administrators must keep a record of IDs.
- Resubmission after marking but before the deadline leaves the earlier mark attached to the old version (O-6, needs a policy decision).
- Single computer, one user at a time; not for a database on a network share.
- File type is checked by extension only; there is no content inspection or malware scanning.
- Windows build and the Windows masked-password input are not verified; target-hardware performance, technician install time (NFR-10) and usability with real users (NFR-08) are not verified.
- Class-wide extensions are not in v1.0 (change request CR1, planned for v1.1).

### Upgrade notes

First release: no upgrade path needed.
