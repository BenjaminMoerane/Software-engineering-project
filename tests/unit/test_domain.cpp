// Domain layer: roles and the abstract User hierarchy (Factory + polymorphism).
#include "domain/User.hpp"

#include <gtest/gtest.h>

using namespace lms::domain;

namespace {
Session sessionFor(Role role) {
    Session s;
    s.userId = 1;
    s.username = "u";
    s.fullName = "U";
    s.role = role;
    return s;
}
} // namespace

TEST(Domain, T_ROLE_01_role_names_round_trip) {
    for (Role r : {Role::Administrator, Role::Lecturer, Role::HeadOfDepartment, Role::Student})
        EXPECT_EQ(parseRole(roleName(r)), r);
}

TEST(Domain, T_ROLE_02_unknown_role_name_rejected) {
    EXPECT_THROW(parseRole("Principal"), std::invalid_argument);
    EXPECT_THROW(parseRole(""), std::invalid_argument);
    EXPECT_THROW(parseRole("administrator"), std::invalid_argument); // exact, case-sensitive
}

TEST(Domain, T_ROLE_03_unknown_enum_value_is_a_logic_error) {
    EXPECT_THROW(roleName(static_cast<Role>(99)), std::logic_error);
    EXPECT_THROW(makeUser(sessionFor(static_cast<Role>(99))), std::logic_error);
}

TEST(Domain, T_ROLE_04_factory_returns_the_right_polymorphic_type) {
    struct Expect {
        Role role;
        bool administer, teach, moderate, submit;
    };
    const Expect table[] = {{Role::Administrator, true, false, false, false},
                            {Role::Lecturer, false, true, false, false},
                            {Role::HeadOfDepartment, false, false, true, false},
                            {Role::Student, false, false, false, true}};
    for (const auto& e : table) {
        const std::unique_ptr<User> user = makeUser(sessionFor(e.role));
        ASSERT_NE(user, nullptr);
        EXPECT_EQ(user->canAdminister(), e.administer) << roleName(e.role);
        EXPECT_EQ(user->canTeach(), e.teach) << roleName(e.role);
        EXPECT_EQ(user->canModerate(), e.moderate) << roleName(e.role);
        EXPECT_EQ(user->canSubmit(), e.submit) << roleName(e.role);
        EXPECT_EQ(user->session().role, e.role);
    }
}
