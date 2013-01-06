/**
 * This file defines the `SegmentHopEvent` class.
 */

#ifndef D2_SEGMENT_HOP_EVENT_HPP
#define D2_SEGMENT_HOP_EVENT_HPP

#include <d2/event_traits.hpp>
#include <d2/segment.hpp>
#include <d2/thread.hpp>

#include <boost/operators.hpp>
#include <iosfwd>


namespace d2 {

/**
 * Represents the entrance of a thread into a new segment. This happens when
 * a thread starts a child thread or when a thread is a child thread itself.
 */
struct SegmentHopEvent : boost::equality_comparable<SegmentHopEvent> {
    Thread thread;
    Segment segment;

    /**
     * This constructor must only be used when serializing events.
     * The object is in an invalid state once default-constructed.
     */
    inline SegmentHopEvent() { }

    inline SegmentHopEvent(Thread const& thread, Segment const& segment)
        : thread(thread), segment(segment)
    { }

    /**
     * Return whether two `SegmentHopEvent`s represent the same thread
     * entering the same segment.
     */
    friend bool operator==(SegmentHopEvent const& a,
                           SegmentHopEvent const& b) {
        return a.thread == b.thread && a.segment == b.segment;
    }

    friend Thread thread_of(SegmentHopEvent const& self) {
        return self.thread;
    }

    template <typename Ostream>
    friend Ostream& operator<<(Ostream& os, SegmentHopEvent const& self) {
        os << self.thread << '>' << self.segment << '>';
        return os;
    }

    template <typename Istream>
    friend Istream& operator>>(Istream& is, SegmentHopEvent& self) {
        char gt;
        is >> self.thread >> gt >> self.segment >> gt;
        return is;
    }

    typedef thread_scope event_scope;
    typedef strict_order_policy ordering_policy;
};

} // end namespace d2

#endif // !D2_SEGMENT_HOP_EVENT_HPP
