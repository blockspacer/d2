/**
 * This file defines the `EventSink` class.
 */

#ifndef D2_EVENT_SINK_HPP
#define D2_EVENT_SINK_HPP

#include <d2/detail/config.hpp>
#include <d2/events.hpp>


namespace d2 {

/**
 * Interface used to log the events generated by the notification functions.
 * Runtime polymorphism is used because:
 *  - The ability to change sinks at runtime is desirable.
 *  - Having a stable ABI is a must because classes in this hierarchy will
 *    interact directly with client code.
 *  - The ability to customize the behavior of the event sink is desirable.
 */
class D2_API EventSink {
public:
    virtual void write(AcquireEvent const& event) = 0;
    virtual void write(ReleaseEvent const& event) = 0;
    virtual void write(StartEvent const& event) = 0;
    virtual void write(JoinEvent const& event) = 0;
    virtual ~EventSink();
};

namespace detail {
    struct OstreamWrapper {
        virtual void put(char c) = 0;
    };

    template <typename Ostream>
    struct OstreamHolder : OstreamWrapper {
        Ostream& os_;

        explicit OstreamHolder(Ostream& os) : os_(os) { }

        virtual void put(char c) {
            os_.put(c);
        }
    };

    D2_API extern void generate(OstreamWrapper& os, Event const& event);
} // end namespace detail

/**
 * Adaptor to create an `EventSink` from an object implementing the `ostream`
 * interface.
 * @note This is probably going to be removed soon in favor of an event sink
 *       handling a repository.
 */
template <typename Ostream>
class OstreamEventSink : public EventSink {
    detail::OstreamHolder<Ostream> os_;

public:
    explicit OstreamEventSink(Ostream& os) : os_(os) { }

    virtual void write(AcquireEvent const& event) {
        detail::generate(os_, event);
    }

    virtual void write(ReleaseEvent const& event) {
        detail::generate(os_, event);
    }

    virtual void write(StartEvent const& event) {
        detail::generate(os_, event);
    }

    virtual void write(JoinEvent const& event) {
        detail::generate(os_, event);
    }
};

/**
 * Simple factory to create `OstreamEventSink`s.
 */
template <typename Ostream>
OstreamEventSink<Ostream> make_ostream_event_sink(Ostream& os) {
    return OstreamEventSink<Ostream>(os);
}

} // end namespace d2

#endif // !D2_EVENT_SINK_HPP
