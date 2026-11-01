// T-LOGIN / T-PASSWORD: authentication and password protection (FR-01, NFR-04, BR-08).
#include "TestSupport.hpp"

#include "security/PasswordHasher.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <stdexcept>

using namespace lmstest;

namespace {
std::string readAll(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
} // namespace

TEST_F(LmsFixture, T_LOGIN_01_valid_credentials_give_a_session) {
    const int id = svc().bootstrapAdministrator("admin", kAdminPassword, "System Administrator");
    const Session s = svc().login("admin", kAdminPassword);
    EXPECT_EQ(s.userId, id);
    EXPECT_EQ(s.role, Role::Administrator);
    EXPECT_EQ(s.fullName, "System Administrator");
}

TEST_F(LmsFixture, T_LOGIN_02_wrong_password_and_unknown_user_look_the_same) {
    svc().bootstrapAdministrator("admin", kAdminPassword, "System Administrator");
    EXPECT_FAILS_WITH(svc().login("admin", "wrong-password"), "Invalid username or password");
    EXPECT_FAILS_WITH(svc().login("nobody", kAdminPassword), "Invalid username or password");
    EXPECT_FAILS_WITH(svc().login("", ""), "Invalid username or password");
    EXPECT_FAILS_WITH(svc().login("admin", ""), "Invalid username or password");
}

TEST_F(LmsFixture, T_LOGIN_03_username_is_exact_and_case_sensitive) {
    svc().bootstrapAdministrator("admin", kAdminPassword, "System Administrator");
    EXPECT_FAILS_WITH(svc().login("ADMIN", kAdminPassword), "Invalid username or password");
    EXPECT_FAILS_WITH(svc().login("admin ", kAdminPassword), "Invalid username or password");
}

TEST_F(LmsFixture, T_LOGIN_04_inactive_account_cannot_log_in) {
    buildWorld();
    svc().deactivateUser(admin_, studentId_);
    EXPECT_FAILS_WITH(svc().login("student", kUserPassword), "Invalid username or password");
}

TEST_F(LmsFixture, T_LOGIN_05_sql_injection_text_is_just_a_wrong_username) {
    svc().bootstrapAdministrator("admin", kAdminPassword, "System Administrator");
    EXPECT_FAILS_WITH(svc().login("admin' OR '1'='1", "x"), "Invalid username or password");
    EXPECT_FAILS_WITH(svc().login("admin'; DROP TABLE users;--", "x"), "Invalid username or password");
    EXPECT_EQ(scalar("SELECT count(*) FROM users"), 1);
    EXPECT_NO_THROW(svc().login("admin", kAdminPassword));
}

TEST_F(LmsFixture, T_LOGIN_06_bootstrap_only_before_first_account) {
    svc().bootstrapAdministrator("admin", kAdminPassword, "System Administrator");
    EXPECT_FAILS_WITH(svc().bootstrapAdministrator("second", kAdminPassword, "Second"),
                      "Bootstrap is only available before the first account exists");
}

TEST_F(LmsFixture, T_LOGIN_07_bootstrap_validates_input) {
    EXPECT_FAILS_WITH(svc().bootstrapAdministrator("", kAdminPassword, "Name"), "Username must not be empty");
    EXPECT_FAILS_WITH(svc().bootstrapAdministrator("   ", kAdminPassword, "Name"), "Username must not be empty");
    EXPECT_FAILS_WITH(svc().bootstrapAdministrator("a", "", "Name"), "Password must not be empty");
    EXPECT_FAILS_WITH(svc().bootstrapAdministrator("a", "123456789", "Name"), "at least 10 characters"); // 9 chars
    EXPECT_FAILS_WITH(svc().bootstrapAdministrator("a", kAdminPassword, " "), "Full name must not be empty");
    EXPECT_EQ(scalar("SELECT count(*) FROM users"), 0);
    EXPECT_NO_THROW(svc().bootstrapAdministrator("a", "1234567890", "Name")); // exactly 10 characters is accepted
}

TEST_F(LmsFixture, T_PASSWORD_01_database_never_contains_the_plaintext_password) {
    buildWorld();
    db().execute("PRAGMA wal_checkpoint(FULL);");
    for (const char* password : {kAdminPassword, kUserPassword}) {
        EXPECT_EQ(readAll(dbPath_).find(password), std::string::npos);
        if (fs::exists(dbPath_ + "-wal")) {
            EXPECT_EQ(readAll(dbPath_ + "-wal").find(password), std::string::npos);
        }
    }
}

TEST_F(LmsFixture, T_PASSWORD_02_each_account_has_its_own_salt_and_a_hash) {
    buildWorld();
    EXPECT_EQ(scalar("SELECT count(DISTINCT password_salt) FROM users"), scalar("SELECT count(*) FROM users"));
    EXPECT_EQ(scalar("SELECT count(*) FROM users WHERE password_hash=''"), 0);
    // two accounts with the same password must not share a hash
    EXPECT_EQ(scalar("SELECT count(DISTINCT password_hash) FROM users WHERE role<>'Administrator'"),
              scalar("SELECT count(*) FROM users WHERE role<>'Administrator'"));
}

TEST(PasswordHasher, T_PASSWORD_03_hash_verify_and_salt_behaviour) {
    using lms::security::PasswordHasher;
    const auto a = PasswordHasher::hash("example-passphrase");
    const auto b = PasswordHasher::hash("example-passphrase");
    EXPECT_NE(a.saltHex, b.saltHex);
    EXPECT_NE(a.hashHex, b.hashHex);
    EXPECT_EQ(a.saltHex.size(), 32U); // 16 bytes in hex
    EXPECT_EQ(a.hashHex.size(), 64U); // 32 bytes in hex
    EXPECT_TRUE(PasswordHasher::verify("example-passphrase", a));
    EXPECT_FALSE(PasswordHasher::verify("example-passphrasE", a));
    EXPECT_FALSE(PasswordHasher::verify("", a));
    EXPECT_NE(a.hashHex.find("example"), 0U);
}

TEST(PasswordHasher, T_PASSWORD_04_damaged_stored_values_never_verify) {
    using lms::security::PasswordHash;
    using lms::security::PasswordHasher;
    const auto good = PasswordHasher::hash("example-passphrase");
    PasswordHash wrongHash = good;
    wrongHash.hashHex[0] = (wrongHash.hashHex[0] == '0') ? '1' : '0';
    EXPECT_FALSE(PasswordHasher::verify("example-passphrase", wrongHash));
    PasswordHash wrongSalt = good;
    wrongSalt.saltHex[0] = (wrongSalt.saltHex[0] == '0') ? '1' : '0';
    EXPECT_FALSE(PasswordHasher::verify("example-passphrase", wrongSalt));
    EXPECT_FALSE(PasswordHasher::verify("example-passphrase", PasswordHash{"", ""}));
    // Well-formed hex of the wrong length is rejected with false.
    EXPECT_FALSE(PasswordHasher::verify("example-passphrase", PasswordHash{good.hashHex.substr(0, 10), good.saltHex}));
    // Observed behaviour: a malformed (non-hex or odd-length) stored encoding throws rather than returning false.
    // Recorded as observation O-1 in docs/test/DEFECT_LOG.md (needs a decision, not changed in this phase).
    EXPECT_THROW(PasswordHasher::verify("example-passphrase", PasswordHash{"zz", "not-hex"}), std::invalid_argument);
    // Even length but non-hex characters in either field is also a malformed encoding.
    PasswordHash badChar = good;
    badChar.saltHex[0] = 'z';
    EXPECT_THROW(PasswordHasher::verify("example-passphrase", badChar), std::invalid_argument);
    badChar = good;
    badChar.hashHex[1] = 'g';
    EXPECT_THROW(PasswordHasher::verify("example-passphrase", badChar), std::invalid_argument);
}

TEST(PasswordHasher, T_PASSWORD_05_upper_case_hex_verifies_and_empty_password_is_refused) {
    using lms::security::PasswordHash;
    using lms::security::PasswordHasher;
    const auto good = PasswordHasher::hash("example-passphrase");
    PasswordHash upper = good;
    for (char& c : upper.hashHex)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (char& c : upper.saltHex)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    EXPECT_TRUE(PasswordHasher::verify("example-passphrase", upper));
    EXPECT_THROW(PasswordHasher::hash(""), std::invalid_argument);
}
