# User manual — Head of Department (v1.0)

Read [USER_MANUAL_COMMON.md](USER_MANUAL_COMMON.md) first. You review every mark in your department before students can see it (the Principal's requirement). You see only your own department.

## 1. Review the moderation queue — option `11` or `12`

Both options first print your department's queue:

```
Mark 12 | MATH101 | Essay 1 | student Thabo Mokoena | LateWithinWindow | raw 72 penalty 20 final 52/100 | PendingModeration | feedback: Good structure…
```

| State | Meaning | Your action |
|---|---|---|
| `PendingModeration` | Marked by the lecturer, waiting for you | Approve or return (option `11`) |
| `ApprovedAwaitingRelease` | You approved it; students cannot see it yet | Release (option `12`) |

A mark you returned disappears from the queue until the lecturer re-marks it, then it comes back as `PendingModeration`.

Check the raw mark, the late status and penalty, the final mark and the feedback against the student's work.

## 2. Moderate — option `11`

Enter the mark ID, `y` to approve or `n` to return it for correction, and a reason (required; it is recorded). Result: `Moderation decision recorded.`

- **Approve (`y`):** the mark becomes `ApprovedAwaitingRelease`.
- **Return (`n`):** it goes back to the lecturer's queue with your reason. The lecturer cannot change an approved mark without your returning it, so every released mark has a decision on its current value.

## 3. Release — option `12`

Enter the mark ID. Only a mark whose **latest** decision is an approval can be released (`Mark cannot be released without latest HoD approval`). Result: `Mark released.` From now on the student sees the final mark and feedback (option `13`).

Releasing is deliberate and per mark, so you can release a whole class at the same time after checking all of it.

## 4. After release

Moderating or changing a released mark is blocked in v1.0 (`… blocked pending Gate 2 decision OD-08`). If a released mark is wrong, record the case and raise it with the school; the post-release correction rule has not yet been decided.

## Late-penalty rule (provisional)

10% of the maximum mark per late day (each started 24-hour period after the deadline or extension counts as a day), up to 3 days; more than 3 days late gives 0. This follows the HoD statement in the requirements, but details such as calendar days versus 24-hour periods still need your formal approval (decisions OD-01 to OD-04).
