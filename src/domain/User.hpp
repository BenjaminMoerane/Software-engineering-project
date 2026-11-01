#pragma once
#include <memory>
#include <stdexcept>
#include <string>
namespace lms::domain {
enum class Role { Administrator, Lecturer, HeadOfDepartment, Student };
inline std::string roleName(Role r) {
    switch (r) {
    case Role::Administrator:
        return "Administrator";
    case Role::Lecturer:
        return "Lecturer";
    case Role::HeadOfDepartment:
        return "HeadOfDepartment";
    case Role::Student:
        return "Student";
    }
    throw std::logic_error("Unknown role");
}
inline Role parseRole(const std::string& s) {
    if (s == "Administrator")
        return Role::Administrator;
    if (s == "Lecturer")
        return Role::Lecturer;
    if (s == "HeadOfDepartment")
        return Role::HeadOfDepartment;
    if (s == "Student")
        return Role::Student;
    throw std::invalid_argument("Invalid role");
}
struct Session {
    int userId{};
    std::string username;
    std::string fullName;
    Role role{Role::Student};
    int departmentId{};
};
class User {
  public:
    explicit User(Session s) : session_(std::move(s)) {}
    virtual ~User() = default;
    const Session& session() const noexcept { return session_; }
    virtual bool canAdminister() const noexcept = 0;
    virtual bool canTeach() const noexcept = 0;
    virtual bool canModerate() const noexcept = 0;
    virtual bool canSubmit() const noexcept = 0;

  private:
    Session session_;
};
class Administrator final : public User {
  public:
    using User::User;
    bool canAdminister() const noexcept override { return true; }
    bool canTeach() const noexcept override { return false; }
    bool canModerate() const noexcept override { return false; }
    bool canSubmit() const noexcept override { return false; }
};
class Lecturer final : public User {
  public:
    using User::User;
    bool canAdminister() const noexcept override { return false; }
    bool canTeach() const noexcept override { return true; }
    bool canModerate() const noexcept override { return false; }
    bool canSubmit() const noexcept override { return false; }
};
class HeadOfDepartment final : public User {
  public:
    using User::User;
    bool canAdminister() const noexcept override { return false; }
    bool canTeach() const noexcept override { return false; }
    bool canModerate() const noexcept override { return true; }
    bool canSubmit() const noexcept override { return false; }
};
class Student final : public User {
  public:
    using User::User;
    bool canAdminister() const noexcept override { return false; }
    bool canTeach() const noexcept override { return false; }
    bool canModerate() const noexcept override { return false; }
    bool canSubmit() const noexcept override { return true; }
};
inline std::unique_ptr<User> makeUser(Session s) {
    switch (s.role) {
    case Role::Administrator:
        return std::make_unique<Administrator>(std::move(s));
    case Role::Lecturer:
        return std::make_unique<Lecturer>(std::move(s));
    case Role::HeadOfDepartment:
        return std::make_unique<HeadOfDepartment>(std::move(s));
    case Role::Student:
        return std::make_unique<Student>(std::move(s));
    }
    throw std::logic_error("Unknown role");
}
} // namespace lms::domain
