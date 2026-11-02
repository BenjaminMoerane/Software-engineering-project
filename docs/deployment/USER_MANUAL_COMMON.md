# User manual — common to all roles (v1.0)

## Starting and logging in

1. Sit at the LMS computer. The technician has set up a shortcut or command that starts the LMS (`lms-start`).
2. At `Login username (or 'quit'):` type your username and press Enter.
3. At `Password:` type your password. **Nothing appears while you type**; this is intentional. Press Enter.
4. You see `Welcome, <your name> (<your role>)` and the menu.

If you see `Login failed: Invalid username or password`, check Caps Lock and try again. The message is the same whether the username or the password is wrong, so that guessing usernames is harder. Accounts can be deactivated by the Administrator; a deactivated account cannot log in.

## The menu

All roles see the same numbered menu. Options marked `[Admin]`, `[Lecturer]`, `[HoD]` or `[Student]` only work for that role; any other role gets `Operation failed: Access denied: role is not permitted to perform this operation`. The checks are made by the system, not by the menu, so this cannot be bypassed.

Type the number and press Enter. When a question asks for an ID (for example `Assignment ID:`), type the number shown in the list printed just before it.

## Dates and times

Deadlines are written in UTC as `YYYY-MM-DDTHH:MM:SSZ`, for example `2026-11-20T12:00:00Z` for 12:00 UTC on 20 November 2026. Lesotho time (SAST) is UTC + 2 hours, so 14:00 in Maseru is `12:00:00Z`.

## Logging out

Choose `0` to log out. At the login prompt, type `quit` to close the program. **Always log out** before leaving the computer: the next person would otherwise act as you.

## Accessibility

The LMS is a plain-text console program, which works with screen readers such as NVDA (Windows) or Orca (Linux), and with the operating system's zoom and high-contrast settings. Ask the technician to increase the console font size if needed. Every action is keyboard-only; no mouse is needed.

## Role guides

- [Administrator](USER_MANUAL_ADMINISTRATOR.md)
- [Lecturer](USER_MANUAL_LECTURER.md)
- [Head of Department](USER_MANUAL_HOD.md)
- [Student](USER_MANUAL_STUDENT.md)
