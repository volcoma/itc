#include "fututre_promise_tests.h"
#include "itc/async.hpp"

#include <chrono>
#include <future>
#include <iostream>
namespace async_tests
{

void run_tests(int iterations)
{
	auto thread = itc::run_thread();
	auto th_id = thread->get_id();

	for(int i = 0; i < iterations; ++i)
	{

		auto fut = itc::async(th_id, [i]() mutable {
			std::cout << "start working" << std::endl;
			itc::this_thread::sleep_for(std::chrono::milliseconds(20));
			std::cout << "setting promise value for " << i << std::endl;
			if(rand() % 10 > 7)
			{
				throw std::runtime_error("testing thrown exception");
			}
			return 5;
		});
		std::cout << "waiting on future for" << i << std::endl;
		fut.wait_for(std::chrono::milliseconds(10));
		try
		{
			auto val0 = fut.get();
			(void)val0;
		}
		catch(const std::exception& e)
		{
			std::cout << e.what() << std::endl;
		}

		std::cout << "future woke up for" << i << std::endl;
		std::cout << "ASYNC TEST " << i << " completed" << std::endl;
	}
}
} // namespace async_tests