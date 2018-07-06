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
    callbacks.logger = [](const std::string& msg)
    {
        std::cout << msg << std::endl;
    };
	itc::init(callbacks);
	itc::this_thread::register_and_link();

	{
		auto th1 = lib1::create_detached_thread();
		auto th1_sh = lib1::create_shared_thread();
		auto th11 = th1_sh->get_id();

		auto th2 = lib2::create_detached_thread();
		auto th2_sh = lib2::create_shared_thread();
		auto th22 = th2_sh->get_id();

		struct informer
		{
			informer()
			{
				std::cout << "default ctor" << std::endl;
			}
			informer(informer&&)
			{
				std::cout << "move ctor" << std::endl;
			}
			informer(const informer&)
			{
				std::cout << "copy ctor" << std::endl;
			}
		};
		auto all_threads = itc::get_all_registered_threads();
		std::cout << "registered threads = " << all_threads.size() << std::endl;
		itc::promise<informer> prom;
		auto fut = prom.get_future();

        auto p = itc::capture(prom);
		itc::invoke(th22, [p]() mutable {
			std::cout << "start working" << std::endl;
			itc::this_thread::sleep_for(std::chrono::seconds(2));
			std::cout << "setting promise value" << std::endl;

			p.get().set_value({});
		});
		std::cout << "waiting on future" << std::endl;

		fut.wait();
		fut.wait();
		auto val0 = fut.get();
		auto val1 = std::move(fut.get());
		(void)val0;
		(void)val1;
		std::cout << "future woke up" << std::endl;

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
			itc::this_thread::wait_for_event();
			std::cout << "th0 woke up ..." << std::endl;
		}
	}
	std::cout << "th0 exitting ..." << std::endl;
	itc::this_thread::unregister_and_unlink();

	itc::shutdown();

	return 0;
}
