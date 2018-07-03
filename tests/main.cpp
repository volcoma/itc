#include "itc/itc.h"
#include <iostream>
#include <mutex>

using namespace std::chrono_literals;

int main()
{
	itc::this_thread::register_and_link();

	auto th0 = std::this_thread::get_id();

	itc::thread thread([th0]() {
		itc::this_thread::register_and_link();

		while(itc::this_thread::is_running())
		{
			itc::notify(th0);

			std::cout << "th1 waiting ..." << std::endl;

			itc::this_thread::sleep_for(1s);

			std::cout << "th1 woke up ..." << std::endl;
		}

		std::cout << "th1 exitting ..." << std::endl;
		itc::this_thread::unregister_and_unlink();
	});

	auto th1 = thread.get_id();
	itc::register_thread(th1);

	int i = 0;
	while(itc::this_thread::is_running())
	{
		if(i++ == 12)
		{
			itc::notify_for_exit(th0);
			itc::notify_for_exit(th1);
		}
		itc::notify(th1);

		std::cout << "th0 waiting ..." << std::endl;

		itc::this_thread::wait_for_event();

		std::cout << "th0 woke up ..." << std::endl;
	}

	thread.join();
	itc::this_thread::unregister_and_unlink();

	return 0;
}
