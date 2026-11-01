// T-SUB / T-SUB-BOUNDARY / T-RESUB / T-FILE: submission timing, resubmission and file handling
// (FR-09, FR-10, FR-11; UC-01 main, alternative and exception flows). Timing rules are the provisional
// OD-01 / OD-02 interpretation: elapsed 24-hour periods rounded up, at-or-before the deadline is on time.
#include "TestSupport.hpp"

using namespace lmstest;

class Submission : public LmsFixture {
  protected:
    void SetUp() override {
        LmsFixture::SetUp();
        buildWorld();
    }
    int filesIn(const fs::path& dir) {
        int n = 0;
        if (fs::exists(dir))
            for (const auto& e : fs::directory_iterator(dir))
                n += e.is_regular_file() ? 1 : 0;
        return n;
    }
};

TEST_F(Submission, T_SUB_01_deadline_boundaries_by_equivalence_class) {
    // Deadline is 2026-10-20T12:00:00Z. Each row is a fresh assignment and a fresh submission.
    const struct {
        const char* when;
        int days;
        const char* status;
    } rows[] = {
        {"2026-10-19T12:00:00Z", 0, "OnTime"},           // a day before
        {"2026-10-20T11:59:59Z", 0, "OnTime"},           // one second before
        {"2026-10-20T12:00:00Z", 0, "OnTime"},           // exactly at the deadline (provisional OD-02)
        {"2026-10-20T12:00:01Z", 1, "LateWithinWindow"}, // one second late -> day 1
        {"2026-10-21T12:00:00Z", 1, "LateWithinWindow"}, // exactly 24 h late -> still day 1
        {"2026-10-21T12:00:01Z", 2, "LateWithinWindow"}, // 24 h + 1 s -> day 2
        {"2026-10-22T12:00:00Z", 2, "LateWithinWindow"}, // exactly 48 h
        {"2026-10-22T12:00:01Z", 3, "LateWithinWindow"}, // 48 h + 1 s -> day 3
        {"2026-10-23T12:00:00Z", 3, "LateWithinWindow"}, // exactly 72 h: last moment inside the window
        {"2026-10-23T12:00:01Z", 4, "BeyondWindow"},     // 72 h + 1 s: after the three-day window
        {"2027-10-20T12:00:00Z", 365, "BeyondWindow"},   // a year late
    };
    for (const auto& row : rows) {
        const int a = newAssignment();
        const int s = submitAt(row.when, a, student_);
        EXPECT_EQ(latenessOf(s), row.days) << row.when;
        EXPECT_EQ(lateStatusOf(s), row.status) << row.when;
    }
}

TEST_F(Submission, T_SUB_02_timestamp_and_deadline_snapshot_are_recorded) {
    const int a = newAssignment();
    const int s = submitAt("2026-10-19T08:30:15Z", a, student_);
    EXPECT_EQ(scalarText("SELECT submitted_at FROM submission_versions WHERE submission_id=" + std::to_string(s)),
              "2026-10-19T08:30:15Z");
    EXPECT_EQ(scalarText("SELECT deadline_used FROM submission_versions WHERE submission_id=" + std::to_string(s)),
              kDeadline);
    EXPECT_EQ(scalar("SELECT version_no FROM submission_versions WHERE submission_id=" + std::to_string(s)), 1);
    EXPECT_EQ(scalar("SELECT student_id FROM submissions WHERE submission_id=" + std::to_string(s)), studentId_);
}

TEST_F(Submission, T_SUB_03_invalid_input_creates_no_record) {
    const int a = newAssignment();
    EXPECT_FAILS_WITH(svc().submit(student_, a, ""), "Submission content reference must not be empty");
    EXPECT_FAILS_WITH(svc().submit(student_, a, "   "), "Submission content reference must not be empty");
    EXPECT_FAILS_WITH(svc().submit(student_, 9999, "ref"), "Assignment not found or inactive");
    now_ = "2026-02-31T12:00:00Z"; // system clock giving an impossible date
    EXPECT_FAILS_WITH(svc().submit(student_, a, "ref"), "System submission timestamp");
    EXPECT_EQ(scalar("SELECT count(*) FROM submissions"), 0);
    EXPECT_EQ(scalar("SELECT count(*) FROM submission_versions"), 0);
}

TEST_F(Submission, T_SUB_04_each_student_has_an_independent_submission) {
    const int a = newAssignment();
    const int s1 = submitAt(kBeforeDeadline, a, student_);
    const int s2 = submitAt(kBeforeDeadline, a, student2_);
    EXPECT_NE(s1, s2);
    EXPECT_EQ(scalar("SELECT count(*) FROM submissions WHERE assignment_id=" + std::to_string(a)), 2);
}

// ---- T-RESUB ----

TEST_F(Submission, T_RESUB_01_resubmission_before_the_deadline_creates_a_new_current_version) {
    const int a = newAssignment();
    const int first = submitAt("2026-10-18T10:00:00Z", a, student_);
    const int second = submitAt("2026-10-19T10:00:00Z", a, student_);
    EXPECT_EQ(first, second) << "same submission record";
    const std::string id = std::to_string(first);
    EXPECT_EQ(scalar("SELECT count(*) FROM submission_versions WHERE submission_id=" + id), 2);
    EXPECT_EQ(scalar("SELECT sum(is_current) FROM submission_versions WHERE submission_id=" + id), 1);
    EXPECT_EQ(scalar("SELECT version_no FROM submission_versions WHERE is_current=1 AND submission_id=" + id), 2);
    EXPECT_EQ(scalar("SELECT current_version_no FROM submissions WHERE submission_id=" + id), 2);
    EXPECT_EQ(scalar("SELECT is_current FROM submission_versions WHERE version_no=1 AND submission_id=" + id),
              0); // kept
}

TEST_F(Submission, T_RESUB_02_the_latest_version_is_the_one_marked) {
    const int a = newAssignment();
    submitAt("2026-10-18T10:00:00Z", a, student_);
    const int s = submitAt("2026-10-19T10:00:00Z", a, student_);
    const auto queue = svc().submissionsForMarking(lecturer_); // before marking: the queue shows the current version
    ASSERT_EQ(queue.size(), 1U);
    EXPECT_EQ(queue[0].versionNo, 2);
    const int mark = svc().markSubmission(lecturer_, s, 60, "fb", "initial");
    const int currentVersion =
        scalar("SELECT version_id FROM submission_versions WHERE is_current=1 AND submission_id=" + std::to_string(s));
    EXPECT_EQ(scalar("SELECT version_id FROM marks WHERE mark_id=" + std::to_string(mark)), currentVersion);
}

// Observation O-6 (docs/test/DEFECT_LOG.md): this pins what the code does today, it is not an approved rule.
// A resubmission after the work was marked but before the deadline leaves the mark tied to the earlier version
// and the work does not return to the lecturer queue. FR-10 says the latest valid version is the one marked.
TEST_F(Submission, T_RESUB_08_resubmission_after_marking_current_behaviour) {
    const int a = newAssignment();
    const int s = submitAt(kBeforeDeadline, a, student_);
    const int mark = svc().markSubmission(lecturer_, s, 60, "fb", "initial");
    const int v1 =
        scalar("SELECT version_id FROM submission_versions WHERE version_no=1 AND submission_id=" + std::to_string(s));
    submitAt("2026-10-19T13:00:00Z", a, student_);
    EXPECT_EQ(scalar("SELECT version_id FROM marks WHERE mark_id=" + std::to_string(mark)), v1);
    EXPECT_TRUE(svc().submissionsForMarking(lecturer_).empty());
    // A lecturer who knows the submission ID can re-mark, which moves the mark to the latest version.
    svc().markSubmission(lecturer_, s, 65, "fb", "marked latest version");
    const int current =
        scalar("SELECT version_id FROM submission_versions WHERE is_current=1 AND submission_id=" + std::to_string(s));
    EXPECT_EQ(scalar("SELECT version_id FROM marks WHERE mark_id=" + std::to_string(mark)), current);
}

TEST_F(Submission, T_RESUB_03_resubmission_exactly_at_the_deadline_is_allowed) {
    const int a = newAssignment();
    submitAt(kBeforeDeadline, a, student_);
    EXPECT_NO_THROW(submitAt(kDeadline, a, student_));
    EXPECT_EQ(scalar("SELECT count(*) FROM submission_versions"), 2);
}

TEST_F(Submission, T_RESUB_04_resubmission_after_the_deadline_is_rejected_and_changes_nothing) {
    const int a = newAssignment();
    const int s = submitAt(kBeforeDeadline, a, student_);
    now_ = "2026-10-20T12:00:01Z";
    EXPECT_FAILS_WITH(svc().submit(student_, a, "late-ref"), "only permitted before the applicable deadline");
    const std::string id = std::to_string(s);
    EXPECT_EQ(scalar("SELECT count(*) FROM submission_versions WHERE submission_id=" + id), 1);
    EXPECT_EQ(scalar("SELECT is_current FROM submission_versions WHERE submission_id=" + id), 1);
    EXPECT_EQ(scalar("SELECT current_version_no FROM submissions WHERE submission_id=" + id), 1);
    EXPECT_EQ(scalarText("SELECT content_ref FROM submission_versions WHERE submission_id=" + id), "content-ref");
}

TEST_F(Submission, T_RESUB_05_a_first_late_submission_is_accepted_but_not_a_late_resubmission) {
    const int a = newAssignment();
    const int s = submitAt("2026-10-21T12:00:00Z", a, student_); // first submission, one day late
    EXPECT_EQ(lateStatusOf(s), "LateWithinWindow");
    now_ = "2026-10-21T13:00:00Z";
    EXPECT_FAILS_WITH(svc().submit(student_, a, "again"), "only permitted before the applicable deadline");
    EXPECT_EQ(scalar("SELECT count(*) FROM submission_versions WHERE submission_id=" + std::to_string(s)), 1);
}

TEST_F(Submission, T_RESUB_06_resubmission_deadline_follows_the_individual_extension) {
    const int a = newAssignment();
    svc().grantIndividualExtension(lecturer_, a, studentId_, "2026-10-25T12:00:00Z", "Illness");
    submitAt("2026-10-19T10:00:00Z", a, student_);
    EXPECT_NO_THROW(submitAt("2026-10-24T10:00:00Z", a, student_)); // after the original, before the revised deadline
    now_ = "2026-10-25T12:00:01Z";
    EXPECT_FAILS_WITH(svc().submit(student_, a, "x"), "only permitted before the applicable deadline");
}

TEST_F(Submission, T_RESUB_07_database_failure_during_resubmission_keeps_the_previous_version_current) {
    const int a = newAssignment();
    const int s = submitAt("2026-10-18T10:00:00Z", a, student_);
    db().execute("CREATE TRIGGER fail_version BEFORE INSERT ON submission_versions "
                 "BEGIN SELECT RAISE(ABORT,'forced failure'); END;");
    now_ = "2026-10-19T10:00:00Z";
    EXPECT_FAILS_WITH(svc().submit(student_, a, "second"), "forced failure");
    db().execute("DROP TRIGGER fail_version;");
    const std::string id = std::to_string(s);
    EXPECT_EQ(scalar("SELECT count(*) FROM submission_versions WHERE submission_id=" + id), 1);
    EXPECT_EQ(scalar("SELECT is_current FROM submission_versions WHERE submission_id=" + id), 1);
    EXPECT_EQ(scalar("SELECT current_version_no FROM submissions WHERE submission_id=" + id), 1);
}

TEST_F(Submission, T_SUB_05_database_failure_on_first_submission_leaves_no_partial_record) {
    const int a = newAssignment();
    db().execute("CREATE TRIGGER fail_version BEFORE INSERT ON submission_versions "
                 "BEGIN SELECT RAISE(ABORT,'forced failure'); END;");
    EXPECT_FAILS_WITH(svc().submit(student_, a, "ref"), "forced failure");
    db().execute("DROP TRIGGER fail_version;");
    EXPECT_EQ(scalar("SELECT count(*) FROM submissions"), 0);
    EXPECT_EQ(scalar("SELECT count(*) FROM submission_versions"), 0);
    EXPECT_NO_THROW(svc().submit(student_, a, "ref")); // the system recovered
}

// ---- T-FILE: submitting real files (local copy, random server-generated name) ----

TEST_F(Submission, T_FILE_01_a_file_is_copied_to_storage_and_referenced) {
    const int a = newAssignment();
    const auto source = makeFile("report.txt", "my assignment text");
    const fs::path store = dir_ / "store";
    now_ = kBeforeDeadline;
    const int s = svc().submitFile(student_, a, source, store);
    const fs::path stored =
        scalarText("SELECT content_ref FROM submission_versions WHERE submission_id=" + std::to_string(s));
    ASSERT_TRUE(fs::is_regular_file(stored));
    EXPECT_TRUE(stored.is_absolute());
    EXPECT_EQ(stored.extension(), ".txt");
    EXPECT_EQ(stored.filename().string().rfind("submission_", 0), 0U);
    EXPECT_NE(stored.filename(), source.filename()) << "the student's file name is not reused";
    EXPECT_EQ(fs::file_size(stored), fs::file_size(source));
    EXPECT_TRUE(fs::exists(source)) << "the original is untouched";
}

TEST_F(Submission, T_FILE_02_allowed_extensions_are_case_insensitive) {
    const fs::path store = dir_ / "store";
    for (const char* name : {"a.PDF", "b.Docx", "c.doc", "d.TXT", "e.zip", "f.png", "g.JPG", "h.jpeg"}) {
        const int a = newAssignment(kDeadline, 100, name);
        EXPECT_NO_THROW(svc().submitFile(student_, a, makeFile(name, "x"), store)) << name;
    }
    EXPECT_EQ(filesIn(store), 8);
}

TEST_F(Submission, T_FILE_03_unsupported_or_missing_extension_is_rejected) {
    const int a = newAssignment();
    const fs::path store = dir_ / "store";
    for (const char* name : {"run.exe", "script.sh", "noextension", "archive.txt.exe", "image.gif", ".hidden"}) {
        SCOPED_TRACE(name);
        EXPECT_FAILS_WITH(svc().submitFile(student_, a, makeFile(name, "x"), store), "Unsupported file type");
    }
    EXPECT_EQ(scalar("SELECT count(*) FROM submission_versions"), 0);
    EXPECT_EQ(filesIn(store), 0);
}

TEST_F(Submission, T_FILE_04_missing_file_or_directory_is_rejected) {
    const int a = newAssignment();
    const fs::path store = dir_ / "store";
    EXPECT_FAILS_WITH(svc().submitFile(student_, a, dir_ / "nothing.txt", store), "existing regular file");
    EXPECT_FAILS_WITH(svc().submitFile(student_, a, dir_, store), "existing regular file");
    EXPECT_FAILS_WITH(svc().submitFile(student_, a, fs::path(""), store), "existing regular file");
    EXPECT_EQ(scalar("SELECT count(*) FROM submissions"), 0);
}

TEST_F(Submission, T_FILE_05_size_limits_empty_and_twenty_mebibytes) {
    const fs::path store = dir_ / "store";
    const int a1 = newAssignment(kDeadline, 100, "empty");
    EXPECT_FAILS_WITH(svc().submitFile(student_, a1, makeFile("empty.txt", ""), store), "between 1 byte and 20 MiB");
    const std::uintmax_t limit = 20U * 1024U * 1024U;
    auto sized = [&](const char* name, std::uintmax_t size) {
        const fs::path p = makeFile(name, "x");
        fs::resize_file(p, size);
        return p;
    };
    const int a2 = newAssignment(kDeadline, 100, "one byte");
    EXPECT_NO_THROW(svc().submitFile(student_, a2, sized("one.txt", 1), store));
    const int a3 = newAssignment(kDeadline, 100, "limit");
    EXPECT_NO_THROW(svc().submitFile(student_, a3, sized("limit.zip", limit), store));
    const int a4 = newAssignment(kDeadline, 100, "over");
    EXPECT_FAILS_WITH(svc().submitFile(student_, a4, sized("over.zip", limit + 1), store), "between 1 byte and 20 MiB");
}

TEST_F(Submission, T_FILE_06_a_failed_submission_removes_the_stored_copy) {
    const int a = newAssignment();
    const fs::path store = dir_ / "store";
    EXPECT_FAILS_WITH(svc().submitFile(outsider_, a, makeFile("w.txt", "x"), store), "not enrolled");
    EXPECT_EQ(filesIn(store), 0) << "no orphan file after a rejected submission";
    EXPECT_EQ(scalar("SELECT count(*) FROM submissions"), 0);
}

TEST_F(Submission, T_FILE_07_database_failure_removes_the_stored_copy) {
    const int a = newAssignment();
    const fs::path store = dir_ / "store";
    db().execute("CREATE TRIGGER fail_version BEFORE INSERT ON submission_versions "
                 "BEGIN SELECT RAISE(ABORT,'forced failure'); END;");
    EXPECT_FAILS_WITH(svc().submitFile(student_, a, makeFile("w.txt", "x"), store), "forced failure");
    EXPECT_EQ(filesIn(store), 0);
}

TEST_F(Submission, T_FILE_08_late_resubmission_by_file_leaves_no_new_file) {
    const int a = newAssignment();
    const fs::path store = dir_ / "store";
    svc().submitFile(student_, a, makeFile("w.txt", "x"), store);
    now_ = "2026-10-21T12:00:00Z";
    EXPECT_FAILS_WITH(svc().submitFile(student_, a, makeFile("w2.txt", "y"), store), "only permitted before");
    EXPECT_EQ(filesIn(store), 1);
}

TEST_F(Submission, T_FILE_09_each_resubmission_gets_its_own_stored_file) {
    const int a = newAssignment();
    const fs::path store = dir_ / "store";
    const int s = svc().submitFile(student_, a, makeFile("w.txt", "first"), store);
    svc().submitFile(student_, a, makeFile("w2.txt", "second"), store);
    EXPECT_EQ(filesIn(store), 2);
    EXPECT_EQ(
        scalar("SELECT count(DISTINCT content_ref) FROM submission_versions WHERE submission_id=" + std::to_string(s)),
        2);
}

TEST_F(Submission, T_FILE_10_storage_directory_that_cannot_be_created_is_reported) {
    const int a = newAssignment();
    const auto blocker = makeFile("blocker", "i am a file, not a directory");
    EXPECT_ANY_THROW(svc().submitFile(student_, a, makeFile("w.txt", "x"), blocker / "store"));
    EXPECT_EQ(scalar("SELECT count(*) FROM submissions"), 0);
}

TEST_F(Submission, T_FILE_11_unreadable_source_file_is_reported) {
    const int a = newAssignment();
    const auto source = makeFile("secret.txt", "x");
    fs::permissions(source, fs::perms::none);
    bool rejected = false;
    try {
        svc().submitFile(student_, a, source, dir_ / "store");
    } catch (const std::exception&) {
        rejected = true;
    }
    fs::permissions(source, fs::perms::owner_all);
    if (!rejected)
        GTEST_SKIP() << "running with privileges that bypass file permissions (for example root)";
    EXPECT_EQ(scalar("SELECT count(*) FROM submissions"), 0);
}
