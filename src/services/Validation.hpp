#pragma once
// Input-validation helpers shared by the application services (FR-20).
#include <string>

namespace lms::services::validation {
void nonblank(const std::string& value, const char* field);              // throws std::invalid_argument
void isoUtc(const std::string& value, const char* field);                // strict YYYY-MM-DDTHH:MM:SSZ
void minimumPasswordLength(const std::string& password);                 // >= 10 characters
std::string systemUtcNow();                                              // default clock
} // namespace lms::services::validation
