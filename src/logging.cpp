/**
 * This file implements the event logging API in d2/logging.hpp.
 */

#define D2_SOURCE
#include <boost/mpl/apply.hpp>
#include <boost/mpl/vector.hpp>
#include <d2/detail/basic_atomic.hpp>
#include <d2/detail/basic_mutex.hpp>
#include <d2/detail/config.hpp>
#include <d2/events/acquire_event.hpp>
#include <d2/events/join_event.hpp>
#include <d2/events/release_event.hpp>
#include <d2/events/segment_hop_event.hpp>
#include <d2/events/start_event.hpp>
#include <d2/filesystem_dispatcher.hpp>
#include <d2/logging.hpp>
#include <d2/repository.hpp>
#include <d2/sync_object.hpp>
#include <d2/thread.hpp>

#include <exception>
#include <string>


namespace d2 {

namespace detail {
namespace repository_setup {
struct SegmentationTag {
    template <typename Ostream>
    friend Ostream& operator<<(Ostream& os, SegmentationTag const&)
    { return os << "segmentation", os; }

    template <typename Istream>
    friend Istream& operator<<(Istream& is, SegmentationTag&) {
        std::string string;
        return is >> string, is;
    }
};

typedef boost::mpl::vector<Thread, SegmentationTag> Keys;

// Mapping policy for the repository: what is logged where.
struct MappingPolicy {
    template <typename Key, typename Stream> struct apply;

    // Each thread has its own sink. The mapping from Thread to sink uses
    // boost::unordered_map.
    template <typename Stream>
    struct apply<Thread, Stream>
        : boost::mpl::apply<boost_unordered_map, Thread, Stream>
    { };

    // There is another sink; it uses no map at all. It will contain
    // the events concerning segmentation.
    template <typename Stream>
    struct apply<SegmentationTag, Stream>
        : boost::mpl::apply<unary_map, SegmentationTag, Stream>
    { };
};

// Locking policy controlling the locks used on each stream.
struct StreamLockingPolicy {
    template <typename Key, typename Stream> struct apply;

    // Don't synchronize per-stream access to threads, because only one thread
    // at a time is going to write in it anyway.
    template <typename Stream>
    struct apply<Thread, Stream>
        : boost::mpl::apply<no_synchronization, Thread, Stream>
    { };

    // Use a basic_mutex to lock the stream in which we'll be logging
    // the segmentation related events.
    template <typename Stream>
    struct apply<SegmentationTag, Stream>
        : boost::mpl::apply<
            synchronize_with<basic_mutex>, SegmentationTag, Stream
        >
    { };
};

// Lock the mapping from thread to stream (and the dummy mapping to the
// segmentation stream) using a basic_mutex.
typedef synchronize_with<basic_mutex> GlobalLockingPolicy;

// Instantiate the Repository type.
typedef Repository<
            Keys, MappingPolicy, GlobalLockingPolicy, StreamLockingPolicy
        > EventRepository;
} // end namespace repository_setup

static FilesystemDispatcher dispatcher;

D2_API extern void push_acquire(SyncObject const& s, Thread const& t,
                                                     unsigned int ignore) {
    if (is_enabled()) {
        AcquireEvent event(s, t);
        event.info.init_call_stack(ignore + 1); // ignore current frame
        dispatcher.dispatch(event);
    }
}

D2_API extern void push_release(SyncObject const& s, Thread const& t) {
    if (is_enabled())
        dispatcher.dispatch(ReleaseEvent(s, t));
}

static basic_mutex segment_lock;
static Segment current_segment; // initialized to the initial segment value
static boost::unordered_map<Thread, Segment> segment_of;

namespace {
    template <typename Value, typename Container>
    bool contains(Value const& v, Container const& c) {
        return c.find(v) != c.end();
    }
}

D2_API extern void push_start(Thread const& parent, Thread const& child) {
    if (is_enabled()) {
        segment_lock.lock();
        BOOST_ASSERT_MSG(parent != child, "thread starting itself");
        BOOST_ASSERT_MSG(segment_of.empty() || contains(parent, segment_of),
        "starting a thread from another thread that has not been created yet");
        // segment_of[parent] will be the initial segment value on the very
        // first call, which is the same as current_segment. so this means
        // two things:
        //  - parent_segment will be the initial segment value on the very
        //    first call, and the segment of `parent` on subsequent calls,
        //    which is fine.
        //  - we must PREincrement the current_segment so it is distinct from
        //    the initial value.
        Segment parent_segment = segment_of[parent];
        Segment new_parent_segment = ++current_segment;
        Segment child_segment = ++current_segment;
        segment_of[child] = child_segment;
        segment_of[parent] = new_parent_segment;
        segment_lock.unlock();

        dispatcher.dispatch(StartEvent(parent_segment, new_parent_segment,
                                                       child_segment));
        dispatcher.dispatch(SegmentHopEvent(parent, new_parent_segment));
        dispatcher.dispatch(SegmentHopEvent(child, child_segment));
    }
}

D2_API extern void push_join(Thread const& parent, Thread const& child) {
    if (is_enabled()) {
        segment_lock.lock();
        BOOST_ASSERT_MSG(parent != child, "thread joining itself");
        BOOST_ASSERT_MSG(contains(parent, segment_of),
        "joining a thread into another thread that has not been created yet");
        BOOST_ASSERT_MSG(contains(child, segment_of),
                            "joining a thread that has not been created yet");
        Segment parent_segment = segment_of[parent];
        Segment child_segment = segment_of[child];
        Segment new_parent_segment = ++current_segment;
        segment_of[parent] = new_parent_segment;
        segment_of.erase(child);
        segment_lock.unlock();

        dispatcher.dispatch(JoinEvent(parent_segment, new_parent_segment,
                                                      child_segment));
        dispatcher.dispatch(SegmentHopEvent(parent, new_parent_segment));
        // We could possibly generate informative events like end-of-thread
        // in the child thread, but that's not strictly necessary right now.
    }
}
} // end namespace detail

D2_API extern bool set_log_repository(std::string const& path) {
    try {
        detail::dispatcher.set_root(path);
        // We really don't want to propagate exceptions across the API
        // boundary. Users might not be using exceptions, or might not
        // want to check the return status anyway (probable).
    } catch(std::exception const&) {
        return false;
    }
    return true;
}

namespace {
    static detail::basic_atomic<bool> event_logging_enabled(false);
}
D2_API extern void disable_event_logging() {
    event_logging_enabled = false;
}

D2_API extern void enable_event_logging() {
    event_logging_enabled = true;
}

D2_API extern bool is_enabled() {
    return event_logging_enabled;
}

} // end namespace d2
