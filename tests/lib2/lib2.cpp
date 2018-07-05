#include "itc/itc.h"

#include "lib2.h"
#include <iostream>

namespace lib2
{

std::thread::id create_detached_thread()
{
	itc::thread th([]() {
		itc::this_thread::register_and_link();

		while(!itc::this_thread::notified_for_exit())
		{
			itc::notify(itc::get_main_id());

			std::cout << "lib2 detached thread waiting ..." << std::endl;

			itc::this_thread::wait_for_event();

			std::cout << "lib2 detached thread woke up ..." << std::endl;
		}

		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::cout << "lib2 thread exitting ..." << std::endl;
		itc::this_thread::unregister_and_unlink();
	});
	auto id = th.get_id();
    itc::register_thread(id);
	th.detach();
	return id;
}

itc::shared_thread create_shared_thread()
{
    auto th = itc::run_thread("lib2");
    itc::invoke(th->get_id(), []()
    {
        while(!itc::this_thread::notified_for_exit())
        {
            itc::notify(itc::get_main_id());

			std::cout << "lib2 shared thread waiting ..." << std::endl;

			itc::this_thread::wait_for_event();

			std::cout << "lib2 shared thread woke up ..." << std::endl;
        }
    });

    return th;
}
}
