# Data dictionary (v1.1.2)

Generated from [`database_schema.sql`](../database_schema.sql) (3 Nov 2026), the single schema source embedded in the program; regenerate it if the schema changes. 15 tables: 14 from the Phase 3 design plus `class_extensions` (CR1, v1.1). All tables are in Third Normal Form (justification: final report §5). Types are SQLite storage classes; timestamps are TEXT in UTC ISO 8601 (`YYYY-MM-DDTHH:MM:SS.sssZ`); booleans are INTEGER 0/1.

**Class–table mapping.** Each domain entity in the class diagram maps one-to-one to a table of the same name. In the code, entities are realised as plain read/write structs defined in `src/domain/Records.hpp` and passed through the repository interfaces rather than as entity classes; only `User` is a class hierarchy, because it carries behaviour. This is a deliberate difference: the structs carry data only, and behaviour lives in the services.

Foreign keys use `ON UPDATE CASCADE`; `ON DELETE RESTRICT` unless stated, so assessment history cannot be orphaned.

| Table | Purpose | Entity in class diagram (Fig 4.2b) | Code types |
|---|---|---|---|
| `departments` | Academic departments; scope of a HoD. | Department | — (ids only) |
| `users` | Accounts for all four roles, with salted password hash. | User + 4 subclasses | `User` hierarchy; `UserCredentials`, `NewUser`, `AccountState` |
| `courses` | Courses, each owned by one department. | Course | — (ids only) |
| `course_lecturers` | Which lecturers teach which course (M:N). | association Course–Lecturer | — |
| `enrolments` | Which students are enrolled in which course (M:N). | association Course–Student | — |
| `assignments` | Assessed work with deadline and maximum mark. | Assignment | `AssignmentInfo`, `NewAssignment`, `AssignmentView` |
| `extensions` | Individual extension for one student on one assignment. | Extension | via `ICourseRepository` |
| `submissions` | One per student per assignment; points to the current version. | Submission | `ExistingSubmission`, `SubmissionView` |
| `submission_versions` | Every uploaded version, with timeliness fixed at submission. | SubmissionVersion | `NewSubmissionVersion` |
| `marks` | Mark for a submission version: raw, penalty, final. | Mark | `MarkWrite`, `CurrentMark`, `MarkingContext` |
| `feedback` | Written feedback attached to a mark. | Feedback | written with `MarkWrite` |
| `moderation_decisions` | HoD decisions on a mark (approve or return). | ModerationDecision | `ModerationItemView`, `ModerationQueueRow` |
| `release_records` | Release of an approved mark to the student. | ReleaseRecord | read as `ResultView` |
| `mark_audit` | Append-only audit of every mark change (FR-14). | MarkAudit | `MarkAuditEntry` |
| `class_extensions` | CR1 (v1.1): class-wide extension history; newest row in force. | ClassExtension | via `ICourseRepository` |

## `departments`

Academic departments; scope of a HoD.

| Column | Type | Null | Key | Constraints / default | Meaning |
|---|---|---|---|---|---|
| `department_id` | INTEGER | no | PK | — | Owning department |
| `name` | TEXT | no | — | UNIQUE; CHECK length(trim(name)) > 0 | Department name |

## `users`

Accounts for all four roles, with salted password hash.

| Column | Type | Null | Key | Constraints / default | Meaning |
|---|---|---|---|---|---|
| `user_id` | INTEGER | no | PK | — | Surrogate key |
| `username` | TEXT | no | — | UNIQUE; CHECK length(trim(username)) > 0 | Login name |
| `password_hash` | TEXT | no | — | CHECK length(password_hash) > 0 | scrypt hash (hex) |
| `password_salt` | TEXT | no | — | CHECK length(password_salt) > 0 | 16-byte random salt (hex) |
| `role` | TEXT | no | — | CHECK role IN ('Administrator','Lecturer','HeadOfDepartment','Student') | One of the four roles |
| `full_name` | TEXT | no | — | CHECK length(trim(full_name)) > 0 | Display name |
| `department_id` | INTEGER | yes | FK → departments(department_id) | — | Owning department |
| `active` | INTEGER | no | — | CHECK active IN (0,1); default 1 | 1 = usable, 0 = deactivated |
| `created_at` | TEXT | no | — | default strftime('%Y-%m-%dT%H:%M:%fZ','now') | Creation time, UTC ISO 8601 |

- Table constraint: `CHECK(role <> 'HeadOfDepartment' OR department_id IS NOT NULL)`

## `courses`

Courses, each owned by one department.

| Column | Type | Null | Key | Constraints / default | Meaning |
|---|---|---|---|---|---|
| `course_id` | INTEGER | no | PK | — | Surrogate key |
| `department_id` | INTEGER | no | FK → departments(department_id) | — | Owning department |
| `course_code` | TEXT | no | — | UNIQUE; CHECK length(trim(course_code)) > 0 | Short course code |
| `title` | TEXT | no | — | CHECK length(trim(title)) > 0 | Title |

- Index: `INDEX ix_courses_department ON courses(department_id)`

## `course_lecturers`

Which lecturers teach which course (M:N).

| Column | Type | Null | Key | Constraints / default | Meaning |
|---|---|---|---|---|---|
| `course_id` | INTEGER | no | PK, FK → courses(course_id) cascade delete | — | Parent courses |
| `lecturer_id` | INTEGER | no | PK, FK → users(user_id) | — | Assigned lecturer (users) |
| `assigned_at` | TEXT | no | — | default strftime('%Y-%m-%dT%H:%M:%fZ','now') | When assigned, UTC |

- Table constraint: `PRIMARY KEY(course_id, lecturer_id)`

## `enrolments`

Which students are enrolled in which course (M:N).

| Column | Type | Null | Key | Constraints / default | Meaning |
|---|---|---|---|---|---|
| `enrolment_id` | INTEGER | no | PK | — | Surrogate key |
| `course_id` | INTEGER | no | FK → courses(course_id) | — | Parent courses |
| `student_id` | INTEGER | no | FK → users(user_id) | — | Student (users) |
| `enrolled_at` | TEXT | no | — | default strftime('%Y-%m-%dT%H:%M:%fZ','now') | When enrolled, UTC |

- Table constraint: `UNIQUE(course_id, student_id)`
- Index: `INDEX ix_enrolments_student ON enrolments(student_id, course_id)`

## `assignments`

Assessed work with deadline and maximum mark.

| Column | Type | Null | Key | Constraints / default | Meaning |
|---|---|---|---|---|---|
| `assignment_id` | INTEGER | no | PK | — | Surrogate key |
| `course_id` | INTEGER | no | FK → courses(course_id) | — | Parent courses |
| `created_by` | INTEGER | no | FK → users(user_id) | — | Lecturer who created it |
| `title` | TEXT | no | — | CHECK length(trim(title)) > 0 | Title |
| `description` | TEXT | no | — | default '' | Free-text brief |
| `deadline` | TEXT | no | — | — | Original deadline, UTC ISO 8601 |
| `max_mark` | REAL | no | — | CHECK max_mark > 0 | Maximum mark M |
| `created_at` | TEXT | no | — | default strftime('%Y-%m-%dT%H:%M:%fZ','now') | Creation time, UTC ISO 8601 |
| `active` | INTEGER | no | — | CHECK active IN (0,1); default 1 | 1 = usable, 0 = deactivated |

- Index: `INDEX ix_assignments_course_deadline ON assignments(course_id, deadline)`

## `extensions`

Individual extension for one student on one assignment.

| Column | Type | Null | Key | Constraints / default | Meaning |
|---|---|---|---|---|---|
| `extension_id` | INTEGER | no | PK | — | Surrogate key |
| `assignment_id` | INTEGER | no | FK → assignments(assignment_id) | — | Parent assignments |
| `student_id` | INTEGER | no | FK → users(user_id) | — | Student (users) |
| `authorised_by` | INTEGER | no | FK → users(user_id) | — | Lecturer who granted it |
| `revised_deadline` | TEXT | no | — | — | New deadline, UTC |
| `reason` | TEXT | no | — | CHECK length(trim(reason)) > 0 | Reason given (required) |
| `created_at` | TEXT | no | — | default strftime('%Y-%m-%dT%H:%M:%fZ','now') | Creation time, UTC ISO 8601 |

- Table constraint: `UNIQUE(assignment_id, student_id)`
- Index: `INDEX ix_extensions_assignment_student ON extensions(assignment_id, student_id)`

## `submissions`

One per student per assignment; points to the current version.

| Column | Type | Null | Key | Constraints / default | Meaning |
|---|---|---|---|---|---|
| `submission_id` | INTEGER | no | PK | — | Surrogate key |
| `assignment_id` | INTEGER | no | FK → assignments(assignment_id) | — | Parent assignments |
| `student_id` | INTEGER | no | FK → users(user_id) | — | Student (users) |
| `current_version_no` | INTEGER | no | — | CHECK current_version_no >= 0; default 0 | Number of the current version (0 = none yet) |
| `created_at` | TEXT | no | — | default strftime('%Y-%m-%dT%H:%M:%fZ','now') | Creation time, UTC ISO 8601 |

- Table constraint: `UNIQUE(assignment_id, student_id)`
- Index: `INDEX ix_submissions_assignment_student ON submissions(assignment_id, student_id)`

## `submission_versions`

Every uploaded version, with timeliness fixed at submission.

| Column | Type | Null | Key | Constraints / default | Meaning |
|---|---|---|---|---|---|
| `version_id` | INTEGER | no | PK | — | Surrogate key |
| `submission_id` | INTEGER | no | FK → submissions(submission_id) cascade delete | — | Parent submissions |
| `version_no` | INTEGER | no | — | CHECK version_no >= 1 | 1, 2, … per submission |
| `submitted_at` | TEXT | no | — | — | Server time of upload, UTC |
| `content_ref` | TEXT | no | — | CHECK length(trim(content_ref)) > 0 | Stored file name (server-generated) |
| `deadline_used` | TEXT | no | — | — | Applicable deadline at submission (snapshot) |
| `lateness_days` | INTEGER | no | — | CHECK lateness_days >= 0; default 0 | Late days counted at submission |
| `late_status` | TEXT | no | — | CHECK late_status IN ('OnTime','LateWithinWindow','BeyondWindow') | OnTime / LateWithinWindow / BeyondWindow |
| `is_current` | INTEGER | no | — | CHECK is_current IN (0,1); default 1 | 1 for the current version only |

- Table constraint: `UNIQUE(submission_id, version_no)`
- Index: `UNIQUE INDEX ux_submission_one_current_version ON submission_versions(submission_id) WHERE is_current=1`
- Index: `INDEX ix_versions_submission_time ON submission_versions(submission_id, submitted_at)`

## `marks`

Mark for a submission version: raw, penalty, final.

| Column | Type | Null | Key | Constraints / default | Meaning |
|---|---|---|---|---|---|
| `mark_id` | INTEGER | no | PK | — | Surrogate key |
| `submission_id` | INTEGER | no | FK → submissions(submission_id) | — | Parent submissions |
| `version_id` | INTEGER | no | FK → submission_versions(version_id) | — | Parent submission_versions |
| `raw_mark` | REAL | no | — | CHECK raw_mark >= 0 | Mark before penalty |
| `penalty_amount` | REAL | no | — | CHECK penalty_amount >= 0; default 0 | Deduction applied once at marking |
| `final_mark` | REAL | no | — | CHECK final_mark >= 0 | raw − penalty, never below 0 |
| `entered_by` | INTEGER | no | FK → users(user_id) | — | Lecturer who marked |
| `marked_at` | TEXT | no | — | default strftime('%Y-%m-%dT%H:%M:%fZ','now') | When marked, UTC |
| `current` | INTEGER | no | — | CHECK current IN (0,1); default 1 | 1 for the current mark only |

- Table constraint: `UNIQUE(submission_id, version_id)`
- Index: `UNIQUE INDEX ux_one_current_mark_per_submission ON marks(submission_id) WHERE current=1`
- Index: `INDEX ix_marks_current ON marks(submission_id, current)`

## `feedback`

Written feedback attached to a mark.

| Column | Type | Null | Key | Constraints / default | Meaning |
|---|---|---|---|---|---|
| `feedback_id` | INTEGER | no | PK | — | Surrogate key |
| `mark_id` | INTEGER | no | FK → marks(mark_id) | — | Parent marks |
| `body` | TEXT | no | — | — | Feedback text |
| `author_id` | INTEGER | no | FK → users(user_id) | — | Lecturer who wrote it |
| `created_at` | TEXT | no | — | default strftime('%Y-%m-%dT%H:%M:%fZ','now') | Creation time, UTC ISO 8601 |

- Table constraint: `UNIQUE(mark_id)`

## `moderation_decisions`

HoD decisions on a mark (approve or return).

| Column | Type | Null | Key | Constraints / default | Meaning |
|---|---|---|---|---|---|
| `moderation_id` | INTEGER | no | PK | — | Surrogate key |
| `mark_id` | INTEGER | no | FK → marks(mark_id) | — | Parent marks |
| `hod_id` | INTEGER | no | FK → users(user_id) | — | Deciding HoD (users) |
| `decision` | TEXT | no | — | CHECK decision IN ('Approved','ReturnedForCorrection') | Approved / ReturnedForCorrection |
| `reason` | TEXT | no | — | — | Reason given (required) |
| `decided_at` | TEXT | no | — | default strftime('%Y-%m-%dT%H:%M:%fZ','now') | When decided, UTC |

- Table constraint: `UNIQUE(mark_id, moderation_id)`
- Index: `INDEX ix_moderation_decision ON moderation_decisions(mark_id, decided_at)`

## `release_records`

Release of an approved mark to the student.

| Column | Type | Null | Key | Constraints / default | Meaning |
|---|---|---|---|---|---|
| `release_id` | INTEGER | no | PK | — | Surrogate key |
| `mark_id` | INTEGER | no | FK → marks(mark_id) | UNIQUE | Parent marks |
| `released_by` | INTEGER | no | FK → users(user_id) | — | HoD who released it |
| `released_at` | TEXT | no | — | default strftime('%Y-%m-%dT%H:%M:%fZ','now') | When released, UTC |

## `mark_audit`

Append-only audit of every mark change (FR-14).

| Column | Type | Null | Key | Constraints / default | Meaning |
|---|---|---|---|---|---|
| `audit_id` | INTEGER | no | PK | — | Surrogate key |
| `mark_id` | INTEGER | no | FK → marks(mark_id) | — | Parent marks |
| `actor_id` | INTEGER | no | FK → users(user_id) | — | User who changed the mark |
| `changed_at` | TEXT | no | — | default strftime('%Y-%m-%dT%H:%M:%fZ','now') | When changed, UTC |
| `old_mark` | REAL | yes | — | — | Final mark before (NULL on first entry) |
| `new_mark` | REAL | no | — | CHECK new_mark >= 0 | Final mark after |
| `reason` | TEXT | no | — | CHECK length(trim(reason)) > 0 | Reason given (required) |

- Table constraint: `CHECK(old_mark IS NULL OR old_mark >= 0)`
- Index: `INDEX ix_audit_mark_time ON mark_audit(mark_id, changed_at)`

## `class_extensions`

CR1 (v1.1): class-wide extension history; newest row in force.

| Column | Type | Null | Key | Constraints / default | Meaning |
|---|---|---|---|---|---|
| `class_extension_id` | INTEGER | no | PK | — | Surrogate key |
| `assignment_id` | INTEGER | no | FK → assignments(assignment_id) | — | Parent assignments |
| `authorised_by` | INTEGER | no | FK → users(user_id) | — | Lecturer who granted it |
| `revised_deadline` | TEXT | no | — | — | New deadline, UTC |
| `reason` | TEXT | no | — | CHECK length(trim(reason)) > 0 | Reason given (required) |
| `created_at` | TEXT | no | — | default strftime('%Y-%m-%dT%H:%M:%fZ','now') | Creation time, UTC ISO 8601 |

- Index: `INDEX ix_class_extensions_assignment ON class_extensions(assignment_id, class_extension_id)`
