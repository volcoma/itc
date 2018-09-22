#include "itc/thread.h"

#include "async_tests.h"
#include "condition_variable_tests.h"
#include "fututre_promise_tests.h"
#include "invoke_tests.h"
#include "overhead_tests.h"
#include "thread_pool_tests.h"

#include "utils.hpp"
#include <iostream>
#include <mutex>
#include <string>

int main()
{
	itc::init_data data;
	data.log_error = [](const std::string& msg) { sout() << msg << "\n"; };
    data.log_info = [](const std::string& msg) { sout() << msg << "\n"; };
	itc::init(data);

    overhead_tests::run_tests(5);
    invoke_tests::run_tests(1000);
    cv_tests::run_tests(50);
    future_promise_tests::run_tests(50);
    async_tests::run_tests(50);
    thread_pool_tests::run_tests(50);

	itc::shutdown();
	return 0;
}
