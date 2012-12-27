// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <test_o_matic.hpp>

#if defined(DBG_DISABLE_ASSERT)
#undef DBG_DISABLE_ASSERT
#endif

#if !defined(DBG_ENABLE_ASSERT)
#define DBG_ENABLE_ASSERT 1
#endif

#include "dbg/assert.hpp"
#include "fixtures.hpp"
#include "utils.hpp"

#include <string>

namespace
{
    std::string softdrinks_failure()
    {
        int coke = 1;
        int pepsi = 2;

        DBG_ASSERT_RELATION(coke, ==, pepsi);

        return DBG_FUNCTION;
    }

} // anonymous

TESTFIX("DBG_ASSERT_RELATION: nothing is done when relation holds", assert_fixture)
{
    DBG_ASSERT_RELATION(1, ==, 1);

    CHECK(log.empty());
    CHECK(fatalities_seen == 0);
}

TESTFIX("DBG_ASSERT_RELATION: fatality function is called when relation does not hold", assert_fixture)
{
    DBG_ASSERT_RELATION(1, ==, 2);
    CHECK(fatalities_seen == 1);
}

TESTFIX("DBG_ASSERT_RELATION: relation is mentioned when it does not hold", assert_fixture)
{
    int coke = 1;
    int pepsi = 2;

    DBG_ASSERT_RELATION(coke, ==, pepsi);

    CHECK(log_contains("condition: coke == pepsi"));
}

TESTFIX("DBG_ASSERT_RELATION: expressions being compared are mentioned in failure output", assert_fixture)
{
    int coke = 1;
    int pepsi = 2;

    DBG_ASSERT_RELATION(coke, >, pepsi);

    CHECK(log_contains("'coke': 1"));
    CHECK(log_contains("'pepsi': 2"));
}

TESTFIX("DBG_ASSERT_RELATION: source code position is mentioned when relation does not hold", assert_fixture)
{
    int coke = 1;
    int pepsi = 2;

    DBG_ASSERT_RELATION(coke, ==, pepsi);
    CHECK( log_contains("location: " __FILE__ ":" + to_string(__LINE__ - 1)) );
}

TESTFIX("DBG_ASSERT_RELATION: failing function is mentioned when relation does not hold", assert_fixture)
{
    std::string func = softdrinks_failure();
    CHECK(log_contains("function: " + func));
}

TESTFIX("DBG_ASSERT_RELATION: failed assertions mention call stack", assert_fixture)
{
    softdrinks_failure();

    // This isn't the greatest test in the world, but it's very hard to reliably 
    // stop compilers optimizing out certain functions.

    CHECK(log_contains("    0x")); // start of a PC address
}

TESTFIX("DBG_ASSERT_RELATION: DBG_TAGged variables are included in failure output", assert_fixture)
{
    int coke = 1;
    int pepsi = 2;
    std::string animal = "kestrel";

    DBG_ASSERT_RELATION(coke, ==, pepsi), DBG_TAG(animal);

    CHECK(log_contains("'coke': 1"));
    CHECK(log_contains("'pepsi': 2"));
    CHECK(log_contains("'animal': kestrel"));
}
