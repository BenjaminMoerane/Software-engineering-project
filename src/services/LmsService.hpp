#pragma once
#include "data/Database.hpp"
#include "domain/User.hpp"
#include <string>
#include <functional>
#include <vector>
#include <filesystem>
namespace lms::services {
struct AssignmentView {int id{};std::string courseCode,title,deadline;double maxMark{};std::string extensionDeadline;};
struct SubmissionView {int submissionId{},versionId{},studentId{},assignmentId{},versionNo{},latenessDays{};std::string studentName,assignmentTitle,submittedAt,lateStatus,contentRef;double maxMark{};};
struct ResultView {int assignmentId{};std::string assignmentTitle,courseCode,feedback;double mark{},maxMark{};};
class LmsService {
public:
 explicit LmsService(data::Database& db,std::function<std::string()> clock = {});
 int bootstrapAdministrator(const std::string& username,const std::string& password,const std::string& fullName);
 domain::Session login(const std::string& username,const std::string& password) const;
 int createDepartment(const domain::Session&,const std::string& name);
 void deactivateUser(const domain::Session&,int userId);
 void updateUserAccount(const domain::Session&,int userId,const std::string& fullName,domain::Role role,int departmentId);
 void updateDepartment(const domain::Session&,int departmentId,const std::string& name);
 void updateCourse(const domain::Session&,int courseId,const std::string& code,const std::string& title,int departmentId);
 int createUser(const domain::Session&,const std::string& username,const std::string& password,const std::string& fullName,domain::Role role,int departmentId=0);
 int createCourse(const domain::Session&,int departmentId,const std::string& code,const std::string& title);
 void assignLecturer(const domain::Session&,int courseId,int lecturerId);
 void enrolStudent(const domain::Session&,int courseId,int studentId);
 int createAssignment(const domain::Session&,int courseId,const std::string& title,const std::string& description,const std::string& deadline,double maxMark);
 void grantIndividualExtension(const domain::Session&,int assignmentId,int studentId,const std::string& revisedDeadline,const std::string& reason);
 std::vector<AssignmentView> listAssignments(const domain::Session&) const;
 int submit(const domain::Session&,int assignmentId,const std::string& contentRef);
 int submitFile(const domain::Session&,int assignmentId,const std::filesystem::path& sourceFile, const std::filesystem::path& storageDirectory);
 std::vector<SubmissionView> submissionsForMarking(const domain::Session&) const;
 int markSubmission(const domain::Session&,int submissionId,double rawMark,const std::string& feedback,const std::string& reason);
 void moderate(const domain::Session&,int markId,bool approve,const std::string& reason);
 void release(const domain::Session&,int markId);
 std::vector<ResultView> releasedResults(const domain::Session&) const;
 std::vector<std::string> listDepartments() const;
private:
 data::Database& db_;
 std::function<std::string()> clock_;
 void requireRole(const domain::Session&,domain::Role) const;
 bool lecturerAssigned(int userId,int courseId) const;
 bool studentEnrolled(int userId,int courseId) const;
 int departmentForCourse(int courseId) const;
};
}
