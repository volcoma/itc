#include "fututre_promise_tests.h"
#include "itc/future.hpp"

#include <chrono>
#include <iostream>

namespace future_promise_tests
{

void run_tests(int iterations)
{
	auto thread = itc::run_thread();
	auto th_id = thread->get_id();

	for(int i = 0; i < iterations; ++i)
	{
		itc::promise<int> prom;
		auto fut = prom.get_future();

		auto p = itc::capture(prom);
		itc::invoke(th_id, [p, i]() mutable {
			std::cout << "start working" << std::endl;
			itc::this_thread::sleep_for(std::chrono::milliseconds(20));
			std::cout << "setting promise value for " << i << std::endl;

			p.get().set_value(5);
		});
		std::cout << "waiting on future for " << i << std::endl;
		fut.wait_for(std::chrono::milliseconds(10));
		auto val0 = fut.get();
		(void)val0;
		std::cout << "future woke up for " << i << std::endl;
		std::cout << "FUTURE TEST " << i << " completed" << std::endl;
	}

	auto thread2 = itc::run_thread();
	auto th_id2 = thread2->get_id();

	for(int i = 0; i < iterations; ++i)
	{
		itc::promise<int> prom;
		auto fut = prom.get_future().share();

		auto p = itc::capture(prom);
		itc::invoke(th_id, [p, i]() mutable {
			std::cout << "th1 start working for " << i << std::endl;
			itc::this_thread::sleep_for(std::chrono::milliseconds(20));
			std::cout << "th1 setting promise value for " << i << std::endl;

			p.get().set_value(5);
		});
		itc::invoke(th_id2, [fut, i]() mutable {
			std::cout << "th2 waiting on shared_future for " << i << std::endl;
			fut.wait_for(std::chrono::milliseconds(20));
			std::cout << "th2 woke up on shared_future for " << i << std::endl;
		});
		std::cout << "th0 waiting on shared_future for " << i << std::endl;
		fut.wait_for(std::chrono::milliseconds(10));
		auto val0 = fut.get();
		(void)val0;
		std::cout << "th0 woke up on shared_future for " << i << std::endl;
		std::cout << "SHARED FUTURE TEST " << i << " completed" << std::endl;
	}
}
} // namespace future_promise_tests
