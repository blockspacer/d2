/**
 * This file defines the interface to log events which constitute the trace
 * of an analyzed program.
 */

#ifndef D2_LOGGING_HPP
#define D2_LOGGING_HPP

#include <d2/events.hpp>
#include <d2/types.hpp>

#include <boost/concept_check.hpp>
#include <boost/range/iterator_range.hpp>
#include <iterator> // for std::istream_iterator
#include <ostream>
#include <string>


namespace d2 {
namespace detail {
    extern void push_event_impl(event const& e);

    // FIXME: We could dispatch cleverly depending on the event type and
    //        improve performances. For example, acquire/release events
    //        could be logged inside thread local storage.
    template <typename Event>
    void push_event(Event const& e) {
        push_event_impl(e);
    }
} // end namespace detail

/**
 * Notify the deadlock detection system of the acquisition of synchronization
 * object `s` by thread `t`.
 */
template <typename SyncObject, typename Thread>
void notify_acquire(SyncObject const& s, Thread const& t,
                                        std::string const& file, int line) {
    BOOST_CONCEPT_ASSERT((UniquelyIdentifiable<SyncObject>));
    BOOST_CONCEPT_ASSERT((UniquelyIdentifiable<Thread>));
    acquire_event e((sync_object(s)), thread(t));
    e.info.file = file;
    e.info.line = line;
    e.info.init_call_stack();
    detail::push_event(e);
}

template <typename SyncObject, typename Thread>
void notify_acquire(SyncObject const& s, Thread const& t) {
    notify_acquire(s, t, "no file information", -1);
}

/**
 * Notify the deadlock detection system of the release of synchronization
 * object `s` by thread `t`.
 */
template <typename SyncObject, typename Thread>
void notify_release(SyncObject const& s, Thread const& t) {
    BOOST_CONCEPT_ASSERT((UniquelyIdentifiable<SyncObject>));
    BOOST_CONCEPT_ASSERT((UniquelyIdentifiable<Thread>));
    detail::push_event(release_event(sync_object(s), thread(t)));
}

/**
 * Notify the deadlock detection system of the start of a new thread `child`
 * initiated by `parent`.
 */
template <typename Thread>
void notify_start(Thread const& parent, Thread const& child) {
    BOOST_CONCEPT_ASSERT((UniquelyIdentifiable<Thread>));
    detail::push_event(start_event(thread(parent), thread(child)));
}

/**
 * Notify the deadlock detection system of the join of thread `child` by
 * `parent`.
 */
template <typename Thread>
void notify_join(Thread const& parent, Thread const& child) {
    BOOST_CONCEPT_ASSERT((UniquelyIdentifiable<Thread>));
    detail::push_event(join_event(thread(parent), thread(child)));
}

/**
 * Set the sink to which events are written when logging of events is enabled.
 * A sink must be set before logging may start, i.e. before
 * `enable_event_logging` is called for the first time.
 * @note This operation can be considered atomic.
 * @note `sink` must be a valid pointer.
 * @note `*sink` must be in scope as long as the logging of events is enabled
 *       with that sink.
 * @note We use a pointer to make it explicit that only a reference to `sink`
 *       is taken.
 */
extern void set_event_sink(std::ostream* sink);

/**
 * Disable the logging of events by the deadlock detection framework.
 * @note This operation can be considered atomic.
 * @note This function is idempotent, i.e. calling it when the logging is
 *       already disabled is useless yet harmless.
 */
extern void disable_event_logging();

/**
 * Enable the logging of events by the deadlock detection framework.
 * The sink that is used is the same that was set last with `set_event_sink`.
 * @note This operation can be considered atomic.
 * @note This function is idempotent, i.e. calling it when the logging is
 *       already enabled is useless yet harmless.
 */
extern void enable_event_logging();

/**
 * Return a range of iterators lazily loading events from the specified
 * `source`. The source must have been created by the logging framework to
 * ensure it can be read correctly.
 * @note The iterators in the range are of an unspecified type. Apart from
 *       them modeling the InputIterator concept with a `value_type` of
 *       `event`, nothing is specified.
 */
template <typename Istream>
boost::iterator_range<std::istream_iterator<event> >
load_events(Istream& source) {
    typedef std::istream_iterator<event> Iterator;
    return boost::iterator_range<Iterator>(Iterator(source), Iterator());
}

} // end namespace d2

#endif // !D2_LOGGING_HPP