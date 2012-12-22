
#include "mock.hpp"
#include <d2/logging.hpp>

#include <algorithm>
#include <boost/move/move.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <cstddef>
#include <ostream>
#include <vector>


static std::size_t const THREADS = 1000;
static std::size_t const NOISE_MUTEXES_PER_THREAD = 100;

int main() {
    auto noise = [&] {
        std::vector<mock_mutex> mutexes(NOISE_MUTEXES_PER_THREAD);
        boost::for_each(mutexes, [](mock_mutex& m) { m.lock(); });
        boost::for_each(mutexes | boost::adaptors::reversed,
                                        [](mock_mutex& m) { m.unlock(); });
    };

    mock_mutex A, B;

    mock_thread t0([&] {
        A.lock();
            B.lock();
            B.unlock();
        A.unlock();
    });

    mock_thread t1([&] {
        B.lock();
            A.lock();
            A.unlock();
        B.unlock();
    });

    std::vector<mock_thread> threads;
    threads.push_back(boost::move(t0)); threads.push_back(boost::move(t1));
    std::generate_n(boost::back_move_inserter(threads), THREADS - 2, [&] {
        return mock_thread(noise);
    });
    boost::range::random_shuffle(threads);

    d2::set_event_sink(&std::cout);
    d2::enable_event_logging();

    boost::for_each(threads, [](mock_thread& t) { t.start(); });
    boost::for_each(threads, [](mock_thread& t) { t.join(); });

    d2::disable_event_logging();
}
