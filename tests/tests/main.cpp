#include "itc/itc.h"
#include "lib1/lib1.h"
#include "lib2/lib2.h"
#include <iostream>
#include <mutex>

using namespace std::chrono_literals;

int main()
{
	itc::this_thread::register_and_link();

	auto th0 = std::this_thread::get_id();

	auto th1 = test_lib1();
	itc::register_thread(th1);

	auto th2 = test_lib2();
	itc::register_thread(th2);

	int i = 0;
	while(itc::this_thread::is_running())
	{
		if(i++ == 1000)
		{
			itc::notify_for_exit(th0);
			itc::notify_for_exit(th1);
			itc::notify_for_exit(th2);
		}
		itc::notify(th1);
		itc::notify(th2);
		std::cout << "th0 waiting ..." << std::endl;

		itc::this_thread::wait_for_event();

		std::cout << "th0 woke up ..." << std::endl;
	}

	std::cout << "th0 exitting ..." << std::endl;
	itc::this_thread::unregister_and_unlink();

	std::cout << "itc waiting for cleanup ..." << std::endl;
	itc::wait_for_cleanup();
	return 0;
}
