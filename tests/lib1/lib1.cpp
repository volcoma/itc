#include "lib1.h"
#include "itc/itc.h"
#include <iostream>
using namespace std::chrono_literals;

std::thread::id test_lib1()
{
    itc::thread thread([]() {
		itc::this_thread::register_and_link();

		while(itc::this_thread::is_running())
		{
			itc::notify(itc::get_main_id());

			std::cout << "lib1 thread waiting ..." << std::endl;

			itc::this_thread::wait_for_event();

			std::cout << "lib1 thread woke up ..." << std::endl;
		}

		std::cout << "lib1 thread exitting ..." << std::endl;
		itc::this_thread::unregister_and_unlink();
	});
    auto id = thread.get_id();
    thread.detach();
    return id;
}
