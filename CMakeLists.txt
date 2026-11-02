cmake_minimum_required(VERSION 3.20)
project(CS4433_Moderated_LMS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
include(CTest)

add_library(lms_core
    src/data/Database.cpp
    src/security/PasswordHasher.cpp
    src/data/SqliteRepositories.cpp
    src/services/Validation.cpp
    src/services/DeadlineEvaluator.cpp
    src/services/AccessServices.cpp
    src/services/AdministrationServices.cpp
    src/services/AssessmentServices.cpp
    src/services/MarkingServices.cpp
    src/services/ModerationServices.cpp
    src/services/LmsService.cpp
)
target_include_directories(lms_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(lms_core PUBLIC SQLite::SQLite3 OpenSSL::Crypto)
# The Phase 3 schema file stays the single source of truth; it is embedded into the program at configure time so an
# installed lms_app does not need the source tree (defect F12). Re-runs automatically when the .sql file changes.
set(LMS_SCHEMA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/database_schema.sql)
file(READ ${LMS_SCHEMA_FILE} LMS_SCHEMA_SQL)
configure_file(cmake/EmbeddedSchema.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedSchema.hpp @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${LMS_SCHEMA_FILE})
target_include_directories(lms_core PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
# Warning policy (docs/CODING_STANDARD.md section 7). CI builds with LMS_WARNINGS_AS_ERRORS=ON.
option(LMS_WARNINGS_AS_ERRORS "Treat compiler warnings in project code as errors" OFF)
if(MSVC)
    set(LMS_WARNING_FLAGS /W4 /permissive-)
    set(LMS_WERROR_FLAG /WX)
else()
    set(LMS_WARNING_FLAGS -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    set(LMS_WERROR_FLAG -Werror)
endif()
if(LMS_WARNINGS_AS_ERRORS)
    list(APPEND LMS_WARNING_FLAGS ${LMS_WERROR_FLAG})
endif()
target_compile_options(lms_core PRIVATE ${LMS_WARNING_FLAGS})

add_executable(lms_app src/main.cpp)
target_link_libraries(lms_app PRIVATE lms_core)
target_compile_options(lms_app PRIVATE ${LMS_WARNING_FLAGS})
target_compile_definitions(lms_app PRIVATE LMS_VERSION="${PROJECT_VERSION}")

# Optional gcov instrumentation (GCC or Clang). Use with a Debug build, then run gcovr (see docs/test/TEST_PLAN.md).
option(LMS_ENABLE_COVERAGE "Instrument the build for gcov/gcovr line coverage" OFF)
if(LMS_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lms_core PUBLIC --coverage -O0 -g)
        target_link_options(lms_core PUBLIC --coverage)
    else()
        message(FATAL_ERROR "LMS_ENABLE_COVERAGE needs GCC or Clang")
    endif()
endif()

# GoogleTest suite. Uses an installed GoogleTest if CMake finds one, otherwise fetches the pinned v1.14.0 from GitHub
# (needs network at configure time). Use -DLMS_ENABLE_GTEST=OFF to build only the dependency-free tests.
option(LMS_ENABLE_GTEST "Build the GoogleTest unit and integration suite" ON)

if(BUILD_TESTING)
    add_executable(lms_smoke_tests tests/smoke_tests.cpp)
    target_link_libraries(lms_smoke_tests PRIVATE lms_core)
    add_test(NAME lms_smoke_tests COMMAND lms_smoke_tests)
    add_executable(lms_integration_tests tests/integration_tests.cpp)
    target_link_libraries(lms_integration_tests PRIVATE lms_core)
    add_test(NAME lms_integration_tests COMMAND lms_integration_tests)
    add_executable(lms_late_policy_tests tests/late_policy_tests.cpp)
    target_link_libraries(lms_late_policy_tests PRIVATE lms_core)
    add_test(NAME lms_late_policy_tests COMMAND lms_late_policy_tests)

    if(LMS_ENABLE_GTEST)
        find_package(GTest QUIET)
        if(NOT GTest_FOUND)
            include(FetchContent)
            set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
            set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
            FetchContent_Declare(googletest
                GIT_REPOSITORY https://github.com/google/googletest.git
                GIT_TAG f8d7d77c06936315286eb55f8de22cd23c188571) # v1.14.0
            FetchContent_MakeAvailable(googletest)
        endif()
        add_executable(lms_unit_tests
            tests/unit/test_domain.cpp
            tests/unit/test_penalty_policy.cpp
            tests/unit/test_security_login.cpp
            tests/unit/test_authorization.cpp
            tests/unit/test_admin.cpp
            tests/unit/test_assignments_extensions.cpp
            tests/unit/test_submission.cpp
            tests/unit/test_marking_audit.cpp
            tests/unit/test_moderation_release.cpp
            tests/unit/test_persistence_errors.cpp
            tests/unit/test_architecture.cpp)
        target_link_libraries(lms_unit_tests PRIVATE lms_core GTest::gtest GTest::gtest_main)
        target_include_directories(lms_unit_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
        target_compile_definitions(lms_unit_tests PRIVATE LMS_SOURCE_SCHEMA_FILE="${LMS_SCHEMA_FILE}")
        if(NOT MSVC)
            target_compile_options(lms_unit_tests PRIVATE -Wall -Wextra)
        endif()
        add_test(NAME lms_unit_tests COMMAND lms_unit_tests)
    endif()

    # System and acceptance tests drive the real console program (needs Python 3).
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        # Architecture rule: no SQL or SQLite access outside src/data/ (Repository/DAO pattern).
        add_test(NAME lms_architecture_sql_isolation
            COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/tests/architecture/check_sql_isolation.py
                    ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME lms_system_tests
            COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/tests/system/run_system_tests.py
                    --app $<TARGET_FILE:lms_app>)
    endif()
endif()

# Installation and packaging (Phase 7). `cmake --install build --prefix <dir>` or `cpack --config build/CPackConfig.cmake`.
include(GNUInstallDirs)
install(TARGETS lms_app RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(WIN32)
    install(PROGRAMS packaging/lms-start.bat DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
    install(PROGRAMS packaging/lms-start.sh DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(FILES README.md RELEASE_NOTES.md database_schema.sql DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY docs/deployment/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/deployment)
set(CPACK_PACKAGE_NAME "cs4433-moderated-lms")
set(CPACK_PACKAGE_VENDOR "CS4433 individual project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Offline moderated learning management system (C++17, SQLite)")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_IGNORE_FILES "/build.*/;/\\.git/;/submission_files/;\\.sqlite3")
include(CPack)

option(BUILD_BENCHMARKS "Build synthetic performance benchmark executable" OFF)
if(BUILD_BENCHMARKS)
    add_executable(lms_performance_benchmark tests/performance_benchmark.cpp)
    target_link_libraries(lms_performance_benchmark PRIVATE lms_core)
endif()
