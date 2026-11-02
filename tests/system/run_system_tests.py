#!/usr/bin/env python3
"""System and acceptance tests for the CS4433 Moderated LMS console program.

Each test starts a fresh database in a fresh temporary directory and drives the real `lms_app`
executable through standard input, exactly as a user would, then checks what the program printed
and what it stored. Test IDs (ST-*) are listed in docs/test/TEST_PLAN.md and docs/test/RTM.md.

Deadlines are computed relative to the current time because the console program uses the real clock.
Usage: run_system_tests.py --app /path/to/lms_app [unittest options]
"""
import argparse
import datetime as dt
import os
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import unittest

APP = None

ADMIN_PW = "AdminPassw0rd!!"
HOD_PW = "HodPassw0rd!!!"
LECT_PW = "LectPassw0rd!!"
STUD_PW = "StudPassw0rd!!"
STUD2_PW = "Stud2Passw0rd!!"


def stamp(delta):
    return (dt.datetime.now(dt.timezone.utc) + delta).strftime("%Y-%m-%dT%H:%M:%SZ")


def hours(n):
    return dt.timedelta(hours=n)


# Fixed world (IDs are deterministic because every test starts from an empty database):
# users 1 admin, 2 hod, 3 lect, 4 stud, 5 stud2; department 1 Science; course 1 SCI101 (lect teaches, stud and stud2 enrolled)
SETUP = [
    "admin", ADMIN_PW, "Administrator",           # first-run setup
    "admin", ADMIN_PW,                            # login
    "2", "Science",
    "3", "HeadOfDepartment", "1", "hod", HOD_PW, "Head Person",
    "3", "Lecturer", "1", "lect", LECT_PW, "Lecturer Person",
    "3", "Student", "1", "stud", STUD_PW, "Student Person",
    "3", "Student", "1", "stud2", STUD2_PW, "Student Two",
    "4", "1", "SCI101", "Science",
    "5", "1", "3",
    "6", "1", "4",
    "6", "1", "5",
    "0",
]


def login(user, pw):
    return [user, pw]


class SystemTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix="lms_st_")
        self.addCleanup(shutil.rmtree, self.dir, True)
        with open(os.path.join(self.dir, "work.txt"), "w") as f:
            f.write("submission text\n")
        self.db = os.path.join(self.dir, "lms.sqlite3")

    # -- helpers ---------------------------------------------------------------------------
    def run_app(self, lines, wrapper=None, db=None):
        text = "\n".join(lines) + "\n"
        cmd = (wrapper or []) + [APP, db or self.db]
        p = subprocess.run(cmd, input=text, capture_output=True, text=True, cwd=self.dir, timeout=300)
        return p

    def world(self, extra=None):
        """Create the fixed world; optionally continue as admin with extra menu lines, then quit."""
        lines = SETUP[:-1] + (extra or []) + ["0", "quit"]
        p = self.run_app(lines)
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        return p

    def role(self, user, pw, ops):
        lines = login(user, pw) + ops + ["0", "quit"]
        p = self.run_app(lines)
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(p.stderr, "")
        return p.stdout

    def assignment(self, deadline, title="Lab", max_mark="100"):
        out = self.role("lect", LECT_PW, ["7", "1", title, "desc", deadline, max_mark])
        self.assertIn("Created assignment", out)
        return out

    def query(self, sql):
        c = sqlite3.connect(self.db)
        try:
            return c.execute(sql).fetchall()
        finally:
            c.close()

    def submit(self, aid, user="stud", pw=STUD_PW, path="work.txt"):
        return self.role(user, pw, ["9", str(aid), path])

    def mark(self, sub, raw, fb="Well done", reason="Initial mark"):
        return self.role("lect", LECT_PW, ["10", str(sub), str(raw), fb, reason])

    def moderate_release(self, mark_id=1):
        return self.role("hod", HOD_PW, ["11", str(mark_id), "y", "Looks fine", "12", str(mark_id)])


# ===========================================================================================
class UC01Submit(SystemTest):
    def test_ST_UC01_01_on_time_submission_main_flow(self):
        self.world()
        self.assignment(stamp(hours(48)))
        listing = self.role("stud", STUD_PW, ["1"])
        self.assertIn("SCI101 | Lab", listing)
        out = self.submit(1)
        self.assertIn("Submission file stored and saved (submission ID 1)", out)
        rows = self.query("SELECT version_no, late_status, lateness_days, is_current FROM submission_versions")
        self.assertEqual(rows, [(1, "OnTime", 0, 1)])
        self.assertEqual(len(os.listdir(os.path.join(self.dir, "submission_files"))), 1)

    def test_ST_UC01_02_late_within_window_is_classified_and_penalised_when_marked(self):
        self.world()
        self.assignment(stamp(-hours(30)))  # 30 h ago => 2 started 24 h periods? elapsed 30 h => 2 late days
        self.submit(1)
        status, days = self.query("SELECT late_status, lateness_days FROM submission_versions")[0]
        self.assertEqual(status, "LateWithinWindow")
        self.assertEqual(days, 2)
        self.mark(1, 80)
        raw, pen, final = self.query("SELECT raw_mark, penalty_amount, final_mark FROM marks")[0]
        self.assertEqual((raw, pen, final), (80, 20, 60))  # provisional OD-01..04: 10% of max (100) per late day

    def test_ST_UC01_03_beyond_window_is_kept_and_marks_zero(self):
        self.world()
        self.assignment(stamp(-hours(24 * 5)))
        self.submit(1)
        self.assertEqual(self.query("SELECT late_status FROM submission_versions")[0][0], "BeyondWindow")
        self.mark(1, 90)
        self.assertEqual(self.query("SELECT final_mark FROM marks")[0][0], 0)

    def test_ST_UC01_04_resubmission_before_deadline_keeps_history(self):
        self.world()
        self.assignment(stamp(hours(48)))
        self.submit(1)
        out = self.submit(1)
        self.assertIn("Submission file stored", out)
        rows = self.query("SELECT version_no, is_current FROM submission_versions ORDER BY version_no")
        self.assertEqual(rows, [(1, 0), (2, 1)])
        self.assertEqual(len(os.listdir(os.path.join(self.dir, "submission_files"))), 2)

    def test_ST_UC01_05_resubmission_after_deadline_is_rejected(self):
        self.world()
        self.assignment(stamp(-hours(1)))
        self.assertIn("Submission file stored", self.submit(1))  # first (late) submission accepted
        out = self.submit(1)
        self.assertIn("Operation failed", out)
        self.assertEqual(self.query("SELECT count(*) FROM submission_versions")[0][0], 1)

    def test_ST_UC01_06_individual_extension_changes_the_applicable_deadline(self):
        self.world()
        self.assignment(stamp(-hours(1)))
        out = self.role("lect", LECT_PW, ["8", "1", "4", stamp(hours(48)), "Medical"])
        self.assertIn("Extension saved", out)
        self.assertIn("your extension", self.role("stud", STUD_PW, ["1"]))
        self.submit(1)
        self.assertEqual(self.query("SELECT late_status FROM submission_versions")[0][0], "OnTime")
        # the other student has no extension and is late
        self.submit(1, "stud2", STUD2_PW)
        self.assertEqual(
            self.query("SELECT sv.late_status FROM submission_versions sv JOIN submissions s USING(submission_id) "
                       "WHERE s.student_id=5")[0][0], "LateWithinWindow")

    def test_ST_UC01_07_exception_flows_missing_file_and_unenrolled(self):
        extra = ["3", "Student", "1", "outsider", "OutsiderPw0rd!!", "Outside Student"]
        self.world(extra)
        self.assignment(stamp(hours(48)))
        self.assertIn("Operation failed", self.submit(1, path="does_not_exist.txt"))
        self.assertIn("Operation failed", self.submit(1, "outsider", "OutsiderPw0rd!!"))
        self.assertEqual(self.query("SELECT count(*) FROM submissions")[0][0], 0)

    def test_ST_UC01_08_empty_file_and_disallowed_type_are_rejected(self):
        self.world()
        self.assignment(stamp(hours(48)))
        open(os.path.join(self.dir, "empty.txt"), "w").close()
        with open(os.path.join(self.dir, "tool.exe"), "w") as f:
            f.write("x")
        self.assertIn("Operation failed", self.submit(1, path="empty.txt"))
        self.assertIn("Operation failed", self.submit(1, path="tool.exe"))
        self.assertEqual(self.query("SELECT count(*) FROM submissions")[0][0], 0)


class UC02Mark(SystemTest):
    def prepare(self):
        self.world()
        self.assignment(stamp(hours(48)))
        self.submit(1)

    def test_ST_UC02_01_lecturer_queue_and_mark_main_flow(self):
        self.prepare()
        out = self.role("lect", LECT_PW, ["10", "1", "80", "Well done", "Initial mark"])
        self.assertIn("Submission 1 version 1 student Student Person", out)
        self.assertIn("Mark recorded as ID 1 (pending HoD moderation)", out)
        self.assertEqual(self.query("SELECT count(*) FROM mark_audit")[0][0], 1)

    def test_ST_UC02_02_mark_range_boundaries(self):
        self.prepare()
        for bad in ("-1", "100.01", "101"):
            out = self.role("lect", LECT_PW, ["10", "1", bad, "fb", "r"])
            self.assertNotIn("Mark recorded", out, bad)
            self.assertIn("Operation failed", out, bad)
        self.assertEqual(self.query("SELECT count(*) FROM marks")[0][0], 0)
        # non-numeric text is re-prompted, not accepted (FR-20)
        out = self.role("lect", LECT_PW, ["10", "1", "abc", "40", "fb", "r"])
        self.assertIn("Enter a number.", out)
        self.assertEqual(self.query("SELECT raw_mark FROM marks")[0][0], 40)
        self.assertIn("Mark recorded", self.mark(1, 0))
        self.assertIn("Mark recorded", self.mark(1, 100, reason="Raise to max"))
        self.assertEqual(self.query("SELECT final_mark FROM marks")[0][0], 100)
        self.assertEqual(self.query("SELECT count(*) FROM mark_audit")[0][0], 3)

    def test_ST_UC02_03_feedback_and_reason_are_mandatory(self):
        self.prepare()
        self.assertIn("Operation failed", self.mark(1, 50, fb=""))
        self.assertIn("Operation failed", self.mark(1, 50, reason=""))
        self.assertEqual(self.query("SELECT count(*) FROM marks")[0][0], 0)

    def test_ST_UC02_04_marks_are_hidden_from_the_student_until_release(self):
        self.prepare()
        self.mark(1, 80)
        self.assertNotIn("80/100", self.role("stud", STUD_PW, ["13"]))
        self.role("hod", HOD_PW, ["11", "1", "y", "ok"])  # approved, not released
        self.assertNotIn("80/100", self.role("stud", STUD_PW, ["13"]))

    def test_ST_UC02_05_a_lecturer_cannot_mark_another_course(self):
        extra = ["4", "1", "SCI200", "Other", "3", "Lecturer", "1", "lect2", "Lect2Passw0rd!!", "Other Lecturer"]
        self.world(extra)
        self.assignment(stamp(hours(48)))
        self.submit(1)
        out = self.role("lect2", "Lect2Passw0rd!!", ["10", "1", "50", "fb", "r"])
        self.assertIn("Operation failed", out)
        self.assertEqual(self.query("SELECT count(*) FROM marks")[0][0], 0)


class UC03Moderate(SystemTest):
    def prepare(self, raw=80):
        self.world()
        self.assignment(stamp(hours(48)))
        self.submit(1)
        self.mark(1, raw)

    def test_ST_UC03_01_hod_queue_approve_release_student_sees_result(self):
        self.prepare()
        queue = self.role("hod", HOD_PW, ["11", "1", "y", "Looks fine"])
        self.assertIn("Mark 1 | SCI101 | Lab | student Student Person", queue)
        self.assertIn("PendingModeration", queue)
        self.assertIn("Moderation decision recorded", queue)
        # approval alone does not release
        self.assertNotIn("80/100", self.role("stud", STUD_PW, ["13"]))
        rel = self.role("hod", HOD_PW, ["12", "1"])
        self.assertIn("ApprovedAwaitingRelease", rel)
        self.assertIn("Mark released", rel)
        self.assertIn("SCI101 | Lab | 80/100 | Well done", self.role("stud", STUD_PW, ["13"]))
        self.assertNotIn("Lab", self.role("stud2", STUD2_PW, ["13"]))  # other student sees nothing
        self.assertEqual(self.query("SELECT count(*) FROM release_records")[0][0], 1)

    def test_ST_UC03_02_release_requires_approval(self):
        self.prepare()
        self.assertIn("Operation failed", self.role("hod", HOD_PW, ["12", "1"]))
        self.assertEqual(self.query("SELECT count(*) FROM release_records")[0][0], 0)

    def test_ST_UC03_03_return_for_correction_then_remark_then_release(self):
        self.prepare()
        self.role("hod", HOD_PW, ["11", "1", "n", "Please clarify feedback"])
        self.assertIn("Operation failed", self.role("hod", HOD_PW, ["12", "1"]))  # returned: cannot release
        # lecturer sees it back in their queue and corrects it
        self.assertIn("Submission 1", self.role("lect", LECT_PW, ["10", "1", "75", "Clarified", "Corrected after HoD return"]))
        self.assertEqual(self.query("SELECT final_mark FROM marks")[0][0], 75)
        self.assertEqual(self.query("SELECT count(*) FROM mark_audit")[0][0], 2)
        out = self.moderate_release()
        self.assertIn("Mark released", out)
        self.assertIn("75/100", self.role("stud", STUD_PW, ["13"]))

    def test_ST_UC03_04_returned_mark_reappears_in_hod_queue_after_correction(self):
        self.prepare()
        self.role("hod", HOD_PW, ["11", "1", "n", "Please clarify"])
        self.mark(1, 75, "Clarified", "Corrected")
        out = self.role("hod", HOD_PW, ["12", "1"])  # listing printed before the prompt; release then fails (not approved)
        self.assertIn("Mark 1 |", out)
        self.assertIn("PendingModeration", out)

    def test_ST_UC03_05_a_change_after_approval_needs_a_fresh_decision(self):
        self.prepare()
        self.role("hod", HOD_PW, ["11", "1", "y", "ok"])
        self.assertIn("Operation failed", self.mark(1, 10, reason="Try to change approved mark"))
        self.assertEqual(self.query("SELECT final_mark FROM marks")[0][0], 80)

    def test_ST_UC03_06_released_marks_cannot_be_changed_or_moderated_again(self):
        self.prepare()
        self.moderate_release()
        self.assertIn("Operation failed", self.mark(1, 10, reason="late change"))
        self.assertIn("Operation failed", self.role("hod", HOD_PW, ["11", "1", "n", "again"]))
        self.assertEqual(self.query("SELECT final_mark FROM marks")[0][0], 80)

    def test_ST_UC03_07_hod_of_another_department_cannot_moderate(self):
        extra = ["2", "Arts", "3", "HeadOfDepartment", "2", "hod2", "Hod2Passw0rd!!", "Arts Head"]
        self.world(extra)
        self.assignment(stamp(hours(48)))
        self.submit(1)
        self.mark(1, 80)
        out = self.role("hod2", "Hod2Passw0rd!!", ["11", "1", "y", "ok"])
        self.assertNotIn("Mark 1 |", out)  # not listed
        self.assertIn("Operation failed", out)
        self.assertEqual(self.query("SELECT count(*) FROM moderation_decisions")[0][0], 0)


class Access(SystemTest):
    def test_ST_ACC_01_roles_cannot_use_other_roles_menu_options(self):
        self.world()
        self.assignment(stamp(hours(48)))
        self.assertIn("Access denied", self.role("stud", STUD_PW, ["2", "Hack"]))
        self.assertIn("Access denied", self.role("lect", LECT_PW, ["3", "Student", "1", "x", "XPassw0rd!!!!", "X"]))
        self.assertIn("Access denied", self.role("hod", HOD_PW, ["7", "1", "t", "d", stamp(hours(5)), "10"]))
        self.assertIn("Access denied", self.role("admin", ADMIN_PW, ["10", "1", "50", "fb", "r"]))
        self.assertIn("Access denied", self.role("admin", ADMIN_PW, ["11", "1", "y", "r"]))
        self.assertIn("Access denied", self.role("admin", ADMIN_PW, ["12", "1"]))
        self.assertIn("Access denied", self.role("admin", ADMIN_PW, ["9", "1", "work.txt"]))
        self.assertEqual(self.query("SELECT count(*) FROM users")[0][0], 5)
        self.assertEqual(self.query("SELECT count(*) FROM departments")[0][0], 1)

    def test_ST_ACC_02_wrong_password_unknown_user_and_deactivated_user_cannot_log_in(self):
        self.world()
        p = self.run_app(["stud", "wrong-password-1", "nobody", "whatever-password", "quit"])
        self.assertEqual(p.stdout.count("Login failed"), 2)
        self.world_admin(["14", "4"])
        p = self.run_app(["stud", STUD_PW, "quit"])
        self.assertIn("Login failed", p.stdout)

    def world_admin(self, ops):
        return self.role("admin", ADMIN_PW, ops)

    def test_ST_ACC_03_passwords_are_not_stored_in_clear(self):
        self.world()
        with open(self.db, "rb") as f:
            raw = f.read()
        for pw in (ADMIN_PW, HOD_PW, LECT_PW, STUD_PW, STUD2_PW):
            self.assertNotIn(pw.encode(), raw)
        # also not in the write-ahead log, which may hold recent pages
        wal = self.db + "-wal"
        if os.path.exists(wal):
            with open(wal, "rb") as f:
                raw = f.read()
            for pw in (ADMIN_PW, HOD_PW, LECT_PW, STUD_PW, STUD2_PW):
                self.assertNotIn(pw.encode(), raw)


class Robustness(SystemTest):
    def test_ST_ROB_01_invalid_menu_input_does_not_crash(self):
        self.world()
        p = self.run_app(login("stud", STUD_PW) + ["abc", "", "99", "-5", "1.5", "0", "quit"])
        self.assertEqual(p.returncode, 0)
        self.assertIn("Unknown option", p.stdout)
        self.assertEqual(p.stderr, "")

    def test_ST_ROB_02_input_ending_early_never_hangs_or_corrupts(self):
        # empty database, input ends during first-run setup
        p = self.run_app([])
        self.assertEqual(p.returncode, 1)
        self.world()
        for cut in (["stud"], login("stud", STUD_PW), login("stud", STUD_PW) + ["9"], login("stud", STUD_PW) + ["9", "1"]):
            p = self.run_app(cut)
            self.assertIn(p.returncode, (0, 1))
        self.assertEqual(self.query("PRAGMA integrity_check")[0][0], "ok")

    def test_ST_ROB_03_unusable_database_path_gives_a_clean_error(self):
        p = self.run_app(["quit"], db=os.path.join(self.dir, "no_such_dir", "x.sqlite3"))
        self.assertEqual(p.returncode, 1)
        self.assertNotIn("Traceback", p.stderr)
        with open(os.path.join(self.dir, "garbage.db"), "wb") as f:
            f.write(os.urandom(4096))
        p = self.run_app(["quit"], db=os.path.join(self.dir, "garbage.db"))
        self.assertEqual(p.returncode, 1)


class Persistence(SystemTest):
    def test_ST_PERS_01_full_workflow_across_separate_program_runs(self):
        self.world()
        self.assignment(stamp(hours(48)))
        self.submit(1)
        self.mark(1, 70)
        self.moderate_release()
        self.assertIn("70/100", self.role("stud", STUD_PW, ["13"]))
        c = sqlite3.connect(self.db)
        self.assertEqual(c.execute("PRAGMA integrity_check").fetchone()[0], "ok")
        self.assertEqual(c.execute("PRAGMA foreign_key_check").fetchall(), [])
        c.close()


class Offline(SystemTest):
    def test_ST_OFF_01_complete_workflow_with_no_network(self):
        if shutil.which("unshare") is None:
            self.skipTest("unshare not available")
        probe = subprocess.run(["unshare", "-rn", "true"], capture_output=True)
        if probe.returncode != 0:
            self.skipTest("network namespaces not permitted here")
        wrap = ["unshare", "-rn"]
        lines = SETUP[:-1] + ["0"] + login("lect", LECT_PW) + ["7", "1", "Lab", "d", stamp(hours(48)), "100", "0"] \
            + login("stud", STUD_PW) + ["9", "1", "work.txt", "0"] \
            + login("lect", LECT_PW) + ["10", "1", "90", "Great", "Initial", "0"] \
            + login("hod", HOD_PW) + ["11", "1", "y", "ok", "12", "1", "0"] \
            + login("stud", STUD_PW) + ["13", "0", "quit"]
        p = self.run_app(lines, wrapper=wrap)
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertIn("SCI101 | Lab | 90/100 | Great", p.stdout)


class CR1ClassExtension(SystemTest):
    """v1.1 change request CR1 (FR-23, BR-11), driven through the console like a lecturer and students would."""

    def test_ST_CR1_01_class_extension_reaches_every_student_and_changes_timeliness(self):
        self.world()
        self.assignment(stamp(-hours(1)))  # deadline already passed
        new_deadline = stamp(hours(48))
        out = self.role("lect", LECT_PW, ["18", "1", new_deadline, "Exam week clash"])
        self.assertIn("Class-wide extension saved", out)
        for user, pw in (("stud", STUD_PW), ("stud2", STUD2_PW)):
            listing = self.role(user, pw, ["1"])
            self.assertIn("class extension " + new_deadline, listing)
            self.assertNotIn("your extension", listing)
        self.assertIn("Submission file stored", self.submit(1))
        self.assertEqual(self.query("SELECT late_status, deadline_used FROM submission_versions"),
                         [("OnTime", new_deadline)])
        self.assertEqual(self.query("SELECT count(*), max(reason) FROM class_extensions"), [(1, "Exam week clash")])

    def test_ST_CR1_02_exception_flows_wrong_role_and_shortening(self):
        self.world()
        self.assignment(stamp(hours(24)))
        out = self.role("stud", STUD_PW, ["18", "1", stamp(hours(48)), "Please"])
        self.assertIn("Operation failed: Access denied", out)
        first = stamp(hours(72))
        self.assertIn("Class-wide extension saved", self.role("lect", LECT_PW, ["18", "1", first, "First"]))
        out = self.role("lect", LECT_PW, ["18", "1", stamp(hours(48)), "Shorten"])
        self.assertIn("Operation failed: A class-wide extension can only move the deadline later", out)
        self.assertEqual(self.query("SELECT revised_deadline FROM class_extensions"), [(first,)])


@unittest.skipIf(os.name == "nt", "POSIX launcher; lms-start.bat is not exercised here")
class Deployment(SystemTest):
    LAUNCHER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "packaging", "lms-start.sh")

    def test_ST_DEP_01_launcher_works_from_a_relative_path_and_keeps_data_in_the_data_folder(self):
        # Defect F14: the launcher used a relative program path after changing into the data folder.
        bindir = os.path.join(self.dir, "bin")
        os.makedirs(bindir)
        shutil.copy(APP, os.path.join(bindir, "lms_app"))
        shutil.copy(self.LAUNCHER, os.path.join(bindir, "lms-start.sh"))
        os.chmod(os.path.join(bindir, "lms-start.sh"), 0o755)
        lines = SETUP[:-1] + ["0", "quit"]
        p = subprocess.run(["sh", os.path.join("bin", "lms-start.sh"), "data"], input="\n".join(lines) + "\n",
                           capture_output=True, text=True, cwd=self.dir, timeout=300)
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertIn("Database: " + os.path.join(self.dir, "data", "school_lms.sqlite3"), p.stdout)
        self.assertTrue(os.path.exists(os.path.join(self.dir, "data", "school_lms.sqlite3")))


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--app", required=True)
    ns, rest = ap.parse_known_args()
    APP = os.path.abspath(ns.app)
    sys.argv = [sys.argv[0]] + rest
    unittest.main(verbosity=2)
