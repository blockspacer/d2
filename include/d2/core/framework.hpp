/**
 * This file defines the `d2::core::framework` class.
 */

#ifndef D2_CORE_FRAMEWORK_HPP
#define D2_CORE_FRAMEWORK_HPP

#include <d2/core/events.hpp>
#include <d2/core/framework_fwd.hpp>
#include <d2/core/lock_id.hpp>
#include <d2/core/segment.hpp>
#include <d2/core/thread_id.hpp>
#include <d2/detail/atomic.hpp>
#include <d2/detail/mutex.hpp>

#include <boost/assert.hpp>
#include <boost/thread/lock_guard.hpp>
#include <cstddef>
#include <cstdlib>


namespace d2 {
namespace core {
namespace framework_detail {
    template <typename Value, typename Container>
    bool contains(Value const& v, Container const& c) {
        return c.find(v) != c.end();
    }
}

framework::framework() : event_logging_enabled_(false) {
    char const* repo = std::getenv("D2_REPOSITORY");
    if (repo) {
        enable();
        set_repository(repo);
    }
}

framework::~framework() {
    if (is_enabled()) {
        disable();
        unset_repository();
    }
}

void framework::enable() { event_logging_enabled_ = true; }
void framework::disable() { event_logging_enabled_ = false; }

bool framework::is_enabled() const { return event_logging_enabled_; }
bool framework::is_disabled() const { return !is_enabled(); }

int framework::set_repository(char const* path) {
    // Note: 0 for success and anything else but 0 for failure.
    return dispatcher_.set_repository_noexcept(path) ? 0 : 1;
}

void framework::unset_repository() {
    dispatcher_.unset_repository();
}

void framework::notify_acquire(std::size_t thread_id, std::size_t lock_id) {
    if (is_enabled()) {
        events::acquire event((ThreadId(thread_id)), LockId(lock_id));
                        // ignore current frame
        event.info.init_call_stack(1);
        dispatcher_.dispatch(event);
    }
}

void framework::notify_release(std::size_t thread_id, std::size_t lock_id) {
    if (is_enabled())
        dispatcher_.dispatch(
            events::release(ThreadId(thread_id), LockId(lock_id)));
}

void framework::notify_recursive_acquire(std::size_t thread_id, std::size_t lock_id) {
    if (is_enabled()) {
        events::recursive_acquire event((ThreadId(thread_id)), LockId(lock_id));
                        // ignore current frame
        event.info.init_call_stack(1);
        dispatcher_.dispatch(event);
    }
}

void framework::notify_recursive_release(std::size_t thread_id, std::size_t lock_id) {
    if (is_enabled())
        dispatcher_.dispatch(
            events::recursive_release(ThreadId(thread_id), LockId(lock_id)));
}

void framework::notify_start(std::size_t parent_id, std::size_t child_id) {
    if (is_disabled())
        return;

    ThreadId parent(parent_id), child(child_id);
    Segment parent_segment, child_segment, new_parent_segment;
    {
        boost::lock_guard<detail::mutex> lock(segment_mutex);
        BOOST_ASSERT_MSG(parent != child, "thread starting itself");
        BOOST_ASSERT_MSG(segment_of.empty() || framework_detail::contains(parent,segment_of),
    "starting a thread from another thread that has not been created yet");
        // segment_of[parent] will be the initial segment value on the
        // very first call, which is the same as current_segment. so this
        // means two things:
        //  - parent_segment will be the initial segment value on the very
        //    first call, and the segment of `parent` on subsequent calls,
        //    which is fine.
        //  - we must PREincrement the current_segment so it is distinct
        //    from the initial value.
        Segment& segment_of_parent = segment_of[parent];
        parent_segment = segment_of_parent;
        new_parent_segment = ++current_segment;
        child_segment = ++current_segment;
        segment_of[child] = child_segment;
        segment_of_parent = new_parent_segment;
    }

    dispatcher_.dispatch(
        events::start(parent_segment, new_parent_segment, child_segment));

    dispatcher_.dispatch(
        events::segment_hop(parent, new_parent_segment));

    dispatcher_.dispatch(
        events::segment_hop(child, child_segment));
}

void framework::notify_join(std::size_t parent_id, std::size_t child_id) {
    if (is_disabled())
        return;

    ThreadId parent(parent_id), child(child_id);
    Segment parent_segment, child_segment, new_parent_segment;
    {
        detail::scoped_lock<detail::mutex> lock(segment_mutex);
        BOOST_ASSERT_MSG(parent != child, "thread joining itself");
        BOOST_ASSERT_MSG(framework_detail::contains(parent, segment_of),
    "joining a thread into another thread that has not been created yet");
        BOOST_ASSERT_MSG(framework_detail::contains(child, segment_of),
                        "joining a thread that has not been created yet");
        Segment& segment_of_parent = segment_of[parent];
        parent_segment = segment_of_parent;
        child_segment = segment_of[child];
        new_parent_segment = ++current_segment;
        segment_of_parent = new_parent_segment;
        segment_of.erase(child);
    }

    dispatcher_.dispatch(
        events::join(parent_segment, new_parent_segment, child_segment));

    dispatcher_.dispatch(events::segment_hop(parent, new_parent_segment));
    // We could possibly generate informative events like end-of-thread
    // in the child thread, but that's not strictly necessary right now.
}
} // end namespace core
} // end namespace d2

#endif // !D2_CORE_FRAMEWORK_HPP
