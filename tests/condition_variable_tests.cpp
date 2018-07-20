#include "condition_variable_tests.h"
#include "itc/condition_variable.hpp"
#include "itc/thread.h"

#include "utils.hpp"
#include <chrono>

namespace cv_tests
{

void run_tests(int iterations)
{
	auto th1 = itc::run_thread();
	auto th2 = itc::run_thread();

	for(int i = 0; i < iterations; ++i)
	{
		// for the purpose of testing we will make it as shared_ptr
		auto cv = std::make_shared<itc::condition_variable>();

		itc::invoke(th1->get_id(), [cv, i]() {
			std::mutex m;
			std::unique_lock<std::mutex> lock(m);
			auto res = cv->wait_for(lock, std::chrono::milliseconds(50));
			if(res == std::cv_status::no_timeout)
			{
				sout() << "th1 cv notified " << i << "\n";
			}
			else
			{
				sout() << "th1 cv timed out " << i << "\n";
			}
		});

		itc::invoke(th2->get_id(), [cv, i]() {
			std::mutex m;
			std::unique_lock<std::mutex> lock(m);
			auto res = cv->wait_for(lock, std::chrono::milliseconds(100));
			if(res == std::cv_status::no_timeout)
			{
				sout() << "th2 cv notified " << i << "\n";
			}
			else
			{
				sout() << "th2 cv timed out " << i << "\n";
			}
		});
		std::this_thread::sleep_for(std::chrono::milliseconds(60));
		cv->notify_all();
	}
}
} // namespace cv_tests
