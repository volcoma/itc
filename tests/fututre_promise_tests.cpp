#include "fututre_promise_tests.h"
#include "utils.hpp"

#include <itc/future.hpp>
#include <chrono>

namespace future_promise_tests
{
using namespace std::chrono_literals;
void run_tests(int iterations)
{
	auto thread = itc::make_thread();
	auto th_id = thread.get_id();

	for(int i = 0; i < iterations; ++i)
	{
		itc::promise<int> prom;
		auto fut = prom.get_future();

		itc::invoke(th_id, [p = std::move(prom), i]() mutable {
			sout() << "start working";
			itc::this_thread::sleep_for(20ms);
			sout() << "setting promise value for " << i;

			p.set_value(5);
		});
		sout() << "waiting on future for " << i;
		fut.wait_for(10ms);

		auto val0 = fut.get();

		sout() << "future woke up for " << i << " with value " << val0;
		sout() << "FUTURE TEST " << i << " completed";
	}

	auto thread2 = itc::make_thread();
	auto th_id2 = thread2.get_id();

	for(int i = 0; i < iterations; ++i)
	{
		itc::promise<int> prom;
		auto fut = prom.get_future().share();

		itc::invoke(th_id, [p = std::move(prom), i]() mutable {
			sout() << "th1 start working for " << i;
			itc::this_thread::sleep_for(20ms);
			sout() << "th1 setting promise value for " << i;

			p.set_value(12);
		});
		itc::invoke(th_id2, [fut, i]() mutable {
			sout() << "th2 waiting on shared_future for " << i;
			fut.wait_for(20ms);
			sout() << "th2 woke up on shared_future for " << i;
		});
		sout() << "th0 waiting on shared_future for " << i;
		fut.wait_for(10ms);

		auto val0 = fut.get();

		sout() << "th0 woke up on shared_future for " << i << " with value " << val0;
		sout() << "SHARED FUTURE TEST " << i << " completed";
	}
}
} // namespace future_promise_tests
