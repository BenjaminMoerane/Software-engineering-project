#include "services/LmsService.hpp"
#include "services/Validation.hpp"

namespace lms::services {
using domain::Role;
using domain::Session;

LmsService::LmsService(data::Database& db, std::function<std::string()> clock)
    : tx_(db), users_(db), courses_(db), submissions_(db), marks_(db), moderation_(db), penalty_(),
      authz_(users_, courses_), authn_(users_), userAdmin_(users_, authz_), courseSvc_(courses_, users_, authz_),
      assignmentSvc_(courses_, authz_), extensionSvc_(courses_, authz_),
      submissionSvc_(courses_, submissions_, tx_, authz_,
                     clock ? std::move(clock) : std::function<std::string()>(validation::systemUtcNow)),
      audit_(marks_), marking_(submissions_, marks_, moderation_, tx_, authz_, audit_, penalty_),
      moderationSvc_(moderation_, authz_), results_(moderation_, authz_) {}

bool LmsService::hasAnyUser() const {
    return authn_.hasAnyUser();
}
int LmsService::bootstrapAdministrator(const std::string& u, const std::string& p, const std::string& n) {
    return authn_.bootstrapAdministrator(u, p, n);
}
Session LmsService::login(const std::string& u, const std::string& p) const {
    return authn_.login(u, p);
}

int LmsService::createDepartment(const Session& s, const std::string& name) {
    return courseSvc_.createDepartment(s, name);
}
void LmsService::deactivateUser(const Session& s, int userId) {
    userAdmin_.deactivateUser(s, userId);
}
void LmsService::updateUserAccount(const Session& s, int userId, const std::string& fullName, Role role, int dep) {
    userAdmin_.updateUserAccount(s, userId, fullName, role, dep);
}
void LmsService::updateDepartment(const Session& s, int dep, const std::string& name) {
    courseSvc_.updateDepartment(s, dep, name);
}
void LmsService::updateCourse(const Session& s, int c, const std::string& code, const std::string& title, int dep) {
    courseSvc_.updateCourse(s, c, code, title, dep);
}
int LmsService::createUser(const Session& s, const std::string& u, const std::string& p, const std::string& n,
                           Role role, int dep) {
    return userAdmin_.createUser(s, u, p, n, role, dep);
}
int LmsService::createCourse(const Session& s, int dep, const std::string& code, const std::string& title) {
    return courseSvc_.createCourse(s, dep, code, title);
}
void LmsService::assignLecturer(const Session& s, int c, int l) {
    courseSvc_.assignLecturer(s, c, l);
}
void LmsService::enrolStudent(const Session& s, int c, int st) {
    courseSvc_.enrolStudent(s, c, st);
}
std::vector<std::string> LmsService::listDepartments() const {
    return courseSvc_.listDepartments();
}

int LmsService::createAssignment(const Session& s, int c, const std::string& t, const std::string& d,
                                 const std::string& deadline, double max) {
    return assignmentSvc_.createAssignment(s, c, t, d, deadline, max);
}
void LmsService::grantIndividualExtension(const Session& s, int a, int st, const std::string& d, const std::string& r) {
    extensionSvc_.grantIndividualExtension(s, a, st, d, r);
}
std::vector<AssignmentView> LmsService::listAssignments(const Session& s) const {
    return assignmentSvc_.listAssignments(s);
}

int LmsService::submit(const Session& s, int a, const std::string& content) {
    return submissionSvc_.submit(s, a, content);
}
int LmsService::submitFile(const Session& s, int a, const std::filesystem::path& src,
                           const std::filesystem::path& dir) {
    return submissionSvc_.submitFile(s, a, src, dir);
}
std::vector<SubmissionView> LmsService::submissionsForMarking(const Session& s) const {
    return submissionSvc_.submissionsForMarking(s);
}

int LmsService::markSubmission(const Session& s, int sub, double raw, const std::string& fb, const std::string& r) {
    return marking_.markSubmission(s, sub, raw, fb, r);
}

std::vector<ModerationItemView> LmsService::moderationQueue(const Session& s) const {
    return moderationSvc_.moderationQueue(s);
}
void LmsService::moderate(const Session& s, int m, bool approve, const std::string& r) {
    moderationSvc_.moderate(s, m, approve, r);
}
void LmsService::release(const Session& s, int m) {
    moderationSvc_.release(s, m);
}
std::vector<ResultView> LmsService::releasedResults(const Session& s) const {
    return results_.releasedResults(s);
}

} // namespace lms::services
