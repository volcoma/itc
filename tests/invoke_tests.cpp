#include "invoke_tests.h"
#include "itc/thread.h"

#include "utils.hpp"
#include <chrono>

namespace invoke_tests
{

itc::thread::id create_detached_thread()
{
	itc::thread th([]() {
		itc::this_thread::register_this_thread();

		auto id = itc::this_thread::get_id();

		while(!itc::this_thread::notified_for_exit())
		{
			itc::notify(itc::main_id());

			sout() << "th" << id << " detached thread waiting ..."
				   << "\n";

			itc::this_thread::wait();

			sout() << "th" << id << " detached thread woke up ..."
				   << "\n";
		}

		sout() << "th" << id << " thread exitting ..."
			   << "\n";
		itc::this_thread::unregister_this_thread();
	});
	th.detach();
	return th.get_id();
}

itc::thread create_thread()
{
	auto th = itc::make_thread();
	itc::invoke(th.get_id(), []() {

		auto id = itc::this_thread::get_id();

		while(!itc::this_thread::notified_for_exit())
		{
			itc::notify(itc::main_id());

			sout() << "th" << id << " shared thread waiting ..."
				   << "\n";

			itc::this_thread::wait();

			sout() << "th" << id << " shared thread woke up ..."
				   << "\n";
		}
	});

	return th;
}

void run_tests(int iterations)
{
	auto th1 = create_detached_thread();
	auto th1_sh = create_thread();
	auto th11 = th1_sh.get_id();

	auto th2 = create_detached_thread();
	auto th2_sh = create_thread();
	auto th22 = th2_sh.get_id();

	auto all_threads = itc::get_all_registered_threads();
	sout() << "registered threads = " << all_threads.size() << "\n";

	int i = 0;
	while(!itc::this_thread::notified_for_exit())
	{
		if(i++ == iterations)
		{
			break;
		}

		sout() << "th0 waiting ... " << i << "\n";
		itc::this_thread::wait();
		itc::this_thread::process();
		sout() << "th0 woke up ... " << i << "\n";

		itc::invoke(th1, [](int arg) { sout() << arg << "\n"; }, i);

		// notify just invokes an empty lambda there
		itc::notify(th11); // identical to itc::invoke(th11, [](){});
		itc::notify(th2);
		itc::notify(th22);
	}
}
} // namespace invoke_tests
