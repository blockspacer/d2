/**
 * This file contains unit tests for the `JoinEvent` event.
 */

#include "serialization_test.hpp"
#include <d2/events/join_event.hpp>
#include <d2/segment.hpp>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/utility/value_init.hpp>


namespace d2 {
namespace test {

struct JoinEventTest {
    typedef JoinEvent value_type;
    static boost::random::mt19937 generator;

    static value_type get_random_object() {
        // Segment values are [initial segment, initial segment + 10000]
        boost::random::uniform_int_distribution<> distribution(0, 10000);
        return value_type(Segment() + distribution(generator),
                          Segment() + distribution(generator),
                          Segment() + distribution(generator));
    }
};

boost::random::mt19937 JoinEventTest::generator = boost::initialized_value;

INSTANTIATE_TYPED_TEST_CASE_P(JoinEvent, SerializationTest, JoinEventTest);

} // end namespace test
} // end namespace d2
