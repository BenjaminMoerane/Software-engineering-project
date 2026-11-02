// AuthorizationService and AuthenticationService.
#include "security/PasswordHasher.hpp"
#include "services/Services.hpp"
#include "services/Validation.hpp"

#include <stdexcept>

namespace lms::services {

void AuthorizationService::requireRole(const Session& s, Role r) const {
    if (s.userId <= 0 || s.role != r)
        throw std::runtime_error("Access denied: role is not permitted to perform this operation");
    const auto stored = users_.findAccountState(s.userId);
    if (!stored || !stored->active)
        throw std::runtime_error("Access denied: account is inactive or missing");
    if (stored->role != domain::roleName(s.role) || stored->departmentId != s.departmentId)
        throw std::runtime_error("Access denied: session does not match the stored account");
}
void AuthorizationService::requireValidSession(const Session& s) const {
    if (s.userId <= 0)
        throw std::runtime_error("Access denied: invalid session");
    const auto stored = users_.findAccountState(s.userId);
    if (!stored || !stored->active || stored->role != domain::roleName(s.role) ||
        stored->departmentId != s.departmentId)
        throw std::runtime_error("Access denied: session is invalid or inactive");
}
bool AuthorizationService::isLecturerAssigned(int lecturerId, int courseId) const {
    return courses_.isLecturerAssigned(lecturerId, courseId);
}
bool AuthorizationService::isStudentEnrolled(int studentId, int courseId) const {
    return courses_.isStudentEnrolled(studentId, courseId);
}
void AuthorizationService::requireLecturerAssigned(int lecturerId, int courseId) const {
    if (!isLecturerAssigned(lecturerId, courseId))
        throw std::runtime_error("Access denied: lecturer is not assigned to this course");
}

bool AuthenticationService::hasAnyUser() const {
    return users_.countUsers() != 0;
}

int AuthenticationService::bootstrapAdministrator(const std::string& username, const std::string& password,
                                                  const std::string& fullName) {
    if (hasAnyUser())
        throw std::runtime_error("Bootstrap is only available before the first account exists");
    validation::nonblank(username, "Username");
    validation::nonblank(password, "Password");
    validation::nonblank(fullName, "Full name");
    validation::minimumPasswordLength(password);
    const auto h = security::PasswordHasher::hash(password);
    return users_.insert({username, h.hashHex, h.saltHex, "Administrator", fullName, 0});
}

Session AuthenticationService::login(const std::string& username, const std::string& password) const {
    // One generic message for unknown user, inactive account and wrong password (no account enumeration).
    const auto user = users_.findCredentials(username);
    if (!user || !user->active)
        throw std::runtime_error("Invalid username or password");
    if (!security::PasswordHasher::verify(password, {user->passwordHash, user->passwordSalt}))
        throw std::runtime_error("Invalid username or password");
    return Session{user->userId, user->username, user->fullName, domain::parseRole(user->role), user->departmentId};
}

} // namespace lms::services
