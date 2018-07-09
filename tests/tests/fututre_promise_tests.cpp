#include "fututre_promise_tests.h"
#include "itc/experimental/future.hpp"

#include <chrono>
#include <iostream>

namespace future_promise_tests
{

void run_tests()
{
    auto thread = itc::run_thread();
    auto th_id = thread->get_id();

    for(int i = 0; i < 100; ++i)
    {
        itc::experimental::promise<int> prom;
        auto fut = prom.get_future();

        auto p = itc::capture(prom);
        itc::invoke(th_id, [p, i]() mutable {
            std::cout << "start working" << std::endl;
            itc::this_thread::sleep_for(std::chrono::milliseconds(20));
            std::cout << "setting promise value for " << i << std::endl;

            p.get().set_value(5);
        });
        std::cout << "waiting on future for" << i << std::endl;
        fut.wait_for(std::chrono::milliseconds(10));
        auto val0 = fut.get();
        (void)val0;
        std::cout << "future woke up for" << i << std::endl;
        std::cout << "TEST " << i << " completed" << std::endl;
    }

}

}
