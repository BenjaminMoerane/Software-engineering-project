PRAGMA foreign_keys = ON;
BEGIN;
CREATE TABLE IF NOT EXISTS departments (
  department_id INTEGER PRIMARY KEY,
  name TEXT NOT NULL UNIQUE CHECK(length(trim(name)) > 0)
);
CREATE TABLE IF NOT EXISTS users (
  user_id INTEGER PRIMARY KEY,
  username TEXT NOT NULL UNIQUE CHECK(length(trim(username)) > 0),
  password_hash TEXT NOT NULL CHECK(length(password_hash) > 0),
  password_salt TEXT NOT NULL CHECK(length(password_salt) > 0),
  role TEXT NOT NULL CHECK(role IN ('Administrator','Lecturer','HeadOfDepartment','Student')),
  full_name TEXT NOT NULL CHECK(length(trim(full_name)) > 0),
  department_id INTEGER REFERENCES departments(department_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1)),
  created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
  CHECK(role <> 'HeadOfDepartment' OR department_id IS NOT NULL)
);
CREATE TABLE IF NOT EXISTS courses (
  course_id INTEGER PRIMARY KEY,
  department_id INTEGER NOT NULL REFERENCES departments(department_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  course_code TEXT NOT NULL UNIQUE CHECK(length(trim(course_code)) > 0),
  title TEXT NOT NULL CHECK(length(trim(title)) > 0)
);
CREATE TABLE IF NOT EXISTS course_lecturers (
  course_id INTEGER NOT NULL REFERENCES courses(course_id) ON UPDATE CASCADE ON DELETE CASCADE,
  lecturer_id INTEGER NOT NULL REFERENCES users(user_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  assigned_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
  PRIMARY KEY(course_id, lecturer_id)
);
CREATE TABLE IF NOT EXISTS enrolments (
  enrolment_id INTEGER PRIMARY KEY,
  course_id INTEGER NOT NULL REFERENCES courses(course_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  student_id INTEGER NOT NULL REFERENCES users(user_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  enrolled_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
  UNIQUE(course_id, student_id)
);
CREATE TABLE IF NOT EXISTS assignments (
  assignment_id INTEGER PRIMARY KEY,
  course_id INTEGER NOT NULL REFERENCES courses(course_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  created_by INTEGER NOT NULL REFERENCES users(user_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  title TEXT NOT NULL CHECK(length(trim(title)) > 0),
  description TEXT NOT NULL DEFAULT '',
  deadline TEXT NOT NULL,
  max_mark REAL NOT NULL CHECK(max_mark > 0),
  created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
  active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1))
);
CREATE TABLE IF NOT EXISTS extensions (
  extension_id INTEGER PRIMARY KEY,
  assignment_id INTEGER NOT NULL REFERENCES assignments(assignment_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  student_id INTEGER NOT NULL REFERENCES users(user_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  authorised_by INTEGER NOT NULL REFERENCES users(user_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  revised_deadline TEXT NOT NULL,
  reason TEXT NOT NULL CHECK(length(trim(reason)) > 0),
  created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
  UNIQUE(assignment_id, student_id)
);
CREATE TABLE IF NOT EXISTS submissions (
  submission_id INTEGER PRIMARY KEY,
  assignment_id INTEGER NOT NULL REFERENCES assignments(assignment_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  student_id INTEGER NOT NULL REFERENCES users(user_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  current_version_no INTEGER NOT NULL DEFAULT 0 CHECK(current_version_no >= 0),
  created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
  UNIQUE(assignment_id, student_id)
);
CREATE TABLE IF NOT EXISTS submission_versions (
  version_id INTEGER PRIMARY KEY,
  submission_id INTEGER NOT NULL REFERENCES submissions(submission_id) ON UPDATE CASCADE ON DELETE CASCADE,
  version_no INTEGER NOT NULL CHECK(version_no >= 1),
  submitted_at TEXT NOT NULL,
  content_ref TEXT NOT NULL CHECK(length(trim(content_ref)) > 0),
  deadline_used TEXT NOT NULL,
  lateness_days INTEGER NOT NULL DEFAULT 0 CHECK(lateness_days >= 0),
  late_status TEXT NOT NULL CHECK(late_status IN ('OnTime','LateWithinWindow','BeyondWindow')),
  is_current INTEGER NOT NULL DEFAULT 1 CHECK(is_current IN (0,1)),
  UNIQUE(submission_id, version_no)
);
CREATE UNIQUE INDEX IF NOT EXISTS ux_submission_one_current_version ON submission_versions(submission_id) WHERE is_current=1;
CREATE TABLE IF NOT EXISTS marks (
  mark_id INTEGER PRIMARY KEY,
  submission_id INTEGER NOT NULL REFERENCES submissions(submission_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  version_id INTEGER NOT NULL REFERENCES submission_versions(version_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  raw_mark REAL NOT NULL CHECK(raw_mark >= 0),
  penalty_amount REAL NOT NULL DEFAULT 0 CHECK(penalty_amount >= 0),
  final_mark REAL NOT NULL CHECK(final_mark >= 0),
  entered_by INTEGER NOT NULL REFERENCES users(user_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  marked_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
  current INTEGER NOT NULL DEFAULT 1 CHECK(current IN (0,1)),
  UNIQUE(submission_id, version_id)
);
CREATE UNIQUE INDEX IF NOT EXISTS ux_one_current_mark_per_submission ON marks(submission_id) WHERE current=1;
CREATE TABLE IF NOT EXISTS feedback (
  feedback_id INTEGER PRIMARY KEY,
  mark_id INTEGER NOT NULL REFERENCES marks(mark_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  body TEXT NOT NULL,
  author_id INTEGER NOT NULL REFERENCES users(user_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
  UNIQUE(mark_id)
);
CREATE TABLE IF NOT EXISTS moderation_decisions (
  moderation_id INTEGER PRIMARY KEY,
  mark_id INTEGER NOT NULL REFERENCES marks(mark_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  hod_id INTEGER NOT NULL REFERENCES users(user_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  decision TEXT NOT NULL CHECK(decision IN ('Approved','ReturnedForCorrection')),
  reason TEXT NOT NULL,
  decided_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
  UNIQUE(mark_id, moderation_id)
);
CREATE TABLE IF NOT EXISTS release_records (
  release_id INTEGER PRIMARY KEY,
  mark_id INTEGER NOT NULL UNIQUE REFERENCES marks(mark_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  released_by INTEGER NOT NULL REFERENCES users(user_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  released_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))
);
CREATE TABLE IF NOT EXISTS mark_audit (
  audit_id INTEGER PRIMARY KEY,
  mark_id INTEGER NOT NULL REFERENCES marks(mark_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  actor_id INTEGER NOT NULL REFERENCES users(user_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  changed_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
  old_mark REAL,
  new_mark REAL NOT NULL CHECK(new_mark >= 0),
  reason TEXT NOT NULL CHECK(length(trim(reason)) > 0),
  CHECK(old_mark IS NULL OR old_mark >= 0)
);

CREATE INDEX IF NOT EXISTS ix_courses_department ON courses(department_id);
CREATE INDEX IF NOT EXISTS ix_enrolments_student ON enrolments(student_id, course_id);
CREATE INDEX IF NOT EXISTS ix_assignments_course_deadline ON assignments(course_id, deadline);
CREATE INDEX IF NOT EXISTS ix_extensions_assignment_student ON extensions(assignment_id, student_id);
CREATE INDEX IF NOT EXISTS ix_submissions_assignment_student ON submissions(assignment_id, student_id);
CREATE INDEX IF NOT EXISTS ix_versions_submission_time ON submission_versions(submission_id, submitted_at);
CREATE INDEX IF NOT EXISTS ix_marks_current ON marks(submission_id, current);
CREATE INDEX IF NOT EXISTS ix_moderation_decision ON moderation_decisions(mark_id, decided_at);
CREATE INDEX IF NOT EXISTS ix_audit_mark_time ON mark_audit(mark_id, changed_at);
-- CR1 / v1.1 (FR-23, BR-11): class-wide extensions. Append-only: the newest row for an assignment is in force and the
-- older rows are the history of changes (who, when, new deadline, reason). Applies to every student enrolled in the
-- assignment's course. Additive change: a v1.0 database gains this table, unchanged otherwise, on first start of v1.1.
CREATE TABLE IF NOT EXISTS class_extensions (
  class_extension_id INTEGER PRIMARY KEY,
  assignment_id INTEGER NOT NULL REFERENCES assignments(assignment_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  authorised_by INTEGER NOT NULL REFERENCES users(user_id) ON UPDATE CASCADE ON DELETE RESTRICT,
  revised_deadline TEXT NOT NULL,
  reason TEXT NOT NULL CHECK(length(trim(reason)) > 0),
  created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))
);
CREATE INDEX IF NOT EXISTS ix_class_extensions_assignment ON class_extensions(assignment_id, class_extension_id);
COMMIT;
