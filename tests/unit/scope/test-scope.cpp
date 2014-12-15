#include <scope/scope.h>

#include <core/posix/exec.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <unity/scopes/SearchReply.h>
#include <unity/scopes/SearchReplyProxyFwd.h>
#include <unity/scopes/Variant.h>
#include <unity/scopes/testing/Category.h>
#include <unity/scopes/testing/MockSearchReply.h>
#include <unity/scopes/testing/TypedScopeFixture.h>

using namespace std;
using namespace testing;
using namespace scope;

namespace posix = core::posix;
namespace sc = unity::scopes;
namespace sct = unity::scopes::testing;

/**
 * Keep the tests in an anonymous namespace
 */
namespace {

/**
 * Custom matcher to check the properties of search results
 */
MATCHER_P2(ResultProp, prop, value, "") {
    if (arg.contains(prop)) {
        *result_listener << "result[" << prop << "] is " << arg[prop].serialize_json();
    } else {
        *result_listener << "result[" << prop << "] is not set";
    }
    return arg.contains(prop) && arg[prop] == sc::Variant(value);
}

/**
 * Custom matcher to check the presence of departments
 */
MATCHER_P(IsDepartment, department, "") {
    return arg->serialize() == department->serialize();
}

typedef sct::TypedScopeFixture<Scope> TypedScopeFixtureScope;

class TestScope: public TypedScopeFixtureScope {
protected:
};

TEST_F(TestScope, empty_search_string) {
}

TEST_F(TestScope, search) {
}

} // namespace

