/**
 * This file implements the `lockable` class.
 */

#ifndef D2_LOCKABLE_HPP
#define D2_LOCKABLE_HPP

#include <d2/basic_lockable.hpp>
#include <d2/detail/inherit_constructors.hpp>

#include <boost/config.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/thread/lockable_traits.hpp>


namespace d2 {
/**
 * Wrapper over a synchronization object modeling the `Lockable` concept.
 *
 * This wrapper augments the behavior of `basic_lockable` with the following:
 *  - When the `*this` is `try_lock()`ed successfully, `d2` is notified
 *    automatically.
 */
template <typename Lockable>
struct lockable : basic_lockable<Lockable> {

    D2_INHERIT_CONSTRUCTORS(lockable, basic_lockable<Lockable>)

    /**
     * Call the `try_lock()` method of `Lockable` and notify `d2` of the
     * acquisition of `*this` if and only if the acquisition succeeded.
     *
     * @return Whether the acquisition succeeded.
     */
    bool try_lock() BOOST_NOEXCEPT {
        if (basic_lockable<Lockable>::try_lock()) {
            notify_lock();
            return true;
        }
        return false;
    }
};
} // end namespace d2

namespace boost {
    namespace sync {
        template <typename L>
        class is_lockable<d2::lockable<L> >
            : public boost::mpl::true_
        { };
    }
}

#endif // !D2_LOCKABLE_HPP