/**
 * This file defines the lock graph data structure used during the post-mortem
 * program analysis.
 */

#ifndef D2_CORE_LOCK_GRAPH_HPP
#define D2_CORE_LOCK_GRAPH_HPP

#include <d2/core/lock_id.hpp>
#include <d2/core/segment.hpp>
#include <d2/core/thread_id.hpp>
#include <d2/detail/decl.hpp>
#include <d2/detail/lock_debug_info.hpp>

#include <boost/assert.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/named_graph.hpp>
#include <boost/make_shared.hpp>
#include <boost/move/move.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/operators.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_set.hpp>
#include <iosfwd>


namespace d2 {
namespace lock_graph_detail {
/**
 * Set whose underlying representation can be shared by several owners.
 *
 * @note This structure is optimized so that several duplicated read-only
 *       copies of it are space efficient.
 */
template <typename Set>
struct shared_set {
    typedef Set underlying_set_type;

    //! Construct an empty set.
    shared_set()
        : set_(boost::make_shared<underlying_set_type>())
    { }

    //! Construct a shared set sharing its underlying set with `other`.
    /*implicit*/ shared_set(shared_set const& other)
        : set_(other.set_)
    { }

    //! Construct a shared set with an underlying set equal to `other`.
    explicit shared_set(BOOST_RV_REF(underlying_set_type) other)
        : set_(boost::make_shared<underlying_set_type>(boost::move(other)))
    { }

    //! Construct a shared set with an underlying set equal to `other`.
    explicit shared_set(underlying_set_type const& other)
        : set_(boost::make_shared<underlying_set_type>(other))
    { }

    //! Return a constant reference to the underlying set of `*this`.
    operator underlying_set_type const&() const {
        BOOST_ASSERT_MSG(set_, "invariant broken: the shared_ptr of a "
                               "shared set instance is invalid");
        return *set_;
    }

private:
    boost::shared_ptr<underlying_set_type const> set_;
};

/**
 * Set of locks held by a thread.
 *
 * @note We use a `shared_set` because an instance of `Gatelocks` is stored
 *       on each edge of the lock graph. We could use a `flyweight` too, but
 *       benchmarking shows that the current solution offers a better
 *       space/time tradeoff.
 *       The main differences between the two approaches are:
 *       - `flyweight` requires the set to be hashed everytime, which is
 *         more CPU intensive.
 *       - Using a `shared_set` is suboptimal because there may be some
 *         repetition of the gatelocks in the lock graph when the gatelocks
 *         are the same on different events.
 */
typedef shared_set<
            boost::multi_index_container<
                LockId,
                boost::multi_index::indexed_by<
                    boost::multi_index::hashed_unique<boost::multi_index::identity<LockId> >,
                    boost::multi_index::sequenced<>
                >
            >
        > Gatelocks;

/**
 * Label stored on each edge of a lock graph.
 */
struct LockGraphLabel : boost::equality_comparable<LockGraphLabel> {
    LockGraphLabel(Segment s1, ThreadId thread,
                   Gatelocks const& gatelocks, Segment s2)
        : s1(s1), s2(s2), thread_(thread), gatelocks_(gatelocks)
    { }

    detail::LockDebugInfo l1_info, l2_info;
    Segment s1, s2;

    friend Gatelocks::underlying_set_type const&
    gatelocks_of(LockGraphLabel const& self) {
        return self.gatelocks_;
    }

    friend ThreadId const& thread_of(LockGraphLabel const& self) {
        return self.thread_;
    }

    friend bool operator==(LockGraphLabel const& a, LockGraphLabel const& b) {
        // Note: We test the easiest first, i.e. the threads and segments,
        //       which are susceptible of being similar to integers.
        return a.s1 == b.s1 &&
               a.s2 == b.s2 &&
               thread_of(a) == thread_of(b) &&
               a.l1_info == b.l1_info &&
               a.l2_info == b.l2_info &&
               gatelocks_of(a).get<1>() == gatelocks_of(b).get<1>();
    }

    D2_DECL friend
    std::ostream& operator<<(std::ostream&, LockGraphLabel const&);

private:
    ThreadId thread_;
    Gatelocks gatelocks_;
};

/**
 * Directed graph representing the contexts in which synchronization objects
 * were acquired by threads.
 */
typedef boost::adjacency_list<
            boost::vecS, boost::vecS, boost::directedS, LockId, LockGraphLabel
        > LockGraph;

} // end namespace lock_graph_detail

namespace core {
    using lock_graph_detail::LockGraph;
    using lock_graph_detail::LockGraphLabel;
    using lock_graph_detail::Gatelocks;
}
} // end namespace d2

namespace boost {
namespace graph {
    // This is to be able to refer to a vertex in the lock graph using the
    // LockId associated to it.
    template <> struct internal_vertex_name<d2::LockId> {
        typedef multi_index::identity<d2::LockId> type;
    };
} // end namespace graph
} // end namespace boost

#endif // !D2_CORE_LOCK_GRAPH_HPP
