
#include "mock.hpp"
#include <d2/logging.hpp>

#include <ostream>


int main() {
    d2::OstreamEventSink<std::ostream> sink(std::cout);
    d2::set_event_sink(&sink);
    d2::enable_event_logging();

    mock::mutex A, B;

    mock::thread t0([&] {
        A.lock();
            B.lock();
            B.unlock();
        A.unlock();
    });

    mock::thread t1([&] {
        B.lock();
            A.lock();
            A.unlock();
        B.unlock();
    });

    t0.start();
    t1.start();

    t1.join();
    t0.join();

    d2::disable_event_logging();
}
