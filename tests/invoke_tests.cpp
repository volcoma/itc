#include "invoke_tests.h"
#include "utils.hpp"

#include <itc/thread.h>
#include <chrono>

namespace invoke_tests
{
std::thread make_std_thread()
{
	std::thread th([]() {
		itc::this_thread::register_this_thread();

		while(!itc::this_thread::notified_for_exit())
		{
			itc::this_thread::wait();
		}

		itc::this_thread::unregister_this_thread();
	});
	itc::register_thread(th.get_id());
	return th;
}
itc::thread::id make_detached_std_thread()
{
	std::thread th([]() {
		itc::this_thread::register_this_thread();

		while(!itc::this_thread::notified_for_exit())
		{
			itc::this_thread::wait();
		}

		itc::this_thread::unregister_this_thread();
	});
	auto id = itc::register_thread(th.get_id());
	th.detach();

	return id;
}

itc::thread::id create_detached_itc_thread()
{
	auto th = itc::make_thread();
	th.detach();
	return th.get_id();
}

void run_tests(int iterations)
{
	auto std_thread = make_std_thread();
	auto std_thread_mapped_id = itc::register_thread(std_thread.get_id());
	auto std_thread_detached_mapped_id = make_detached_std_thread();

	auto itc_thread = itc::make_thread();
	auto itc_thread_detached_id = create_detached_itc_thread();

	auto all_threads = itc::get_all_registered_threads();
	sout() << "registered threads = " << all_threads.size();

	int i = 0;
	while(!itc::this_thread::notified_for_exit())
	{
		if(i++ == iterations)
		{
			break;
		}

		sout() << "main_thread waiting ... " << i;
		itc::this_thread::wait();
		itc::this_thread::process();
		sout() << "main_thread woke up ... " << i;

		// clang-format off
		itc::invoke(std_thread_mapped_id,
        [](int arg)
        {
            sout() << "on std::thread " << arg;

            itc::invoke(itc::main_thread::get_id(),
            [](int arg)
            {
                sout() << "on main_thread from std::thread " << arg;
            }, arg);

        }, i);

        itc::invoke(std_thread_detached_mapped_id,
        [](int arg)
        {
            sout() << "on detached std::thread " << arg;

            itc::invoke(itc::main_thread::get_id(),
            [](int arg)
            {
                sout() << "on main_thread from detached std::thread " << arg;
            }, arg);

        }, i);
        itc::invoke(itc_thread.get_id(),
        [](int arg)
        {
            sout() << "on itc::thread " << arg;

            itc::invoke(itc::main_thread::get_id(),
            [](int arg)
            {
                sout() << "on main_thread from itc::thread " << arg;
            }, arg);

        }, i);
        itc::invoke(itc_thread_detached_id,
        [](int arg)
        {
            sout() << "on detached itc::thread " << arg;

            itc::invoke(itc::main_thread::get_id(),
            [](int arg)
            {
                sout() << "on main_thread from detached itc::thread " << arg;
            }, arg);

        }, i);
		// clang-format on
	}

	// for std::thread we have to do this
	itc::notify_for_exit(std_thread_mapped_id);
	if(std_thread.joinable())
	{
		std_thread.join();
	}

	// itc::thread will behave as a proper RAII type and do it in its destructor
}
} // namespace invoke_tests
