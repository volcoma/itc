#include "fututre_promise_tests.h"
#include "utils.hpp"

#include <threadpp/future.hpp>
#include <chrono>

namespace future_promise_tests
{
using namespace std::chrono_literals;
void run_tests(int iterations)
{
	auto thread = tpp::make_thread();
	auto th_id = thread.get_id();

	for(int i = 0; i < iterations; ++i)
	{
		tpp::promise<int> prom;
		auto fut = prom.get_future();

		tpp::invoke(th_id, [p = std::move(prom), i]() mutable {
			sout() << "start working";
			tpp::this_thread::sleep_for(20ms);
			sout() << "setting promise value for " << i;

			p.set_value(5);
		});
		sout() << "waiting on future for " << i;
		fut.wait_for(10ms);

		auto val0 = fut.get();

		sout() << "future woke up for " << i << " with value " << val0;
		sout() << "FUTURE TEST " << i << " completed";
	}

	auto thread2 = tpp::make_thread();
	auto th_id2 = thread2.get_id();

	for(int i = 0; i < iterations; ++i)
	{
		tpp::promise<int> prom;
		auto fut = prom.get_future().share();

		tpp::invoke(th_id, [p = std::move(prom), i]() mutable {
			sout() << "th1 start working for " << i;
			tpp::this_thread::sleep_for(20ms);
			sout() << "th1 setting promise value for " << i;

			p.set_value(12);
		});
		tpp::invoke(th_id2, [fut, i]() mutable {
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
