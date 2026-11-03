#include "data/Database.hpp"
#include "services/LmsService.hpp"
#include <exception>
#include <iostream>
#include <cstdint>
#include <map>
#include <limits>
#include <string>
#include <filesystem>
#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace {
std::string ask(const std::string& label) {
    std::cout << label;
    std::string v;
    if (!std::getline(std::cin, v))
        return "quit";
    return v;
}
std::string askSecret(const std::string& label) {
    std::cout << label << std::flush;
#ifdef _WIN32
    std::string value;
    for (;;) {
        int ch = _getch();
        if (ch == '\r' || ch == '\n') {
            std::cout << '\n';
            break;
        }
        if (ch == 3) {
            std::cout << '\n';
            throw std::runtime_error("Input cancelled");
        }
        if (ch == '\b') {
            if (!value.empty())
                value.pop_back();
            continue;
        }
        if (ch >= 32 && ch <= 126)
            value.push_back(static_cast<char>(ch));
    }
    return value;
#else
    termios oldState{};
    const bool terminal = (tcgetattr(STDIN_FILENO, &oldState) == 0);
    if (terminal) {
        termios hidden = oldState;
        hidden.c_lflag &= static_cast<tcflag_t>(~ECHO);
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &hidden) != 0)
            throw std::runtime_error("Could not configure terminal for hidden password input");
    }
    try {
        std::string value;
        if (!std::getline(std::cin, value)) {
            if (terminal)
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldState);
            throw std::runtime_error("Password input ended");
        }
        if (terminal)
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldState);
        std::cout << '\n';
        return value;
    } catch (...) {
        if (terminal)
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldState);
        throw;
    }
#endif
}
int askInt(const std::string& label) {
    for (;;) {
        try {
            auto s = ask(label);
            std::size_t n = 0;
            int v = std::stoi(s, &n);
            if (n != s.size())
                throw std::invalid_argument("bad");
            return v;
        } catch (...) {
            if (!std::cin)
                return 0;
            std::cout << "Enter a whole number.\n";
        }
    }
}
double askDouble(const std::string& label) {
    for (;;) {
        try {
            auto s = ask(label);
            std::size_t n = 0;
            double v = std::stod(s, &n);
            if (n != s.size())
                throw std::invalid_argument("bad");
            return v;
        } catch (...) {
            if (!std::cin)
                return 0.0;
            std::cout << "Enter a number.\n";
        }
    }
}
void showAssignments(const lms::services::LmsService& svc, const lms::domain::Session& s) {
    for (const auto& a : svc.listAssignments(s)) {
        std::cout << a.id << " | " << a.courseCode << " | " << a.title << " | deadline " << a.deadline << " | max "
                  << a.maxMark;
        if (!a.classExtensionDeadline.empty()) // CR1: class-wide extension shown on the assignment list
            std::cout << " | class extension " << a.classExtensionDeadline;
        if (!a.extensionDeadline.empty())
            std::cout << " | your extension " << a.extensionDeadline;
        std::cout << '\n';
    }
}
void showModerationQueue(const lms::services::LmsService& svc, const lms::domain::Session& s) {
    for (const auto& m : svc.moderationQueue(s))
        std::cout << "Mark " << m.markId << " | " << m.courseCode << " | " << m.assignmentTitle << " | student "
                  << m.studentName << " | " << m.lateStatus << " | raw " << m.rawMark << " penalty " << m.penalty
                  << " final " << m.finalMark << "/" << m.maxMark << " | " << m.state << " | feedback: " << m.feedback
                  << '\n';
}
using lms::domain::Session;
using lms::services::LmsService;
using Handler = void (*)(LmsService&, const Session&);

// One handler per menu option (Phase 6 code review CR-03: main() had CCN 31; each handler is now CCN 1-2).
// Prompts and messages are unchanged; system tests ST_* depend on them.
// All menu handlers share one Handler signature, so read-only handlers cannot take a const service.
// cppcheck-suppress [constParameterCallback,constParameterReference]
void listAssignments(LmsService& svc, const Session& s) {
    showAssignments(svc, s);
}
void createDepartment(LmsService& svc, const Session& s) {
    int id = svc.createDepartment(s, ask("Department name: "));
    std::cout << "Created department " << id << '\n';
}
void createUser(LmsService& svc, const Session& s) {
    auto role = lms::domain::parseRole(ask("Role (Administrator/Lecturer/HeadOfDepartment/Student): "));
    int dep = askInt("Department ID (0 if none): ");
    auto un = ask("Username: ");
    auto pw = askSecret("Password (10+ chars): ");
    auto fn = ask("Full name: ");
    int id = svc.createUser(s, un, pw, fn, role, dep);
    std::cout << "Created user " << id << '\n';
}
void createCourse(LmsService& svc, const Session& s) {
    int dep = askInt("Department ID: ");
    auto code = ask("Course code: ");
    auto title = ask("Course title: ");
    int id = svc.createCourse(s, dep, code, title);
    std::cout << "Created course " << id << '\n';
}
void assignLecturer(LmsService& svc, const Session& s) {
    int course = askInt("Course ID: ");
    int lecturer = askInt("Lecturer user ID: ");
    svc.assignLecturer(s, course, lecturer);
    std::cout << "Lecturer assigned.\n";
}
void enrolStudent(LmsService& svc, const Session& s) {
    int course = askInt("Course ID: ");
    int student = askInt("Student user ID: ");
    svc.enrolStudent(s, course, student);
    std::cout << "Student enrolled.\n";
}
void createAssignment(LmsService& svc, const Session& s) {
    int course = askInt("Course ID: ");
    auto title = ask("Title: ");
    auto desc = ask("Description: ");
    auto deadline = ask("Deadline UTC (YYYY-MM-DDTHH:MM:SSZ): ");
    double max = askDouble("Maximum mark: ");
    int id = svc.createAssignment(s, course, title, desc, deadline, max);
    std::cout << "Created assignment " << id << '\n';
}
void grantExtension(LmsService& svc, const Session& s) {
    int a = askInt("Assignment ID: ");
    int student = askInt("Student ID: ");
    auto deadline = ask("Revised deadline UTC: ");
    auto reason = ask("Reason: ");
    svc.grantIndividualExtension(s, a, student, deadline, reason);
    std::cout << "Extension saved.\n";
}
void grantClassExtension(LmsService& svc, const Session& s) { // CR1 / FR-23
    int a = askInt("Assignment ID: ");
    auto deadline = ask("Revised deadline for the whole class UTC: ");
    auto reason = ask("Reason: ");
    svc.grantClassExtension(s, a, deadline, reason);
    std::cout << "Class-wide extension saved for every student enrolled in the course.\n";
}
void submitWork(LmsService& svc, const Session& s) {
    int a = askInt("Assignment ID: ");
    auto source = ask("Path to file to submit: ");
    int id = svc.submitFile(s, a, std::filesystem::path(source), std::filesystem::path("submission_files"));
    std::cout << "Submission file stored and saved (submission ID " << id << ").\n";
}
void markWork(LmsService& svc, const Session& s) {
    for (const auto& v : svc.submissionsForMarking(s))
        std::cout << "Submission " << v.submissionId << " version " << v.versionNo << " student " << v.studentName
                  << " assignment " << v.assignmentTitle << " status " << v.lateStatus << " max " << v.maxMark
                  << " file " << v.contentRef << '\n';
    int id = askInt("Submission ID: ");
    double m = askDouble("Raw mark: ");
    auto fb = ask("Written feedback: ");
    auto reason = ask("Mark-change reason: ");
    const int markId = svc.markSubmission(s, id, m, fb, reason);
    std::cout << "Mark recorded as ID " << markId << " (pending HoD moderation).\n";
}
void moderateMark(LmsService& svc, const Session& s) {
    showModerationQueue(svc, s);
    int id = askInt("Mark ID: ");
    bool approve = ask("Approve? (y/n): ") == "y";
    auto reason = ask("Reason: ");
    svc.moderate(s, id, approve, reason);
    std::cout << "Moderation decision recorded.\n";
}
void releaseMark(LmsService& svc, const Session& s) {
    showModerationQueue(svc, s);
    svc.release(s, askInt("Mark ID: "));
    std::cout << "Mark released.\n";
}
// All menu handlers share one Handler signature, so read-only handlers cannot take a const service.
// cppcheck-suppress [constParameterCallback,constParameterReference]
void viewResults(LmsService& svc, const Session& s) {
    for (const auto& r : svc.releasedResults(s))
        std::cout << r.courseCode << " | " << r.assignmentTitle << " | " << r.mark << "/" << r.maxMark << " | "
                  << r.feedback << '\n';
}
void deactivateUser(LmsService& svc, const Session& s) {
    svc.deactivateUser(s, askInt("User ID to deactivate: "));
    std::cout << "Account deactivated.\n";
}
void updateUser(LmsService& svc, const Session& s) {
    int id = askInt("User ID: ");
    auto name = ask("New full name: ");
    auto role = lms::domain::parseRole(ask("Role (Administrator/Lecturer/HeadOfDepartment/Student): "));
    int dep = askInt("Department ID (0 if none): ");
    svc.updateUserAccount(s, id, name, role, dep);
    std::cout << "User account updated.\n";
}
void updateDepartment(LmsService& svc, const Session& s) {
    int id = askInt("Department ID: ");
    auto name = ask("New department name: ");
    svc.updateDepartment(s, id, name);
    std::cout << "Department updated.\n";
}
void updateCourse(LmsService& svc, const Session& s) {
    int id = askInt("Course ID: ");
    auto code = ask("New course code: ");
    auto title = ask("New course title: ");
    int dep = askInt("New department ID: ");
    svc.updateCourse(s, id, code, title, dep);
    std::cout << "Course updated.\n";
}

const std::map<int, Handler>& menuHandlers() {
    static const std::map<int, Handler> handlers{
        {1, listAssignments}, {2, createDepartment},    {3, createUser},       {4, createCourse},
        {5, assignLecturer},  {6, enrolStudent},        {7, createAssignment}, {8, grantExtension},
        {9, submitWork},      {10, markWork},           {11, moderateMark},    {12, releaseMark},
        {13, viewResults},    {14, deactivateUser},     {15, updateUser},      {16, updateDepartment},
        {17, updateCourse},   {18, grantClassExtension}};
    return handlers;
}

constexpr const char* kMenu =
    "\n1 List assignments  2 Create department [Admin]  3 Create user [Admin]\n4 Create course [Admin] 5 Assign "
    "lecturer [Admin] 6 Enrol student [Admin]\n7 Create assignment [Lecturer] 8 Grant individual extension "
    "[Lecturer]\n9 Submit/resubmit [Student] 10 Mark submission [Lecturer] 11 Moderate mark [HoD]\n12 Release mark "
    "[HoD] 13 View released results [Student] 14 Deactivate user [Admin] 15 Update user [Admin] 16 Update "
    "department [Admin] 17 Update course [Admin] 18 Grant class-wide extension [Lecturer] 0 Logout\n";

// Runs one logged-in session until the user chooses 0 (or input ends, which askInt reports as 0).
void runSession(LmsService& svc, const Session& s) {
    for (;;) {
        std::cout << kMenu;
        const int op = askInt("Choice: ");
        if (op == 0)
            return;
        const auto it = menuHandlers().find(op);
        if (it == menuHandlers().end()) {
            std::cout << "Unknown option.\n";
            continue;
        }
        try {
            it->second(svc, s);
        } catch (const std::exception& e) {
            std::cout << "Operation failed: " << e.what() << '\n';
        }
    }
}

enum class SetupResult : std::uint8_t { Created, Retry, InputEnded };
SetupResult firstRunSetup(LmsService& svc) {
    std::cout << "First-run setup: create the initial Administrator account.\n";
    try {
        const auto un = ask("Username: ");
        const auto pw = askSecret("Password (at least 10 characters): ");
        const auto fn = ask("Full name: ");
        svc.bootstrapAdministrator(un, pw, fn);
        std::cout << "Administrator created.\n";
        return SetupResult::Created;
    } catch (const std::exception& e) {
        std::cout << "Setup failed: " << e.what() << "\n";
        if (!std::cin) {
            std::cout << "Input ended before the initial Administrator was created.\n";
            return SetupResult::InputEnded;
        }
        return SetupResult::Retry;
    }
}
} // namespace

#ifndef LMS_VERSION
#define LMS_VERSION "unknown"
#endif

// Installation check (docs/deployment/INSTALLATION_GUIDE.md section 3). Returns true if the flag was handled.
bool printVersionIfRequested(int argc, char* argv[]) {
    if (argc < 2 || std::string(argv[1]) != "--version")
        return false;
    std::cout << "lms_app " << LMS_VERSION << '\n';
    return true;
}

int main(int argc, char* argv[]) {
    if (printVersionIfRequested(argc, argv))
        return 0;
    const std::string path = argc > 1 ? argv[1] : "lms.sqlite3";
    try {
        lms::data::Database db(path);
        db.initialiseSchema();
        LmsService svc(db);
        std::cout << "CS4433 Moderated Learning Management System (console, offline)\nDatabase: " << path << "\n";
        for (;;) {
            if (!svc.hasAnyUser()) {
                const SetupResult r = firstRunSetup(svc);
                if (r == SetupResult::InputEnded)
                    return 1;
                if (r == SetupResult::Retry)
                    continue;
            }
            const std::string username = ask("Login username (or 'quit'): ");
            if (username == "quit")
                break;
            try {
                const auto s = svc.login(username, askSecret("Password: "));
                std::cout << "Welcome, " << s.fullName << " (" << lms::domain::roleName(s.role) << ")\n";
                runSession(svc, s);
            } catch (const std::exception& e) {
                std::cout << "Login failed: " << e.what() << '\n';
            }
        }
        std::cout << "Goodbye.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Startup failed: " << e.what() << '\n';
        return 1;
    }
}
