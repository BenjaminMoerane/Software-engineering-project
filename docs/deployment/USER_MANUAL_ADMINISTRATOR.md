# User manual — Administrator (v1.1)

Read [USER_MANUAL_COMMON.md](USER_MANUAL_COMMON.md) first. You set up the school's structure: departments, accounts, courses, lecturer assignments and enrolments. You cannot mark, moderate or submit.

## First-time setup order

Do these in order, because each step needs the ID from the one before:

| Step | Option | You enter | You get |
|---|---|---|---|
| 1 | `2` Create department | Name, e.g. `Mathematics` | `Created department 1` |
| 2 | `3` Create user (Head of Department) | Role `HeadOfDepartment`, department ID `1`, username, temporary password (10+ characters), full name | `Created user N` |
| 3 | `3` Create user (Lecturers, Students) | Role `Lecturer` or `Student`; department ID or `0` | `Created user N` |
| 4 | `4` Create course | Department ID, course code (e.g. `MATH101`), title | `Created course N` |
| 5 | `5` Assign lecturer | Course ID, lecturer's user ID | `Lecturer assigned.` |
| 6 | `6` Enrol student | Course ID, student's user ID | `Student enrolled.` |

Role names must be typed exactly: `Administrator`, `Lecturer`, `HeadOfDepartment`, `Student`. A Head of Department must belong to a department (`HeadOfDepartment must belong to a department`), because they can only moderate marks in their own department.

Write down IDs as you create them; This version has no "list users" or "list courses" screen (option `1` lists assignments only). Keeping a simple register (ID, name, role) next to the computer avoids mistakes.

## Maintaining accounts

| Task | Option | Notes |
|---|---|---|
| Change a name, role or department | `15` Update user | Enter user ID, new full name, role, department ID. |
| Rename a department | `16` | |
| Change a course code, title or department | `17` | |
| Someone leaves | `14` Deactivate user | The account can no longer log in. Its history (submissions, marks, audit records) is kept. You cannot deactivate your own account. |

**Passwords:** passwords are never shown or stored, only a one-way hash. This version has **no password-reset option**. If a user forgets their password, the only route is to deactivate the account and create a new one, which does not carry over the old account's submissions or marks. Ask users to keep passwords safe, and hand out temporary passwords in person.

## Good practice

- Use a strong password of your own; the Administrator account controls every other account.
- Deactivate accounts of people who have left, on the day they leave.
- Ask the technician to make a backup before each term and before any upgrade.
