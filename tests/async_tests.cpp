#include "async_tests.h"
#include "utils.hpp"

#include <chrono>
#include <itc/future.hpp>

namespace async_tests
{
using namespace std::chrono_literals;

void run_tests(int iterations)
{
	auto thread1 = itc::make_thread();
	auto thread2 = itc::make_thread();

	auto th1_id = thread1.get_id();
	auto th2_id = thread2.get_id();
	auto this_th_id = itc::this_thread::get_id();

	for(int i = 0; i < iterations; ++i)
	{

		// clang-format off

        // async + chaining continuations
        {
            // some move only object
            // can pass it by move either to the capture list or as a parameter to async
            auto up = std::make_unique<int>(5);
            auto future = itc::async(th1_id, [u = std::move(up)](int i)
            {
                itc::this_thread::sleep_for(20ms);

                if(i % 10 == 0)
                {
                    throw std::runtime_error("propagated exception");
                }
                return i;
            }, i);

            auto shared_future = itc::async(th2_id, [u = std::move(up)](int i)
            {
                itc::this_thread::sleep_for(20ms);

                if(i % 10 == 0)
                {
                    throw std::runtime_error("propagated exception");
                }
                return i;
            }, i).share();

            {
                auto chain = future.then(th1_id, [u = std::move(up)](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain then " << result;
                    return result;
                })
                .then(this_th_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain then " << result;
                    return result;
                })
                .then(th2_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain then " << result;
                    return result;
                });

                try
                {
                    sout() << "wait on chain\n";
                    auto result = chain.get();
                    sout() << "woke up on chain with result " << result;
                }
                catch(const std::exception& e)
                {
                    sout() << e.what();
                }
            }
        }

		// clang-format on

		sout() << "future woke up for" << i;
		sout() << "ASYNC TEST " << i << " completed\n";
	}
}
} // namespace async_tests
