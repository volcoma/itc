#include "fututre_promise_tests.h"
#include "itc/async.hpp"

#include <chrono>
#include <future>
#include <iostream>
namespace async_tests
{

void run_tests(int iterations)
{
	auto thread = itc::run_thread();
	auto th_id = thread->get_id();

	for(int i = 0; i < iterations; ++i)
	{

		std::unique_ptr<int> up;
		auto fut0 = itc::async(th_id, [i, u = std::move(up)]() mutable {
			std::cout << "start working" << std::endl;
			itc::this_thread::sleep_for(std::chrono::milliseconds(20));
			std::cout << "setting promise value for " << i << std::endl;
			if(rand() % 10 > 7)
			{
				throw std::runtime_error("testing thrown exception");
			}
			return 5;
		});
        auto fut1 = itc::async(th_id, [i, u = std::move(up)]() mutable {
			std::cout << "start working" << std::endl;
			itc::this_thread::sleep_for(std::chrono::milliseconds(20));
			std::cout << "setting promise value for " << i << std::endl;
			if(rand() % 10 > 7)
			{
				throw std::runtime_error("testing thrown exception");
			}
			return 5;
		});

        //std::vector<itc::future<int>> futs;
        //futs.emplace_back(std::move(fut0));
        //futs.emplace_back(std::move(fut1));
        //itc::when_all(std::begin(futs), std::end(futs)).wait();
        itc::then(itc::when_all(fut0, fut1.share()), th_id, []()
        {
            std::cout << "then" << std::endl;

        }).wait();

		//auto fut2 = itc::then(fut0, th_id, []() { return 5; });

		std::cout << "waiting on future for" << i << std::endl;
//		fut2.wait_for(std::chrono::milliseconds(10));
//		try
//		{
//			auto val0 = fut2.get();
//			(void)val0;
//		}
//		catch(const std::future_error& e)
//		{
//			std::cout << e.what() << std::endl;
//		}

		std::cout << "future woke up for" << i << std::endl;
		std::cout << "ASYNC TEST " << i << " completed" << std::endl;
	}
}
} // namespace async_tests
