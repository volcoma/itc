#include "itc/experimental/condition_variable.hpp"
#include "itc/experimental/future.hpp"

#include "itc/future.hpp"
#include "itc/thread.h"
#include "itc/utility.hpp"
#include "lib1/lib1.h"
#include "lib2/lib2.h"

#include "async_tests.h"
#include "condition_variable_tests.h"
#include "fututre_promise_tests.h"
#include "invoke_tests.h"

#include <iostream>
#include <mutex>
#include <string>

int main()
{
	itc::utility_callbacks callbacks;
	callbacks.log_error = [](const std::string& msg) { std::cout << msg << std::endl; };
	callbacks.log_info = [](const std::string& msg) { std::cout << msg << std::endl; };

	itc::init(callbacks);
	itc::this_thread::register_and_link();

	{

		future_promise_tests::run_tests(50);
		async_tests::run_tests(50);
		cv_tests::run_tests(50);
		invoke_tests::run_tests(1000);
	}

	itc::this_thread::unregister_and_unlink();
	itc::shutdown();

	return 0;
}
