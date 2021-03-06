/**
 * This file contains unit tests for the segmentation graph construction.
 */

#include <d2/core/build_segmentation_graph.hpp>
#include <d2/core/events.hpp>
#include <d2/core/exceptions.hpp>
#include <d2/core/segment.hpp>
#include <d2/core/segmentation_graph.hpp>

#include <boost/assign.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/variant.hpp>
#include <gtest/gtest.h>
#include <iostream>
#include <vector>


using namespace d2;
using namespace d2::core;

namespace {
typedef d2::core::events::start StartEvent;
typedef d2::core::events::join JoinEvent;
typedef d2::core::events::acquire AcquireEvent;

struct test_segmentation_graph : testing::Test {
    std::vector<core::events::non_thread_specific> events;
    SegmentationGraph graph;
    std::vector<Segment> segments;

    void SetUp() {
        for (unsigned int i = 0; i < 1000; ++i)
            segments.push_back(Segment() + i);
    }

    void TearDown() {
        if (HasFailure()) {
            std::clog << "Test failed, printing the segmentation graph:\n";
            boost::write_graphviz(std::clog, graph);
        }
    }
};

static bool const ignore_other_events = true;
static bool const dont_ignore_other_events = false;

TEST_F(test_segmentation_graph, no_events_create_empty_graph) {
    build_segmentation_graph<ignore_other_events>(events, graph);

    ASSERT_EQ(0, num_vertices(graph));
}

TEST_F(test_segmentation_graph, test_one_start_event_adds_right_edges) {
    using namespace boost::assign;
    //      0   1   2
    // t0   o___o
    // t1   |_______o

    events += StartEvent(segments[0], segments[1], segments[2]);

    build_segmentation_graph<ignore_other_events>(events, graph);
    ASSERT_EQ(3, num_vertices(graph));

    EXPECT_TRUE(happens_before(segments[0], segments[1], graph));
    EXPECT_TRUE(happens_before(segments[0], segments[2], graph));

    EXPECT_FALSE(happens_before(segments[1], segments[2], graph));
    EXPECT_FALSE(happens_before(segments[2], segments[1], graph));
}

TEST_F(test_segmentation_graph, simple_start_and_join) {
    using namespace boost::assign;
    //      0   1   2   3
    // t0   o___o_______o
    // t1   |_______o___|

    events +=
        StartEvent(segments[0], segments[1], segments[2]),
        JoinEvent(segments[1], segments[3], segments[2])
    ;

    build_segmentation_graph<ignore_other_events>(events, graph);
    ASSERT_EQ(4, num_vertices(graph));

    EXPECT_TRUE(happens_before(segments[0], segments[1], graph));
    EXPECT_TRUE(happens_before(segments[0], segments[2], graph));
    EXPECT_TRUE(happens_before(segments[0], segments[3], graph));

    EXPECT_FALSE(happens_before(segments[1], segments[2], graph));

    EXPECT_TRUE(happens_before(segments[0], segments[3], graph));
    EXPECT_TRUE(happens_before(segments[1], segments[3], graph));
    EXPECT_TRUE(happens_before(segments[2], segments[3], graph));
}

TEST_F(test_segmentation_graph, throws_on_unexpected_event_when_told_to) {
    using namespace boost::assign;
    typedef boost::variant<StartEvent, JoinEvent, AcquireEvent> Events;
    std::vector<Events> events;
    events +=
        StartEvent(segments[0], segments[1], segments[2]),
        AcquireEvent(),
        JoinEvent(segments[1], segments[3], segments[2])
    ;

    ASSERT_THROW(
        build_segmentation_graph<dont_ignore_other_events>(events, graph)
    , EventTypeException);
}

TEST_F(test_segmentation_graph,
                has_strong_guarantee_when_first_event_is_not_a_start_event) {
    using namespace boost::assign;
    events +=
        // Note: join comes before start.
        JoinEvent(segments[1], segments[3], segments[2]),
        StartEvent(segments[0], segments[1], segments[2])
    ;

    // It should throw because the first event is not a StartEvent as expected.
    ASSERT_THROW(
        build_segmentation_graph<ignore_other_events>(events, graph)
    , EventTypeException);

    // It should leave the graph untouched.
    ASSERT_EQ(0, num_vertices(graph));
}

TEST_F(test_segmentation_graph, multiple_starts_from_main_thread) {
    using namespace boost::assign;
    //      0   1   2   3   4   5   6
    // t0   o___o_______o_______o___o
    // t1   |___|___o___________|   |
    // t2       |___________o_______|

    events +=
        StartEvent(segments[0], segments[1], segments[2]),
        StartEvent(segments[1], segments[3], segments[4]),
        JoinEvent(segments[3], segments[5], segments[2]),
        JoinEvent(segments[5], segments[6], segments[4])
    ;

    build_segmentation_graph<ignore_other_events>(events, graph);
    ASSERT_EQ(7, num_vertices(graph));

    EXPECT_FALSE(happens_before(segments[0], segments[0], graph));
    EXPECT_TRUE(happens_before(segments[0], segments[1], graph));
    EXPECT_TRUE(happens_before(segments[0], segments[2], graph));
    EXPECT_TRUE(happens_before(segments[0], segments[3], graph));
    EXPECT_TRUE(happens_before(segments[0], segments[4], graph));
    EXPECT_TRUE(happens_before(segments[0], segments[5], graph));
    EXPECT_TRUE(happens_before(segments[0], segments[6], graph));

    EXPECT_FALSE(happens_before(segments[1], segments[0], graph));
    EXPECT_FALSE(happens_before(segments[1], segments[1], graph));
    EXPECT_FALSE(happens_before(segments[1], segments[2], graph));
    EXPECT_TRUE(happens_before(segments[1], segments[3], graph));
    EXPECT_TRUE(happens_before(segments[1], segments[4], graph));
    EXPECT_TRUE(happens_before(segments[1], segments[5], graph));
    EXPECT_TRUE(happens_before(segments[1], segments[6], graph));

    EXPECT_FALSE(happens_before(segments[2], segments[0], graph));
    EXPECT_FALSE(happens_before(segments[2], segments[1], graph));
    EXPECT_FALSE(happens_before(segments[2], segments[2], graph));
    EXPECT_FALSE(happens_before(segments[2], segments[3], graph));
    EXPECT_FALSE(happens_before(segments[2], segments[4], graph));
    EXPECT_TRUE(happens_before(segments[2], segments[5], graph));
    EXPECT_TRUE(happens_before(segments[2], segments[6], graph));

    EXPECT_FALSE(happens_before(segments[3], segments[0], graph));
    EXPECT_FALSE(happens_before(segments[3], segments[1], graph));
    EXPECT_FALSE(happens_before(segments[3], segments[2], graph));
    EXPECT_FALSE(happens_before(segments[3], segments[3], graph));
    EXPECT_FALSE(happens_before(segments[3], segments[4], graph));
    EXPECT_TRUE(happens_before(segments[3], segments[5], graph));
    EXPECT_TRUE(happens_before(segments[3], segments[6], graph));

    EXPECT_FALSE(happens_before(segments[4], segments[0], graph));
    EXPECT_FALSE(happens_before(segments[4], segments[1], graph));
    EXPECT_FALSE(happens_before(segments[4], segments[2], graph));
    EXPECT_FALSE(happens_before(segments[4], segments[3], graph));
    EXPECT_FALSE(happens_before(segments[4], segments[4], graph));
    EXPECT_FALSE(happens_before(segments[4], segments[5], graph));
    EXPECT_TRUE(happens_before(segments[4], segments[6], graph));

    EXPECT_FALSE(happens_before(segments[5], segments[0], graph));
    EXPECT_FALSE(happens_before(segments[5], segments[1], graph));
    EXPECT_FALSE(happens_before(segments[5], segments[2], graph));
    EXPECT_FALSE(happens_before(segments[5], segments[3], graph));
    EXPECT_FALSE(happens_before(segments[5], segments[4], graph));
    EXPECT_FALSE(happens_before(segments[5], segments[5], graph));
    EXPECT_TRUE(happens_before(segments[5], segments[6], graph));

    EXPECT_FALSE(happens_before(segments[6], segments[0], graph));
    EXPECT_FALSE(happens_before(segments[6], segments[1], graph));
    EXPECT_FALSE(happens_before(segments[6], segments[2], graph));
    EXPECT_FALSE(happens_before(segments[6], segments[3], graph));
    EXPECT_FALSE(happens_before(segments[6], segments[4], graph));
    EXPECT_FALSE(happens_before(segments[6], segments[5], graph));
    EXPECT_FALSE(happens_before(segments[6], segments[6], graph));
}
} // end anonymous namespace
