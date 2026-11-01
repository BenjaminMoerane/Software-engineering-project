#include "data/Database.hpp"
#include "services/LmsService.hpp"
#include <exception>
#include <iostream>
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
    std::string value;
    try {
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
void showAssignments(lms::services::LmsService& svc, const lms::domain::Session& s) {
    for (const auto& a : svc.listAssignments(s)) {
        std::cout << a.id << " | " << a.courseCode << " | " << a.title << " | deadline " << a.deadline << " | max "
                  << a.maxMark;
        if (!a.extensionDeadline.empty())
            std::cout << " | your extension " << a.extensionDeadline;
        std::cout << '\n';
    }
}
} // namespace
int main(int argc, char* argv[]) {
    const std::string path = argc > 1 ? argv[1] : "lms.sqlite3";
    try {
        lms::data::Database db(path);
        db.initialiseSchema();
        lms::services::LmsService svc(db);
        std::cout << "CS4433 Moderated Learning Management System (console, offline)\nDatabase: " << path << "\n";
        for (;;) {
            lms::data::Statement count(db.handle(), "SELECT count(*) FROM users");
            count.step();
            if (count.integer(0) == 0) {
                std::cout << "First-run setup: create the initial Administrator account.\n";
                try {
                    const auto un = ask("Username: ");
                    const auto pw = askSecret("Password (at least 10 characters): ");
                    const auto fn = ask("Full name: ");
                    svc.bootstrapAdministrator(un, pw, fn);
                    std::cout << "Administrator created.\n";
                } catch (const std::exception& e) {
                    std::cout << "Setup failed: " << e.what() << "\n";
                    if (!std::cin) {
                        std::cout << "Input ended before the initial Administrator was created.\n";
                        return 1;
                    }
                    continue;
                }
            }
            const std::string username = ask("Login username (or 'quit'): ");
            if (username == "quit")
                break;
            try {
                auto s = svc.login(username, askSecret("Password: "));
                std::cout << "Welcome, " << s.fullName << " (" << lms::domain::roleName(s.role) << ")\n";
                bool done = false;
                while (!done) {
                    std::cout
                        << "\n1 List assignments  2 Create department [Admin]  3 Create user [Admin]\n4 Create course [Admin] 5 Assign lecturer [Admin] 6 Enrol student [Admin]\n7 Create assignment [Lecturer] 8 Grant individual extension [Lecturer]\n9 Submit/resubmit [Student] 10 Mark submission [Lecturer] 11 Moderate mark [HoD]\n12 Release mark [HoD] 13 View released results [Student] 14 Deactivate user [Admin] 15 Update user [Admin] 16 Update department [Admin] 17 Update course [Admin] 0 Logout\n";
                    int op = askInt("Choice: ");
                    try {
                        switch (op) {
                        case 0:
                            done = true;
                            break;
                        case 1:
                            showAssignments(svc, s);
                            break;
                        case 2: {
                            int id = svc.createDepartment(s, ask("Department name: "));
                            std::cout << "Created department " << id << '\n';
                            break;
                        }
                        case 3: {
                            auto role =
                                lms::domain::parseRole(ask("Role (Administrator/Lecturer/HeadOfDepartment/Student): "));
                            int dep = askInt("Department ID (0 if none): ");
                            auto un = ask("Username: ");
                            auto pw = askSecret("Password (10+ chars): ");
                            auto fn = ask("Full name: ");
                            int id = svc.createUser(s, un, pw, fn, role, dep);
                            std::cout << "Created user " << id << '\n';
                            break;
                        }
                        case 4: {
                            int dep = askInt("Department ID: ");
                            auto code = ask("Course code: ");
                            auto title = ask("Course title: ");
                            int id = svc.createCourse(s, dep, code, title);
                            std::cout << "Created course " << id << '\n';
                            break;
                        }
                        case 5: {
                            int course = askInt("Course ID: ");
                            int lecturer = askInt("Lecturer user ID: ");
                            svc.assignLecturer(s, course, lecturer);
                            std::cout << "Lecturer assigned.\n";
                            break;
                        }
                        case 6: {
                            int course = askInt("Course ID: ");
                            int student = askInt("Student user ID: ");
                            svc.enrolStudent(s, course, student);
                            std::cout << "Student enrolled.\n";
                            break;
                        }
                        case 7: {
                            int course = askInt("Course ID: ");
                            auto title = ask("Title: ");
                            auto desc = ask("Description: ");
                            auto deadline = ask("Deadline UTC (YYYY-MM-DDTHH:MM:SSZ): ");
                            double max = askDouble("Maximum mark: ");
                            int id = svc.createAssignment(s, course, title, desc, deadline, max);
                            std::cout << "Created assignment " << id << '\n';
                            break;
                        }
                        case 8: {
                            int a = askInt("Assignment ID: ");
                            int student = askInt("Student ID: ");
                            auto deadline = ask("Revised deadline UTC: ");
                            auto reason = ask("Reason: ");
                            svc.grantIndividualExtension(s, a, student, deadline, reason);
                            std::cout << "Extension saved.\n";
                            break;
                        }
                        case 9: {
                            int a = askInt("Assignment ID: ");
                            auto source = ask("Path to file to submit: ");
                            int id = svc.submitFile(s, a, std::filesystem::path(source),
                                                    std::filesystem::path("submission_files"));
                            std::cout << "Submission file stored and saved (submission ID " << id << ").\n";
                            break;
                        }
                        case 10: {
                            auto list = svc.submissionsForMarking(s);
                            for (auto& v : list)
                                std::cout << "Submission " << v.submissionId << " version " << v.versionNo
                                          << " student " << v.studentName << " assignment " << v.assignmentTitle
                                          << " status " << v.lateStatus << " max " << v.maxMark << " file "
                                          << v.contentRef << '\n';
                            int id = askInt("Submission ID: ");
                            double m = askDouble("Raw mark: ");
                            auto fb = ask("Written feedback: ");
                            auto reason = ask("Mark-change reason: ");
                            std::cout << "Mark recorded as ID " << svc.markSubmission(s, id, m, fb, reason)
                                      << " (pending HoD moderation).\n";
                            break;
                        }
                        case 11: {
                            int id = askInt("Mark ID: ");
                            bool approve = ask("Approve? (y/n): ") == "y";
                            auto reason = ask("Reason: ");
                            svc.moderate(s, id, approve, reason);
                            std::cout << "Moderation decision recorded.\n";
                            break;
                        }
                        case 12:
                            svc.release(s, askInt("Mark ID: "));
                            std::cout << "Mark released.\n";
                            break;
                        case 13:
                            for (auto& r : svc.releasedResults(s))
                                std::cout << r.courseCode << " | " << r.assignmentTitle << " | " << r.mark << "/"
                                          << r.maxMark << " | " << r.feedback << '\n';
                            break;
                        case 14:
                            svc.deactivateUser(s, askInt("User ID to deactivate: "));
                            std::cout << "Account deactivated.\n";
                            break;
                        case 15: {
                            int id = askInt("User ID: ");
                            auto name = ask("New full name: ");
                            auto role =
                                lms::domain::parseRole(ask("Role (Administrator/Lecturer/HeadOfDepartment/Student): "));
                            int dep = askInt("Department ID (0 if none): ");
                            svc.updateUserAccount(s, id, name, role, dep);
                            std::cout << "User account updated.\n";
                            break;
                        }
                        case 16: {
                            int id = askInt("Department ID: ");
                            auto name = ask("New department name: ");
                            svc.updateDepartment(s, id, name);
                            std::cout << "Department updated.\n";
                            break;
                        }
                        case 17: {
                            int id = askInt("Course ID: ");
                            auto code = ask("New course code: ");
                            auto title = ask("New course title: ");
                            int dep = askInt("New department ID: ");
                            svc.updateCourse(s, id, code, title, dep);
                            std::cout << "Course updated.\n";
                            break;
                        }
                        default:
                            std::cout << "Unknown option.\n";
                        }
                    } catch (const std::exception& e) {
                        std::cout << "Operation failed: " << e.what() << '\n';
                    }
                }
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
