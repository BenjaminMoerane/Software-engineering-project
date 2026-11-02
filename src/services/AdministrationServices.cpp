// UserAdministrationService and CourseService (Administrator use cases).
#include "security/PasswordHasher.hpp"
#include "services/Services.hpp"
#include "services/Validation.hpp"

#include <stdexcept>

namespace lms::services {

int UserAdministrationService::createUser(const Session& s, const std::string& username, const std::string& password,
                                          const std::string& fullName, Role role, int departmentId) {
    authz_.requireRole(s, Role::Administrator);
    validation::nonblank(username, "Username");
    validation::nonblank(password, "Password");
    validation::nonblank(fullName, "Full name");
    validation::minimumPasswordLength(password);
    if (role == Role::HeadOfDepartment && departmentId <= 0)
        throw std::invalid_argument("HeadOfDepartment must belong to a department");
    if (role != Role::HeadOfDepartment && departmentId < 0)
        throw std::invalid_argument("Invalid department ID");
    const auto h = security::PasswordHasher::hash(password);
    return users_.insert({username, h.hashHex, h.saltHex, domain::roleName(role), fullName, departmentId});
}

void UserAdministrationService::updateUserAccount(const Session& s, int userId, const std::string& fullName, Role role,
                                                  int departmentId) {
    authz_.requireRole(s, Role::Administrator);
    validation::nonblank(fullName, "Full name");
    if (role == Role::HeadOfDepartment && departmentId <= 0)
        throw std::invalid_argument("HeadOfDepartment must belong to a department");
    if (departmentId < 0)
        throw std::invalid_argument("Invalid department ID");
    if (!users_.update(userId, fullName, domain::roleName(role), departmentId))
        throw std::runtime_error("User account not found");
}

void UserAdministrationService::deactivateUser(const Session& s, int userId) {
    authz_.requireRole(s, Role::Administrator);
    if (userId == s.userId)
        throw std::invalid_argument("Administrator cannot deactivate the currently authenticated account");
    if (!users_.deactivate(userId))
        throw std::runtime_error("Active user account not found");
}

int CourseService::createDepartment(const Session& s, const std::string& name) {
    authz_.requireRole(s, Role::Administrator);
    validation::nonblank(name, "Department name");
    return courses_.insertDepartment(name);
}

void CourseService::updateDepartment(const Session& s, int departmentId, const std::string& name) {
    authz_.requireRole(s, Role::Administrator);
    validation::nonblank(name, "Department name");
    if (!courses_.updateDepartment(departmentId, name))
        throw std::runtime_error("Department not found");
}

std::vector<std::string> CourseService::listDepartments() const {
    std::vector<std::string> out;
    for (const auto& [id, name] : courses_.listDepartments())
        out.push_back(std::to_string(id) + " | " + name);
    return out;
}

int CourseService::createCourse(const Session& s, int departmentId, const std::string& code,
                                const std::string& title) {
    authz_.requireRole(s, Role::Administrator);
    validation::nonblank(code, "Course code");
    validation::nonblank(title, "Course title");
    return courses_.insertCourse(departmentId, code, title);
}

void CourseService::updateCourse(const Session& s, int courseId, const std::string& code, const std::string& title,
                                 int departmentId) {
    authz_.requireRole(s, Role::Administrator);
    validation::nonblank(code, "Course code");
    validation::nonblank(title, "Course title");
    if (!courses_.updateCourse(courseId, code, title, departmentId))
        throw std::runtime_error("Course not found");
}

void CourseService::requireActiveTarget(int userId, const char* role, const char* message) const {
    const auto target = users_.findAccountState(userId);
    if (!target || target->role != role || !target->active)
        throw std::invalid_argument(message);
}

void CourseService::assignLecturer(const Session& s, int courseId, int lecturerId) {
    authz_.requireRole(s, Role::Administrator);
    requireActiveTarget(lecturerId, "Lecturer", "Target account must be an active Lecturer");
    courses_.assignLecturer(courseId, lecturerId);
}

void CourseService::enrolStudent(const Session& s, int courseId, int studentId) {
    authz_.requireRole(s, Role::Administrator);
    requireActiveTarget(studentId, "Student", "Target account must be an active Student");
    courses_.enrolStudent(courseId, studentId);
}

} // namespace lms::services
