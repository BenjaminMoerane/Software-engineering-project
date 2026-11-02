#pragma once
#include <sqlite3.h>
#include <string>

namespace lms::data {
class Statement {
  public:
    Statement(sqlite3* db, const std::string& sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;
    void bind(int index, const std::string& value);
    void bind(int index, const char* value);
    void bind(int index, int value);
    void bind(int index, double value);
    void bindNull(int index);
    bool step(); // true for a row, false for DONE
    void execute();
    int integer(int column) const;
    double real(int column) const;
    std::string text(int column) const;
    bool isNull(int column) const;

  private:
    sqlite3* db_{};
    sqlite3_stmt* stmt_{};
};
class Transaction {
  public:
    explicit Transaction(sqlite3* db);
    ~Transaction();
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction(Transaction&&) = delete;
    Transaction& operator=(Transaction&&) = delete;
    void commit();

  private:
    sqlite3* db_{};
    bool committed_{false};
};
class Database {
  public:
    explicit Database(const std::string& path);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;
    sqlite3* handle() const noexcept;
    void execute(const std::string& sql) const;
    void initialiseSchema() const;
    bool foreignKeysEnabled() const;

  private:
    sqlite3* db_{nullptr};
};
} // namespace lms::data
