#include "data/Database.hpp"
#include "security/PasswordHasher.hpp"
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
}
int main() {
    const std::string path = "lms_smoke_test.sqlite3";
    std::remove(path.c_str());
    try {
        {
            lms::data::Database database(path);
            require(database.foreignKeysEnabled(), "foreign keys should be enabled");
            database.initialiseSchema();
            database.execute("INSERT INTO departments(name) VALUES ('Computer Science');");
            bool rejected = false;
            try { database.execute("INSERT INTO courses(department_id, course_code, title) VALUES (999, 'BAD1', 'Invalid FK');"); }
            catch (const std::runtime_error&) { rejected = true; }
            require(rejected, "invalid foreign key should be rejected");
        }
        {
            lms::data::Database reopened(path);
            sqlite3_stmt* statement = nullptr;
            require(sqlite3_prepare_v2(reopened.handle(), "SELECT count(*) FROM departments WHERE name=?1;", -1, &statement, nullptr) == SQLITE_OK,
                    "prepare persistence query");
            sqlite3_bind_text(statement, 1, "Computer Science", -1, SQLITE_STATIC);
            require(sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int(statement, 0) == 1,
                    "data should persist after reopening database");
            sqlite3_finalize(statement);
        }
        const auto first = lms::security::PasswordHasher::hash("example-passphrase");
        const auto second = lms::security::PasswordHasher::hash("example-passphrase");
        require(first.saltHex != second.saltHex, "each hash should use a distinct random salt");
        require(lms::security::PasswordHasher::verify("example-passphrase", first), "correct password should verify");
        require(!lms::security::PasswordHasher::verify("wrong-password", first), "wrong password should not verify");
        std::remove(path.c_str());
        std::cout << "PASS: schema initialisation, foreign keys, persistence, salted scrypt hash verification\n";
        return 0;
    } catch (const std::exception& error) {
        std::remove(path.c_str());
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
