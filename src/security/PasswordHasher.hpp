#pragma once
#include <string>

namespace lms::security {
struct PasswordHash {
    std::string hashHex;
    std::string saltHex;
};

// Uses OpenSSL's scrypt KDF. The salt is generated independently for each hash.
class PasswordHasher {
  public:
    static PasswordHash hash(const std::string& password);
    static bool verify(const std::string& password, const PasswordHash& stored);
};
} // namespace lms::security
