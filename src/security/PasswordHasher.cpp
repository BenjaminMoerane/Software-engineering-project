#include "security/PasswordHasher.hpp"

#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace lms::security {
namespace {
constexpr std::size_t kSaltBytes = 16;
constexpr std::size_t kHashBytes = 32;
constexpr std::uint64_t kScryptN = 16384;
constexpr std::uint64_t kScryptR = 8;
constexpr std::uint64_t kScryptP = 1;
constexpr std::uint64_t kMaxMemory = 64ULL * 1024ULL * 1024ULL;

std::string toHex(const unsigned char* bytes, std::size_t length) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(length * 2);
    for (std::size_t i = 0; i < length; ++i) {
        result.push_back(digits[bytes[i] >> 4]);
        result.push_back(digits[bytes[i] & 0x0f]);
    }
    return result;
}
int fromHexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
std::vector<unsigned char> fromHex(const std::string& text) {
    if (text.size() % 2 != 0) throw std::invalid_argument("Stored password hash encoding is invalid");
    std::vector<unsigned char> bytes(text.size() / 2);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const int high = fromHexDigit(text[2 * i]);
        const int low = fromHexDigit(text[2 * i + 1]);
        if (high < 0 || low < 0) throw std::invalid_argument("Stored password hash encoding is invalid");
        bytes[i] = static_cast<unsigned char>((high << 4) | low);
    }
    return bytes;
}
std::vector<unsigned char> derive(const std::string& password, const unsigned char* salt, std::size_t saltLength) {
    std::vector<unsigned char> output(kHashBytes);
    if (EVP_PBE_scrypt(password.data(), password.size(), salt, saltLength,
                       kScryptN, kScryptR, kScryptP, kMaxMemory,
                       output.data(), output.size()) != 1)
        throw std::runtime_error("Password hashing failed in OpenSSL scrypt");
    return output;
}
}

PasswordHash PasswordHasher::hash(const std::string& password) {
    if (password.empty()) throw std::invalid_argument("Password must not be empty");
    std::array<unsigned char, kSaltBytes> salt{};
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1)
        throw std::runtime_error("Secure random salt generation failed");
    const auto derived = derive(password, salt.data(), salt.size());
    return {toHex(derived.data(), derived.size()), toHex(salt.data(), salt.size())};
}

bool PasswordHasher::verify(const std::string& password, const PasswordHash& stored) {
    const auto salt = fromHex(stored.saltHex);
    const auto expected = fromHex(stored.hashHex);
    if (salt.size() != kSaltBytes || expected.size() != kHashBytes) return false;
    const auto actual = derive(password, salt.data(), salt.size());
    return CRYPTO_memcmp(actual.data(), expected.data(), expected.size()) == 0;
}
} // namespace lms::security
