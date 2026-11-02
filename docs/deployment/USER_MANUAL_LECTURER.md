# User manual — Lecturer (v1.1)

Read [USER_MANUAL_COMMON.md](USER_MANUAL_COMMON.md) first. You can work only with courses the Administrator has assigned to you.

## 1. Create an assignment — option `7`

| Prompt | Example | Rules |
|---|---|---|
| `Course ID:` | `3` | Must be a course assigned to you. |
| `Title:` / `Description:` | `Essay 1` / `Explain photosynthesis` | Must not be empty. |
| `Deadline UTC (YYYY-MM-DDTHH:MM:SSZ):` | `2026-11-20T12:00:00Z` | A real date in UTC (14:00 Maseru = `12:00:00Z`). |
| `Maximum mark:` | `100` | Greater than zero. |

Result: `Created assignment N`. Students enrolled in the course now see it with option `1`.

## 2. Give one student more time — option `8`

Enter the assignment ID, the student's user ID, the **revised deadline** (must be later than the original, otherwise `Revised deadline must be later than original deadline`) and a reason. The reason is stored. The student sees `your extension <date>` in their assignment list, and lateness for that student is measured from the revised deadline.

## 2a. Give the whole class more time — option `18` (new in v1.1)

Enter the assignment ID, the **revised deadline for the whole class** (UTC) and a reason. Result: `Class-wide extension saved for every student enrolled in the course.` Every student enrolled in the course, including students enrolled later, sees `class extension <date>` in their assignment list.

| Rule | Detail |
|---|---|
| Must be later | than the original deadline, and later than any class-wide extension already in force. You can extend again, but you cannot bring the date forward (`A class-wide extension can only move the deadline later than …`). |
| Kept on record | Every class-wide extension and every change is stored with your name, the time and the reason. |
| Combined with individual extensions | Each student gets the **latest** of: the original deadline, the class-wide extension, and their own individual extension. A class extension never shortens a student's individual extension. |
| Work already submitted | Keeps the lateness recorded when it was submitted. To excuse one student's lateness, give that student an individual extension **before** they submit. |

These combination rules are provisional (CR1-D1 … D4) until approved by the Head of Department.

## 3. Mark work — option `10`

The screen first lists your marking queue:

```
Submission 1 version 1 student Thabo Mokoena assignment Essay 1 status LateWithinWindow max 100 file C:\LMS\data\submission_files\submission_fab14184…bf76.pdf
```

- `version` is the latest version the student submitted (they may resubmit before the deadline).
- `status` is `OnTime`, `LateWithinWindow` (1–3 days late) or `BeyondWindow` (more than 3 days).
- `file` is the full path of the stored copy on this computer (renamed to a random name for security); open it from there to read it.

Then enter the submission ID, the **raw mark** (0 to the maximum), your **written feedback** for the student, and a **mark-change reason** (for example `first marking`). The reason goes into the audit log with your name and the time.

The system applies the late penalty itself; do not deduct it yourself:

| Status | What happens to the raw mark (provisional rule, awaiting HoD approval) |
|---|---|
| OnTime | No deduction |
| LateWithinWindow | 10% of the maximum mark per late day (each started 24 hours counts as a day), at most 3 days; never below 0 |
| BeyondWindow | Final mark 0 |

Result: `Mark recorded as ID N (pending HoD moderation).` The student cannot see the mark yet.

## 4. Corrections

- **Before the HoD has decided:** mark the same submission again; the mark is updated and the change is audited (old value, new value, reason).
- **After the HoD approved it:** you cannot change it (`Mark is approved by the HoD; it must be returned for correction …`). Ask the HoD to return it.
- **Returned by the HoD:** it reappears in your queue (option `10`); re-mark it with a reason, and it goes back to the HoD.
- **After release:** changes are blocked in this version, pending a school decision (OD-08).

## Known limitation

If a student resubmits **after** you have marked their work but before the deadline, the existing mark stays attached to the earlier version and the new version does not reappear in your queue. Until the school decides the rule (observation O-6), avoid marking before the deadline has passed.
