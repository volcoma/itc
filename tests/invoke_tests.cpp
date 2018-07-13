#include "invoke_tests.h"
#include "itc/thread.h"

#include <chrono>
#include <iostream>
namespace invoke_tests
{

itc::thread::id create_detached_thread(int id)
{
	itc::thread th([id]() {
		itc::this_thread::register_and_link();

		while(!itc::this_thread::notified_for_exit())
		{
			itc::notify(itc::get_main_id());

			std::cout << "th" << id << " detached thread waiting ..." << std::endl;

			itc::this_thread::wait();

			std::cout << "th" << id << " detached thread woke up ..." << std::endl;
		}

		std::cout << "th" << id << "thread exitting ..." << std::endl;
		itc::this_thread::unregister_and_unlink();
	});
	th.detach();
	return th.get_id();
}

itc::shared_thread create_shared_thread(int id)
{
	auto th = itc::run_thread();
	itc::invoke(th->get_id(), [id]() {
		while(!itc::this_thread::notified_for_exit())
		{
			itc::notify(itc::get_main_id());

			std::cout << "th" << id << " shared thread waiting ..." << std::endl;

			itc::this_thread::wait();

			std::cout << "th" << id << " shared thread woke up ..." << std::endl;
		}
	});

	return th;
}

void run_tests(int iterations)
{
	auto th1 = create_detached_thread(1);
	auto th1_sh = create_shared_thread(2);
	auto th11 = th1_sh->get_id();

	auto th2 = create_detached_thread(3);
	auto th2_sh = create_shared_thread(4);
	auto th22 = th2_sh->get_id();

	auto all_threads = itc::get_all_registered_threads();
	std::cout << "registered threads = " << all_threads.size() << std::endl;

	int i = 0;
	while(!itc::this_thread::notified_for_exit())
	{
		if(i++ == iterations)
		{
			break;
		}

		std::cout << "th0 waiting ... " << i << std::endl;
		itc::this_thread::wait();
		itc::this_thread::process();
		std::cout << "th0 woke up ... " << i << std::endl;

		itc::notify(th1);
		itc::notify(th11);
		itc::notify(th2);
		itc::notify(th22);
	}
}
} // namespace invoke_tests
