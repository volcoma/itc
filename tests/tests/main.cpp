#include "itc/experimental/condition_variable.hpp"
#include "itc/experimental/future.hpp"

#include "itc/future.hpp"
#include "itc/thread.h"
#include "itc/utility.hpp"
#include "lib1/lib1.h"
#include "lib2/lib2.h"
#include <iostream>
#include <mutex>
#include <string>

int main()
{
	itc::utility_callbacks callbacks;
	callbacks.logger = [](const std::string& msg) { std::cout << msg << std::endl; };
	itc::init(callbacks);
	itc::this_thread::register_and_link();

	{
		auto th1 = lib1::create_detached_thread();
		auto th1_sh = lib1::create_shared_thread();
		auto th11 = th1_sh->get_id();

		auto th2 = lib2::create_detached_thread();
		auto th2_sh = lib2::create_shared_thread();
		auto th22 = th2_sh->get_id();

		auto all_threads = itc::get_all_registered_threads();
		std::cout << "registered threads = " << all_threads.size() << std::endl;
		itc::promise<int> prom;
		auto fut = prom.get_future();

		auto p = itc::capture(prom);
		itc::invoke(th22, [p]() mutable {
			std::cout << "start working" << std::endl;
			itc::this_thread::sleep_for(std::chrono::seconds(2));
			std::cout << "setting promise value" << std::endl;

			p.get().set_value(5);
		});
		std::cout << "waiting on future" << std::endl;

		fut.wait_for(std::chrono::seconds(1));
		auto val0 = fut.get();
		(void)val0;
		std::cout << "future woke up" << std::endl;

		auto thcond1 = itc::run_thread();

		auto thcond2 = itc::run_thread();

		//		static itc::experimental::condition_variable cv;
		//		itc::invoke(thcond1->get_id(), []() {
		//			std::mutex m;
		//			std::unique_lock<std::mutex> lock(m);
		//			cv.wait(lock);
		//			std::cout << "cv notified" << std::endl;
		//		});

		//		itc::invoke(thcond2->get_id(), []() {
		//			std::mutex m;
		//			std::unique_lock<std::mutex> lock(m);
		//			cv.wait(lock);
		//			std::cout << "cv notified" << std::endl;
		//		});

		//		std::this_thread::sleep_for(std::chrono::seconds(2));
		//		cv.notify_one();

		int i = 0;
		while(!itc::this_thread::notified_for_exit())
		{
			if(i++ == 1000)
			{
				break;
			}
			itc::notify(th1);
			itc::notify(th11);
			itc::notify(th2);
			itc::notify(th22);

			std::cout << "th0 waiting ..." << std::endl;
			itc::this_thread::wait();
			std::cout << "th0 woke up ..." << std::endl;
		}
	}
	std::cout << "th0 exitting ..." << std::endl;
	itc::this_thread::unregister_and_unlink();

	itc::shutdown();

	return 0;
}
