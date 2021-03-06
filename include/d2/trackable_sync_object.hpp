/*!
 * @file
 * This file implements the `d2::trackable_sync_object` class.
 */

#ifndef D2_TRACKABLE_SYNC_OBJECT_HPP
#define D2_TRACKABLE_SYNC_OBJECT_HPP

#include <d2/core/raw_api.hpp>
#include <d2/detail/decl.hpp>
#include <d2/detail/ut_access.hpp>

#include <boost/config.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/mpl/or.hpp>
#include <boost/type_traits/is_same.hpp>
#include <cstddef>
#include <dyno/thread_id.hpp>
#include <dyno/uniquely_identifiable.hpp>


namespace d2 {
namespace trackable_sync_object_detail {
    //! @internal Class holding an identifier unique across all locks.
    struct unique_id_for_all_locks
        : dyno::uniquely_identifiable<unique_id_for_all_locks>
    { };
} // end namespace trackable_sync_object_detail

/*!
 * Tag to signal that it is legal for a synchronization object to be acquired
 * recursively by the same thread.
 */
struct recursive;

/*!
 * Tag to signal that it is not legal for a synchronization object to be
 * acquired recursively by the same thread.
 */
struct non_recursive;

/*!
 * Class providing basic facilities to notify the acquisition and the release
 * of synchronization objects to `d2`.
 *
 * An instance of this class must be associated with a single synchronization
 * object. The `notify_lock()` and `notify_unlock()` methods must be called
 * as appropriate to notify the library of an acquisition or release of the
 * associated synchronization object.
 *
 * The easiest way to achieve this is the following:
 * @code
 *
 *  class my_sync_object : private d2::trackable_sync_object<d2::non_recursive>
 *  {
 *  public:
 *      void lock() {
 *          // ...
 *          this->notify_lock();
 *      }
 *
 *      void unlock() {
 *          // ...
 *          this->notify_unlock();
 *      }
 *  };
 *
 * @endcode
 *
 * @note Using private inheritance should be preferred when possible for
 *       the following reasons:
 *          - It opens the door for the empty base class optimization.
 *          - It ensures a one-to-one correspondence between synchronization
 *            objects and `d2::trackable_sync_object`s without hassle.
 *          - It does not alter the public interface of the derived class.
 *
 * @tparam Recursive
 *         Tag signalling whether it is legal for a synchronization object
 *         to be acquired recursively by the same thread. It must be one of
 *         `d2::non_recursive` and `d2::recursive`.
 */
template <typename Recursive>
class trackable_sync_object {
    trackable_sync_object_detail::unique_id_for_all_locks lock_id_;

    // No need to evaluate the metafunctions with the current usage.
    typedef boost::is_same<Recursive, recursive> is_recursive;
    typedef boost::is_same<Recursive, non_recursive> is_non_recursive;

    BOOST_MPL_ASSERT((boost::mpl::or_<is_recursive, is_non_recursive>));

    friend class detail::ut_access;
    std::size_t d2_unique_id() const {
        using dyno::unique_id;
        return unique_id(lock_id_);
    }

public:
    /*!
     * Notify `d2` of the acquisition of this synchronization object by the
     * current thread.
     */
    void notify_lock() const BOOST_NOEXCEPT {
#ifdef D2_ENABLED
        using dyno::unique_id;
        std::size_t const tid = unique_id(dyno::this_thread::get_id());
        if (::d2::trackable_sync_object<Recursive>::is_recursive::value)
            core::notify_recursive_acquire(tid, unique_id(lock_id_));
        else
            core::notify_acquire(tid, unique_id(lock_id_));
#endif
    }

    /*!
     * Notify `d2` of the release of this synchronization object by the
     * current thread.
     */
    void notify_unlock() const BOOST_NOEXCEPT {
#ifdef D2_ENABLED
        using dyno::unique_id;
        std::size_t const tid = unique_id(dyno::this_thread::get_id());
        if (::d2::trackable_sync_object<Recursive>::is_recursive::value)
            core::notify_recursive_release(tid, unique_id(lock_id_));
        else
            core::notify_release(tid, unique_id(lock_id_));
#endif
    }
};
} // end namespace d2

#endif // !D2_TRACKABLE_SYNC_OBJECT_HPP
