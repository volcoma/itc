#include "invoke_tests.h"
#include "utils.hpp"

#include <threadpp/thread.h>
#include <chrono>

namespace invoke_tests
{
std::thread make_std_thread()
{
	std::thread th([]() {
		tpp::this_thread::register_this_thread();

		while(!tpp::this_thread::notified_for_exit())
		{
			tpp::this_thread::wait();
		}

		tpp::this_thread::unregister_this_thread();
	});
	tpp::register_thread(th.get_id());
	return th;
}
tpp::thread::id make_detached_std_thread()
{
	std::thread th([]() {
		tpp::this_thread::register_this_thread();

		while(!tpp::this_thread::notified_for_exit())
		{
			tpp::this_thread::wait();
		}

		tpp::this_thread::unregister_this_thread();
	});
	auto id = tpp::register_thread(th.get_id());
	th.detach();

	return id;
}

tpp::thread::id create_detached_itc_thread()
{
	auto th = tpp::make_thread();
	th.detach();
	return th.get_id();
}

void run_tests(int iterations)
{
	auto std_thread = make_std_thread();
	auto std_thread_mapped_id = tpp::register_thread(std_thread.get_id());
	auto std_thread_detached_mapped_id = make_detached_std_thread();

	auto itc_thread = tpp::make_thread();
	auto itc_thread_detached_id = create_detached_itc_thread();

	auto all_threads = tpp::get_all_registered_threads();
	sout() << "registered threads = " << all_threads.size();

	int i = 0;
	while(!tpp::this_thread::notified_for_exit())
	{
		if(i++ == iterations)
		{
			break;
		}

		sout() << "main_thread waiting ... " << i;
		tpp::this_thread::process();
		sout() << "main_thread woke up ... " << i;

        tpp::invoke(std_thread_detached_mapped_id,
        [](int arg)
        {
            sout() << "on detached std::thread " << arg;

            tpp::invoke(tpp::main_thread::get_id(),
            [](int arg)
            {
                sout() << "on main_thread from detached std::thread " << arg;
            }, arg);

        }, i);
        tpp::invoke(itc_thread.get_id(),
        [](int arg)
        {
            sout() << "on tpp::thread " << arg;

            tpp::invoke(tpp::main_thread::get_id(),
            [](int arg)
            {
                sout() << "on main_thread from tpp::thread " << arg;
            }, arg);

        }, i);
        tpp::invoke(itc_thread_detached_id,
        [](int arg)
        {
            sout() << "on detached tpp::thread " << arg;

            tpp::invoke(tpp::main_thread::get_id(),
            [](int arg)
            {
                sout() << "on main_thread from detached tpp::thread " << arg;
            }, arg);

        }, i);
		// clang-format on
	}

	// for std::thread we have to do this
	tpp::notify_for_exit(std_thread_mapped_id);
	if(std_thread.joinable())
	{
		std_thread.join();
	}

	// tpp::thread will behave as a proper RAII type and do it in its destructor
}
} // namespace invoke_tests
