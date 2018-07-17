#include "fututre_promise_tests.h"
#include "itc/when_all_any.hpp"

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

		// std::vector<itc::future<int>> futs;
		// futs.emplace_back(std::move(fut0));
		// futs.emplace_back(std::move(fut1));
		// itc::when_all(std::begin(futs), std::end(futs)).wait();
		auto this_id = itc::this_thread::get_id();
		auto chain_f = itc::when_all(fut0, fut1.share())
						   .then(th_id,
								 []() {
									 throw std::runtime_error("then1 ex");
									 std::cout << "then1" << std::endl;
								 })
						   .then(this_id, []() { std::cout << "then2" << std::endl; })
						   .then(this_id, []() { std::cout << "then3" << std::endl; });

		std::cout << "waiting on future for" << i << std::endl;
		chain_f.wait_for(std::chrono::milliseconds(10));
		try
		{
			chain_f.get();
			std::cout << "woke up on future for" << i << std::endl;
		}
		catch(const std::exception& e)
		{
			std::cout << e.what() << std::endl;
		}

		std::cout << "future woke up for" << i << std::endl;
		std::cout << "ASYNC TEST " << i << " completed" << std::endl;
	}
}
} // namespace async_tests
