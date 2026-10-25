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
    src/services/LmsService.cpp
)
target_include_directories(lms_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(lms_core PUBLIC SQLite::SQLite3 OpenSSL::Crypto)
target_compile_definitions(lms_core PRIVATE LMS_SCHEMA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/database_schema.sql")
if(MSVC)
    target_compile_options(lms_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(lms_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_executable(lms_app src/main.cpp)
target_link_libraries(lms_app PRIVATE lms_core)

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
endif()

option(BUILD_BENCHMARKS "Build synthetic performance benchmark executable" OFF)
if(BUILD_BENCHMARKS)
    add_executable(lms_performance_benchmark tests/performance_benchmark.cpp)
    target_link_libraries(lms_performance_benchmark PRIVATE lms_core)
endif()
