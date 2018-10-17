#include "thread_pool_tests.h"
#include "itc/thread_pool.h"

#include "utils.hpp"
#include <chrono>

namespace thread_pool_tests
{
using namespace std::chrono_literals;

void run_tests(int iterations)
{
	auto now = itc::clock::now();

	itc::thread_pool pool({{itc::priority::category::normal, 2},
						   {itc::priority::category::high, 1},
						   {itc::priority::category::critical, 1}});

	for(int i = 0; i < iterations; ++i)
	{
		for(size_t j = 0; j < 5; ++j)
		{
			// clang-format off
            auto job = pool.schedule(itc::priority::normal(j), [i, j]()
            {
                std::this_thread::sleep_for(10ms);
                sout() << "call normal priority job " << i << " variant : " << j;
            });
//            pool.change_priority(job.id, itc::priority::critical());

//            //job is just a normal shared_future and we can use it like any other
//            job.then(itc::this_thread::get_id(), [](auto parent)
//            {
//                sout() << "job is done\n";
//            });
//            pool.wait(job.id);
//            pool.stop(job.id);

			// clang-format on
		}

		for(size_t j = 0; j < 5; ++j)
		{
			// clang-format off
            pool.schedule(itc::priority::high(j), [i, j]()
            {
                std::this_thread::sleep_for(10ms);
                sout() << "call high priority job " << i << " variant : " << j;
            });
			// clang-format on
		}

		for(size_t j = 0; j < 5; ++j)
		{
			// clang-format off
            pool.schedule(itc::priority::critical(j), [i, j]()
            {
                std::this_thread::sleep_for(10ms);
                sout() << "call critical priority job " << i << " variant : " << j;
            });
			// clang-format on
		}
	}

	// pool.stop_all();
	pool.wait_all();

	auto end = itc::clock::now();
	auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - now);
	sout() << dur.count() << "ms\n";
}
} // namespace thread_pool_tests
