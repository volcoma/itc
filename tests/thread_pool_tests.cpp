#include "thread_pool_tests.h"
#include "utils.hpp"

#include <threadpp/thread_pool.h>
#include <chrono>

namespace thread_pool_tests
{
using namespace std::chrono_literals;

void run_tests(int iterations)
{
	auto now = tpp::clock::now();

	tpp::thread_pool pool({{tpp::priority::category::normal, 2},
						   {tpp::priority::category::high, 1},
						   {tpp::priority::category::critical, 1}});

	for(int i = 0; i < iterations; ++i)
	{
		for(size_t j = 0; j < 5; ++j)
		{
			// clang-format off
            auto job = pool.schedule(tpp::priority::normal(j), [i, j]()
            {
                std::this_thread::sleep_for(10ms);
                sout() << "call normal priority job " << i << " variant : " << j;
            });
//            pool.change_priority(job.id, tpp::priority::critical());

//            //job is just a normal shared_future and we can use it like any other
//            job.then(tpp::this_thread::get_id(), [](auto parent)
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
            pool.schedule(tpp::priority::high(j), [i, j]()
            {
                std::this_thread::sleep_for(10ms);
                sout() << "call high priority job " << i << " variant : " << j;
            });
			// clang-format on
		}

		for(size_t j = 0; j < 5; ++j)
		{
			// clang-format off
            pool.schedule(tpp::priority::critical(j), [i, j]()
            {
                std::this_thread::sleep_for(10ms);
                sout() << "call critical priority job " << i << " variant : " << j;
            });
			// clang-format on
		}
	}

	// pool.stop_all();
	pool.wait_all();

	auto end = tpp::clock::now();
	auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - now);
	sout() << dur.count() << "ms\n";
}
} // namespace thread_pool_tests
