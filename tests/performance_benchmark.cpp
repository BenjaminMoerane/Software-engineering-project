#include "data/Database.hpp"
#include "security/PasswordHasher.hpp"
#include "services/LmsService.hpp"
#include <chrono>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>

using Clock = std::chrono::steady_clock;
static double milliseconds(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}
static void checkSql(int rc, sqlite3* db, const char* action) {
    if (rc != SQLITE_OK) throw std::runtime_error(std::string(action) + ": " + sqlite3_errmsg(db));
}
int main() {
    const std::string path = "lms_performance_benchmark.sqlite3";
    std::remove(path.c_str());
    try {
        lms::data::Database db(path);
        db.initialiseSchema();
        db.execute("INSERT INTO departments(name) VALUES ('Benchmark Department')");
        const int department = static_cast<int>(sqlite3_last_insert_rowid(db.handle()));
        const auto hash = lms::security::PasswordHasher::hash("BenchmarkPassword1!");
        {
            lms::data::Transaction tx(db.handle());
            sqlite3_stmt* raw = nullptr;
            checkSql(sqlite3_prepare_v2(db.handle(), "INSERT INTO users(username,password_hash,password_salt,role,full_name,department_id) VALUES(?1,?2,?3,'Student',?4,?5)", -1, &raw, nullptr), db.handle(), "Prepare user seed");
            try {
                for (int i = 0; i < 800; ++i) {
                    sqlite3_bind_text(raw, 1, ("student" + std::to_string(i)).c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(raw, 2, hash.hashHex.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(raw, 3, hash.saltHex.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(raw, 4, ("Benchmark Student " + std::to_string(i)).c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int(raw, 5, department);
                    if (sqlite3_step(raw) != SQLITE_DONE) throw std::runtime_error(std::string("Seed user: ") + sqlite3_errmsg(db.handle()));
                    sqlite3_reset(raw); sqlite3_clear_bindings(raw);
                }
                sqlite3_finalize(raw);
            } catch (...) { sqlite3_finalize(raw); throw; }
            tx.commit();
        }
        {
            lms::data::Statement lecturer(db.handle(), "INSERT INTO users(username,password_hash,password_salt,role,full_name,department_id) VALUES('benchlecturer',?1,?2,'Lecturer','Benchmark Lecturer',?3)");
            lecturer.bind(1, hash.hashHex); lecturer.bind(2, hash.saltHex); lecturer.bind(3, department); lecturer.execute();
        }
        const int lecturerId = static_cast<int>(sqlite3_last_insert_rowid(db.handle()));
        lms::data::Statement courseInsert(db.handle(), "INSERT INTO courses(department_id,course_code,title) VALUES(?1,'BENCH101','Benchmark Course')");
        courseInsert.bind(1, department); courseInsert.execute();
        const int course = static_cast<int>(sqlite3_last_insert_rowid(db.handle()));
        lms::data::Statement assignmentLecturer(db.handle(), "INSERT INTO course_lecturers(course_id,lecturer_id) VALUES(?1,?2)");
        assignmentLecturer.bind(1, course); assignmentLecturer.bind(2, lecturerId); assignmentLecturer.execute();
        lms::data::Statement enrol(db.handle(), "INSERT INTO enrolments(course_id,student_id) VALUES(?1,1)");
        enrol.bind(1, course); enrol.execute();
        {
            lms::data::Transaction tx(db.handle());
            sqlite3_stmt* raw = nullptr;
            checkSql(sqlite3_prepare_v2(db.handle(), "INSERT INTO assignments(course_id,created_by,title,description,deadline,max_mark) VALUES(?1,?3,?2,'Benchmark row','2026-11-01T12:00:00Z',100)", -1, &raw, nullptr), db.handle(), "Prepare assignment seed");
            try {
                for (int i = 0; i < 100000; ++i) {
                    const std::string title = "Assignment " + std::to_string(i);
                    sqlite3_bind_int(raw, 1, course);
                    sqlite3_bind_text(raw, 2, title.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int(raw, 3, lecturerId);
                    if (sqlite3_step(raw) != SQLITE_DONE) throw std::runtime_error(std::string("Seed assignment: ") + sqlite3_errmsg(db.handle()));
                    sqlite3_reset(raw); sqlite3_clear_bindings(raw);
                }
                sqlite3_finalize(raw);
            } catch (...) { sqlite3_finalize(raw); throw; }
            tx.commit();
        }
        lms::services::LmsService service(db);
        const auto loginStart = Clock::now();
        auto session = service.login("student0", "BenchmarkPassword1!");
        const auto loginEnd = Clock::now();
        const auto listStart = Clock::now();
        auto assignments = service.listAssignments(session);
        const auto listEnd = Clock::now();
        const double loginMs = milliseconds(loginStart, loginEnd);
        const double listMs = milliseconds(listStart, listEnd);
        std::cout << "Environment-specific synthetic benchmark (not target-school acceptance evidence)\n"
                  << "students_stored=800\ntotal_users_stored=801\nassignments_stored=100000\n"
                  << "login_ms=" << loginMs << "\nassignment_list_ms=" << listMs
                  << "\nassignments_returned=" << assignments.size() << '\n';
        if (assignments.size() != 100000) throw std::runtime_error("Expected 100000 assignment rows returned");
        std::remove(path.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::remove(path.c_str());
        std::cerr << "BENCHMARK FAILED: " << error.what() << '\n';
        return 1;
    }
}
