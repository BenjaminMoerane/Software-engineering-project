// T-PERSIST / T-DB-ERROR: persistence across restarts, foreign keys, and the data-access layer's error handling
// (FR-19, FR-20; Database, Statement and Transaction classes).
#include "TestSupport.hpp"

#include <utility>

using namespace lmstest;
using lms::data::Database;
using lms::data::Statement;
using lms::data::Transaction;

class Persistence : public LmsFixture {};

TEST_F(Persistence, T_PERSIST_01_a_full_workflow_survives_a_restart) {
    buildWorld();
    const int a = newAssignment();
    const int s = submitAt(kBeforeDeadline, a, student_);
    const int mark = markApproveRelease(s, 77.0);
    reopen();
    // every record is still there and usable
    const Session student = svc().login("student", kUserPassword);
    const auto results = svc().releasedResults(student);
    ASSERT_EQ(results.size(), 1U);
    EXPECT_DOUBLE_EQ(results[0].mark, 77.0);
    EXPECT_EQ(results[0].feedback, "Feedback");
    EXPECT_EQ(scalar("SELECT count(*) FROM mark_audit WHERE mark_id=" + std::to_string(mark)), 1);
    EXPECT_EQ(scalar("SELECT count(*) FROM submission_versions"), 1);
    EXPECT_EQ(scalar("SELECT count(*) FROM enrolments"), 2);
    EXPECT_EQ(scalar("SELECT count(*) FROM users"), 8);
}

TEST_F(Persistence, T_PERSIST_02_schema_initialisation_is_repeatable_and_non_destructive) {
    buildWorld();
    const int users = scalar("SELECT count(*) FROM users");
    db().initialiseSchema();
    db().initialiseSchema();
    reopen();
    EXPECT_EQ(scalar("SELECT count(*) FROM users"), users);
    EXPECT_EQ(scalar("SELECT count(*) FROM departments"), 2);
}

TEST_F(Persistence, T_PERSIST_03_passwords_still_verify_after_a_restart) {
    buildWorld();
    reopen();
    EXPECT_NO_THROW(svc().login("lecturer", kUserPassword));
    EXPECT_NO_THROW(svc().login("admin", kAdminPassword));
}

TEST_F(Persistence, T_PERSIST_04_the_database_passes_sqlite_integrity_and_foreign_key_checks) {
    buildWorld();
    const int a = newAssignment();
    markApproveRelease(submitAt(kBeforeDeadline, a, student_));
    EXPECT_EQ(scalarText("PRAGMA integrity_check"), "ok");
    Statement fk(db().handle(), "PRAGMA foreign_key_check");
    EXPECT_FALSE(fk.step()) << "no foreign-key violations";
}

TEST_F(Persistence, T_PERSIST_05_every_table_of_the_phase_3_design_exists) {
    const char* tables[] = {
        "users",       "departments",          "courses",         "course_lecturers",    "enrolments",
        "assignments", "extensions",           "submissions",     "submission_versions", "marks",
        "feedback",    "moderation_decisions", "release_records", "mark_audit"};
    for (const char* t : tables)
        EXPECT_EQ(scalar(std::string("SELECT count(*) FROM sqlite_master WHERE type='table' AND name='") + t + "'"), 1)
            << t;
}

// ---- T-DB-ERROR ----

TEST_F(Persistence, T_DB_ERROR_01_foreign_keys_are_enforced) {
    EXPECT_TRUE(db().foreignKeysEnabled());
    EXPECT_ANY_THROW(db().execute("INSERT INTO courses(department_id,course_code,title) VALUES (999,'BAD1','FK')"));
}

TEST_F(Persistence, T_DB_ERROR_02_check_constraints_reject_bad_rows) {
    db().execute("INSERT INTO departments(name) VALUES ('D')");
    EXPECT_ANY_THROW(db().execute("INSERT INTO departments(name) VALUES ('   ')"));
    EXPECT_ANY_THROW(db().execute("INSERT INTO departments(name) VALUES ('D')")); // unique
    EXPECT_ANY_THROW(db().execute("INSERT INTO users(username,password_hash,password_salt,role,full_name) "
                                  "VALUES ('u','h','s','Principal','N')"));
    EXPECT_ANY_THROW(db().execute("INSERT INTO users(username,password_hash,password_salt,role,full_name) "
                                  "VALUES ('u','h','s','HeadOfDepartment','N')")); // HoD needs a department
}

TEST_F(Persistence, T_DB_ERROR_03_the_service_stays_usable_after_a_failed_operation) {
    buildWorld();
    EXPECT_ANY_THROW(svc().createDepartment(admin_, "Science")); // duplicate
    EXPECT_NO_THROW(svc().createDepartment(admin_, "History"));
    EXPECT_EQ(scalar("SELECT count(*) FROM departments"), 3);
}

TEST(DataAccess, T_DB_ERROR_04_unopenable_databases_are_reported_not_crashed) {
    EXPECT_ANY_THROW(Database("/nonexistent_directory_for_lms_tests/x.sqlite3"));
    EXPECT_ANY_THROW(Database(fs::temp_directory_path().string())); // a directory
}

TEST(DataAccess, T_DB_ERROR_05_a_file_that_is_not_a_database_is_reported) {
    const fs::path p = fs::temp_directory_path() / "lms_not_a_database.sqlite3";
    {
        std::ofstream out(p);
        out << "this is plain text, definitely not a SQLite database file.....................................";
    }
    EXPECT_ANY_THROW({
        Database db(p.string());
        db.initialiseSchema();
    });
    std::error_code ec;
    fs::remove(p, ec);
}

class RawDb : public ::testing::Test {
  protected:
    void SetUp() override {
        path_ = (fs::temp_directory_path() /
                 ("lms_raw_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
                  std::to_string(counter_++) + ".sqlite3"))
                    .string();
        db_ = std::make_unique<Database>(path_);
        db_->execute("CREATE TABLE t(id INTEGER PRIMARY KEY, n INTEGER, x REAL, s TEXT)");
    }
    void TearDown() override {
        db_.reset();
        std::error_code ec;
        for (const char* suffix : {"", "-wal", "-shm"})
            fs::remove(path_ + suffix, ec);
    }
    static inline int counter_ = 0;
    std::string path_;
    std::unique_ptr<Database> db_;
};

TEST_F(RawDb, T_DB_ERROR_06_statement_binds_all_types_and_reads_them_back) {
    Statement ins(db_->handle(), "INSERT INTO t(n,x,s) VALUES(?1,?2,?3)");
    ins.bind(1, 42);
    ins.bind(2, 2.5);
    ins.bind(3, std::string("text with 'quotes' and ; semicolons"));
    ins.execute();
    Statement sel(db_->handle(), "SELECT n,x,s FROM t");
    ASSERT_TRUE(sel.step());
    EXPECT_EQ(sel.integer(0), 42);
    EXPECT_DOUBLE_EQ(sel.real(1), 2.5);
    EXPECT_EQ(sel.text(2), "text with 'quotes' and ; semicolons");
    EXPECT_FALSE(sel.step());
}

TEST_F(RawDb, T_DB_ERROR_07_null_handling_and_const_char_binding) {
    Statement ins(db_->handle(), "INSERT INTO t(n,s) VALUES(?1,?2)");
    ins.bindNull(1);
    ins.bind(2, static_cast<const char*>(nullptr)); // a null C string binds NULL
    ins.execute();
    Statement ins2(db_->handle(), "INSERT INTO t(n,s) VALUES(?1,?2)");
    ins2.bind(1, 7);
    ins2.bind(2, "literal");
    ins2.execute();
    Statement sel(db_->handle(), "SELECT n,s FROM t ORDER BY id");
    ASSERT_TRUE(sel.step());
    EXPECT_TRUE(sel.isNull(0));
    EXPECT_TRUE(sel.isNull(1));
    EXPECT_EQ(sel.text(1), "") << "NULL text reads as an empty string";
    ASSERT_TRUE(sel.step());
    EXPECT_FALSE(sel.isNull(0));
    EXPECT_EQ(sel.text(1), "literal");
}

TEST_F(RawDb, T_DB_ERROR_08_sql_injection_text_is_stored_as_data) {
    const std::string evil = "x'); DROP TABLE t;--";
    Statement ins(db_->handle(), "INSERT INTO t(s) VALUES(?1)");
    ins.bind(1, evil);
    ins.execute();
    Statement sel(db_->handle(), "SELECT s FROM t");
    ASSERT_TRUE(sel.step());
    EXPECT_EQ(sel.text(0), evil);
}

TEST_F(RawDb, T_DB_ERROR_09_invalid_sql_and_bad_bind_indexes_throw) {
    EXPECT_ANY_THROW(Statement(db_->handle(), "SELEKT nonsense"));
    EXPECT_ANY_THROW(Statement(db_->handle(), "SELECT * FROM missing_table"));
    Statement st(db_->handle(), "INSERT INTO t(n) VALUES(?1)");
    EXPECT_ANY_THROW(st.bind(5, 1)); // there is only one parameter
    EXPECT_ANY_THROW(db_->execute("NOT SQL AT ALL"));
}

TEST_F(RawDb, T_DB_ERROR_10_execute_rejects_statements_that_return_rows) {
    db_->execute("INSERT INTO t(n) VALUES(1)");
    Statement sel(db_->handle(), "SELECT n FROM t");
    EXPECT_FAILS_WITH(sel.execute(), "unexpectedly returned rows");
}

TEST_F(RawDb, T_DB_ERROR_11_statements_can_be_moved) {
    Statement a(db_->handle(), "SELECT 41+1");
    Statement b(std::move(a));
    ASSERT_TRUE(b.step());
    EXPECT_EQ(b.integer(0), 42);
    Statement c(db_->handle(), "SELECT 7");
    c = std::move(b);
    EXPECT_EQ(c.integer(0), 42); // c now holds b's (already stepped) statement
}

TEST_F(RawDb, T_DB_ERROR_12_transaction_commits_and_rolls_back) {
    {
        Transaction tx(db_->handle());
        db_->execute("INSERT INTO t(n) VALUES(1)");
        tx.commit();
    }
    {
        Transaction tx(db_->handle()); // leaves scope without commit -> rolled back
        db_->execute("INSERT INTO t(n) VALUES(2)");
    }
    Statement count(db_->handle(), "SELECT count(*) FROM t");
    ASSERT_TRUE(count.step());
    EXPECT_EQ(count.integer(0), 1);
}

TEST_F(RawDb, T_DB_ERROR_13_nested_transactions_are_reported) {
    Transaction outer(db_->handle());
    EXPECT_FAILS_WITH(Transaction(db_->handle()), "Begin transaction");
}

TEST_F(RawDb, T_DB_ERROR_14_database_objects_can_be_moved) {
    Database moved(std::move(*db_));
    EXPECT_EQ(moved.handle() != nullptr, true);
    EXPECT_EQ(db_->handle(), nullptr);
    Database other(path_ + ".other");
    other = std::move(moved);
    EXPECT_NE(other.handle(), nullptr);
    EXPECT_TRUE(other.foreignKeysEnabled());
    std::error_code ec;
    for (const char* suffix : {".other", ".other-wal", ".other-shm"})
        fs::remove(path_ + suffix, ec);
}
