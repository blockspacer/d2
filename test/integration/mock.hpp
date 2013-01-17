/**
 * This file defines mock mutex and thread implementations
 * for testing purposes.
 */

#ifndef TEST_MOCK_HPP
#define TEST_MOCK_HPP

#include <d2/detail/basic_atomic.hpp>

#include <boost/function.hpp>
#include <boost/move/move.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <cstddef>
#include <string>


namespace d2 {
namespace mock {

struct integration_test {
    integration_test(int argc, char const* argv[], std::string const& file);
    ~integration_test();
};

class thread {
    boost::scoped_ptr<boost::thread> actual_;
    boost::function<void()> f_;

public:
    class id {
        boost::thread::id id_;

    public:
        /* implicit */ id(boost::thread::id const& thread_id);

        friend std::size_t unique_id(id const& self);
    };

    explicit thread(boost::function<void()> const& f);

    thread(BOOST_RV_REF(thread) other);

    friend void swap(thread& a, thread& b);

    void start();

    void join();
};

namespace this_thread {
    extern thread::id get_id();
}

class mutex {
    static d2::detail::basic_atomic<std::size_t> counter;
    std::size_t id_;

public:
    mutex();

    void lock() const;

    void unlock() const;

    friend std::size_t unique_id(mutex const& self);
};

class recursive_mutex : public mutex {
public:
    void lock() const;

    void unlock() const;
};

} // end namespace mock
} // end namespace d2

#endif // !TEST_MOCK_HPP
