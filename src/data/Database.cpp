#include "data/Database.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

#ifndef LMS_SCHEMA_PATH
#define LMS_SCHEMA_PATH "database_schema.sql"
#endif
namespace lms::data {
namespace {
std::string errorMessage(sqlite3* db, const std::string& action) {
    return action + ": " + (db ? sqlite3_errmsg(db) : "SQLite database is unavailable");
}
void check(int result, sqlite3* db, const std::string& action) {
    if (result != SQLITE_OK)
        throw std::runtime_error(errorMessage(db, action));
}
} // namespace
Statement::Statement(sqlite3* db, const std::string& sql) : db_(db) {
    check(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr), db, "Prepare SQL");
}
Statement::~Statement() {
    if (stmt_)
        sqlite3_finalize(stmt_);
}
Statement::Statement(Statement&& other) noexcept : db_(other.db_), stmt_(std::exchange(other.stmt_, nullptr)) {}
Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        if (stmt_)
            sqlite3_finalize(stmt_);
        db_ = other.db_;
        stmt_ = std::exchange(other.stmt_, nullptr);
    }
    return *this;
}
void Statement::bind(int i, const std::string& v) {
    check(sqlite3_bind_text(stmt_, i, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT), db_, "Bind text");
}
void Statement::bind(int i, const char* v) {
    if (v)
        check(sqlite3_bind_text(stmt_, i, v, -1, SQLITE_TRANSIENT), db_, "Bind text");
    else
        bindNull(i);
}
void Statement::bind(int i, int v) {
    check(sqlite3_bind_int(stmt_, i, v), db_, "Bind integer");
}
void Statement::bind(int i, double v) {
    check(sqlite3_bind_double(stmt_, i, v), db_, "Bind number");
}
void Statement::bindNull(int i) {
    check(sqlite3_bind_null(stmt_, i), db_, "Bind NULL");
}
bool Statement::step() {
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW)
        return true;
    if (rc == SQLITE_DONE)
        return false;
    throw std::runtime_error(errorMessage(db_, "Execute query"));
}
void Statement::execute() {
    if (step())
        throw std::runtime_error("SQL command unexpectedly returned rows");
}
int Statement::integer(int c) const {
    return sqlite3_column_int(stmt_, c);
}
double Statement::real(int c) const {
    return sqlite3_column_double(stmt_, c);
}
std::string Statement::text(int c) const {
    const auto* p = sqlite3_column_text(stmt_, c);
    return p ? reinterpret_cast<const char*>(p) : "";
}
bool Statement::isNull(int c) const {
    return sqlite3_column_type(stmt_, c) == SQLITE_NULL;
}
Transaction::Transaction(sqlite3* db) : db_(db) {
    char* e = nullptr;
    if (sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, &e) != SQLITE_OK) {
        std::string m = e ? e : sqlite3_errmsg(db_);
        sqlite3_free(e);
        throw std::runtime_error("Begin transaction: " + m);
    }
}
Transaction::~Transaction() {
    if (!committed_ && db_)
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
}
void Transaction::commit() {
    char* e = nullptr;
    if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &e) != SQLITE_OK) {
        std::string m = e ? e : sqlite3_errmsg(db_);
        sqlite3_free(e);
        throw std::runtime_error("Commit transaction: " + m);
    }
    committed_ = true;
}
Database::Database(const std::string& path) {
    const int rc = sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        auto m = errorMessage(db_, "Could not open database");
        if (db_)
            sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error(m);
    }
    try {
        execute("PRAGMA foreign_keys=ON;");
        if (!foreignKeysEnabled())
            throw std::runtime_error("SQLite foreign-key enforcement could not be enabled");
        execute("PRAGMA busy_timeout=5000;");
        execute("PRAGMA journal_mode=WAL;");
    } catch (...) {
        sqlite3_close(db_);
        db_ = nullptr;
        throw;
    }
}
Database::~Database() {
    if (db_)
        sqlite3_close(db_);
}
Database::Database(Database&& o) noexcept : db_(std::exchange(o.db_, nullptr)) {}
Database& Database::operator=(Database&& o) noexcept {
    if (this != &o) {
        if (db_)
            sqlite3_close(db_);
        db_ = std::exchange(o.db_, nullptr);
    }
    return *this;
}
sqlite3* Database::handle() const noexcept {
    return db_;
}
void Database::execute(const std::string& sql) const {
    char* e = nullptr;
    const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &e);
    if (rc != SQLITE_OK) {
        std::string d = e ? e : sqlite3_errmsg(db_);
        sqlite3_free(e);
        throw std::runtime_error("SQLite statement failed: " + d);
    }
}
void Database::initialiseSchema() const {
    std::ifstream in(LMS_SCHEMA_PATH);
    if (!in)
        throw std::runtime_error(std::string("Cannot read schema file: ") + LMS_SCHEMA_PATH);
    std::ostringstream s;
    s << in.rdbuf();
    execute(s.str());
}
bool Database::foreignKeysEnabled() const {
    Statement s(db_, "PRAGMA foreign_keys;");
    return s.step() && s.integer(0) == 1;
}
} // namespace lms::data
