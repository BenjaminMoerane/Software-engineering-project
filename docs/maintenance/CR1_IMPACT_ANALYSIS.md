# CR1 — Change request analysis (Phase 8)

| Field | Value |
|---|---|
| Change request | **CR1**: "A lecturer should be able to extend the deadline for a whole class, not just an individual student, and extensions should be shown on the student's assignment list." |
| Raised by | The school (after release v1.0) |
| Baseline | Tag `v1.0` (commit `063b7e9`); 154 + 1 GoogleTest tests, 28 system tests, all passing |
| Target | Release `v1.1.0` |
| Branch | `feature/cr1-class-extensions` |
| Status | Analysed → implemented → regression-tested (§7) |

## 1. Classification

**Perfective maintenance** (Valacich & George's four types, which match ISO/IEC/IEEE 14764: corrective, adaptive, perfective, preventive).

| Type | Applies? | Reason |
|---|---|---|
| Corrective | No | v1.0 is not faulty here. BR-10 and constraint C-10 deliberately excluded class-wide extensions from v1.0, so v1.0 behaves as specified. |
| Adaptive | No | Nothing in the environment (OS, hardware, law, interfacing system) changed. |
| **Perfective** | **Yes** | The users ask for new functionality: a new kind of extension, plus its display, to make the existing extension process less laborious. Without CR1 a lecturer extending a class of 40 would make 40 separate entries, each a chance to miss a student. |
| Preventive | No | The change is not being made to reduce future maintenance cost, although the deadline resolver it introduces (§4) has that side-benefit. |

The second half of CR1 ("shown on the student's assignment list") is partly met in v1.0 for individual extensions (FR-21, `your extension …`). CR1 extends that display to class-wide extensions.

## 2. Decisions this change needs

The Phase 3 SDD (§7) states that the precedence rule "should be documented in the change record and approved before implementation". The rules below are therefore **provisional (CR1-D1 … D4), awaiting HoD/supervisor approval**, in the same way as OD-01 … OD-08. Each one is isolated in a single place in the code, so changing it later is a one-function change.

| ID | Question | Decision | Rationale | Rejected alternatives |
|---|---|---|---|---|
| CR1-D1 | Who is "the whole class"? | Every student enrolled in the assignment's course, **including students enrolled after the extension is granted**. | Matches how lecturers think of a class; computed at use time, so there is no per-student copy to fall out of date. | Copying one individual extension row per enrolled student: N rows per change, late enrollers missed, and an individual extension a student already had would be silently overwritten by the copy. |
| CR1-D2 | **Precedence** when a student has both an individual and a class-wide extension | **The latest deadline applies**: `applicable = max(original, class-wide, individual)`. | Fairness: no action by a lecturer should take time away from a student who was already given it. If an individual extension (e.g. for illness) were overridden by an earlier class-wide date, that student would lose time; if a class extension were ignored because a student had an earlier individual one, that student would have *less* time than the rest of the class. | (a) Individual always overrides class: penalises students with an earlier individual extension. (b) Class always overrides individual: can shorten a medical extension. |
| CR1-D3 | Can a class-wide extension be changed? | Yes, but **only later**: a new class-wide deadline must be after the original deadline and after the class-wide deadline currently in force. Every change is kept (append-only history: who, when, new deadline, reason). | Students may already have planned around the extension; moving it earlier is unfair and could make on-time work late. Keeping history gives the audit trail the SDD asks for when one action affects many students. | Overwrite in place: no history. Allow shortening: fairness problem above. |
| CR1-D4 | What happens to work already submitted when a class-wide extension is granted? | **Nothing.** Each submission version keeps the deadline and lateness recorded at submission time (existing v1.0 snapshot design). | The SDD requires "existing submission versions retain the deadline snapshot used at submission". Changing recorded lateness after the fact would change marks already moderated or released. A lecturer who wants to waive lateness for one student does so with an individual extension *before* the student submits. | Re-classifying existing submissions retroactively: rewrites assessment history. |

## 3. Requirements affected

| ID | v1.0 text (summary) | Impact |
|---|---|---|
| **BR-10** | "Class-wide extensions are CR1/v1.1, not the initial release." | **Satisfied/retired** by this release. |
| **FR-23 (new)** | — | *A lecturer shall grant a class-wide extension (revised deadline and reason) for an assignment in a course they are assigned to; it applies to every student enrolled in that course, and every change is recorded with the authorising lecturer and time.* MoSCoW: **Must** (it is the change request). Verifiable: T_CR1_01, 04, 05, 06. |
| **BR-11 (new)** | — | Applicable deadline = latest of original, class-wide and individual (CR1-D2); class-wide may only move later (CR1-D3). |
| FR-21 | Display individual revised deadline to the affected student | **Extended**: the assignment list also shows any class-wide extension (`class extension …`). Priority stays Should. |
| FR-09 | Record submission timestamp against the *applicable* deadline | **Behaviour change**: the applicable deadline now also considers the class-wide extension (BR-11). |
| FR-10 | Resubmission before the deadline | **Behaviour change** by the same resolver: resubmission is allowed until the applicable (possibly class-extended) deadline. |
| FR-07, FR-08, FR-11 | Individual extension; role-scoped list; late penalty | **Unchanged**; regression only. FR-11 still receives lateness from the snapshot. |
| FR-02, NFR-05 | Role-based authorisation | New operation must deny Student, HoD and Administrator, and Lecturers not assigned to the course (T_CR1_06). |
| FR-19 | Persistence | New table; existing v1.0 databases must migrate without data loss (T_CR1_09). |

## 4. Design artefacts affected

| Artefact (Phase 3 SDD) | Affected? | Change |
|---|---|---|
| Use case model | Yes | *Grant extension* gains a class-wide scope (lecturer); UC-01 step 4 ("displays the applicable deadline") now resolves BR-11. |
| Class diagram (Fig 2) | Yes | `ExtensionService::grantClassExtension()`; `ICourseRepository::insertClassExtension()`, `findClassExtensionDeadline()`; `DeadlineEvaluator::applicableDeadline()` (static, pure); `AssignmentView.classExtensionDeadline`; new entity `ClassExtension`. See `CR1_DESIGN_UPDATES.md`. |
| ERD (Fig 7) and data dictionary | Yes | New table `class_extensions` (1 assignment : 0..* class extensions). |
| Sequence: Submit assignment (Fig 5) | Yes | The deadline lookup becomes: find assignment → find individual extension → find class extension → `applicableDeadline()` → `latenessDays()`. |
| Activity diagram (Fig 4) | Yes (minor) | Lecturer swimlane: "grant extension" may be individual or class-wide. |
| State machine: Submission (Fig 3) | **No** | States and transitions are unchanged; only the deadline used for the OnTime/Late guard changes, and that guard is already expressed as "applicable deadline". |
| Sequence: Moderate and release (Fig 6) | **No** | Moderation does not read deadlines. |
| Component diagram (Fig 1), deployment diagram | **No** | No new component, node or dependency. |

## 5. Code, schema and tests affected

| Item | Change |
|---|---|
| `database_schema.sql` | `CREATE TABLE IF NOT EXISTS class_extensions …` + index. Additive only: no existing table, column or constraint changes. Because the schema is embedded and executed at every start-up with `IF NOT EXISTS`, **a v1.0 database is migrated automatically and non-destructively the first time v1.1 opens it**. |
| `src/repositories/Repositories.hpp`, `src/data/SqliteRepositories.*` | Two repository operations; the assignment-list query returns the class-wide deadline in force. |
| `src/services/DeadlineEvaluator.*` | `applicableDeadline(original, individual, classWide)`: the **only** place BR-11 is implemented. |
| `src/services/AssessmentServices.cpp` | `SubmissionService::submit` uses the resolver; `ExtensionService::grantClassExtension`. |
| `src/services/LmsService.*`, `src/main.cpp` | Facade method; menu option **18** "Grant class-wide extension [Lecturer]"; `class extension …` in the assignment list. |
| `src/domain/Records.hpp` | `AssignmentView::classExtensionDeadline`. |
| Tests changed | `T_PERSIST_05` expects 15 tables (the CR1 table is an approved addition); `FakeCourses` implements the two new operations. |
| Tests added | `T_CR1_01` … `T_CR1_10` (unit/integration) and `ST_CR1_01` (console); see §7. |
| Docs | Lecturer and Student manuals, release notes, RTM, data dictionary delta. |

## 6. Risk and effort

| Risk | Likelihood | Impact | Control |
|---|---|---|---|
| Resolver changes lateness for existing behaviour (no class extension) | Low | High (wrong marks) | `max()` with no class extension equals v1.0 behaviour; the whole v1.0 suite is the regression check, run unchanged. |
| Migration loses or alters v1.0 data | Low | High | Additive DDL only; T_CR1_09 opens a real v1.0-schema database with data and compares it. Backup before upgrade (installation guide §8). |
| Precedence rule not what the school wants | Medium | Medium | Provisional and isolated (CR1-D2); one function plus its tests would change. |
| Menu text change breaks system tests | Low | Low | Option 18 is appended; existing text is unchanged. |

Estimated effort: about 1 day, including documents.

## 7. Results (filled in after implementation)

See §8 of this file after implementation and `docs/maintenance/REGRESSION_RESULTS.md`.
