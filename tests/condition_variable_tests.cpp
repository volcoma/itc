#include "condition_variable_tests.h"
#include "utils.hpp"

#include <itc/condition_variable.hpp>
#include <itc/thread.h>
#include <chrono>

namespace cv_tests
{
using namespace std::chrono_literals;

void run_tests(int iterations)
{
	auto th1 = itc::make_thread();
	auto th2 = itc::make_thread();

	for(int i = 0; i < iterations; ++i)
	{
		// for the purpose of testing we will make it as shared_ptr
		auto cv = std::make_shared<itc::condition_variable>();

		itc::invoke(th1.get_id(), [cv, i]() {
			std::mutex m;
			std::unique_lock<std::mutex> lock(m);
			auto res = cv->wait_for(lock, 50ms);
			if(res == std::cv_status::no_timeout)
			{
				sout() << "th1 cv notified " << i;
			}
			else
			{
				sout() << "th1 cv timed out " << i;
			}
		});

		itc::invoke(th2.get_id(), [cv, i]() {
			std::mutex m;
			std::unique_lock<std::mutex> lock(m);
			auto res = cv->wait_for(lock, 100ms);
			if(res == std::cv_status::no_timeout)
			{
				sout() << "th2 cv notified " << i;
			}
			else
			{
				sout() << "th2 cv timed out " << i;
			}
		});
		std::this_thread::sleep_for(60ms);
		cv->notify_all();
	}
}
} // namespace cv_tests
