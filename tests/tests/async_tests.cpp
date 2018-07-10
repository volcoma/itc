#include "fututre_promise_tests.h"
#include "itc/experimental/async.hpp"

#include <chrono>
#include <iostream>

namespace async_tests
{

void run_tests(int iterations)
{
	auto thread = itc::run_thread();
	auto th_id = thread->get_id();

	for(int i = 0; i < iterations; ++i)
	{

		auto fut = itc::experimental::async(th_id, [i]() mutable {
			std::cout << "start working" << std::endl;
			itc::this_thread::sleep_for(std::chrono::milliseconds(20));
			std::cout << "setting promise value for " << i << std::endl;

			return 5;
		});
		std::cout << "waiting on future for" << i << std::endl;
		fut.wait_for(std::chrono::milliseconds(10));
		auto val0 = fut.get();
		(void)val0;
		std::cout << "future woke up for" << i << std::endl;
		std::cout << "ASYNC TEST " << i << " completed" << std::endl;
	}
}
}
