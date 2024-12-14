#include "when_tests.h"
#include "utils.hpp"

#include <chrono>
#include <threadpp/thread_pool.h>
#include <threadpp/when_all_any.hpp>

namespace when_tests
{
using namespace std::chrono_literals;

void run_tests(int iterations)
{
	auto thread1 = tpp::make_thread();
	auto thread2 = tpp::make_thread();

	auto th1_id = thread1.get_id();
	auto th2_id = thread2.get_id();
	auto this_th_id = tpp::this_thread::get_id();

	// clang-format off
	for(int i = 0; i < iterations; ++i)
	{

//		{
//			auto f0 = tpp::async(th1_id, []() { return 0; });
//			auto f1 = tpp::async(th2_id, []() { return 1; }).share();

//			// Use an applicative pattern. Attach a continuation depending
//			// on multiple futures

//			// Pass by reference
//			auto when = tpp::when_all(f0, f1)
//            .then(this_th_id, [](auto parent)
//            {
//				auto futures = parent.get();

//				auto f0 = std::move(std::get<0>(futures));
//				auto f1 = std::move(std::get<1>(futures));

//				sout() << "future 0 result = " << f0.get();
//				sout() << "future 1 result = " << f1.get();

//			});

//			// waiting for test purposes
//			when.wait();
//		}

//		{
//			auto f0 = tpp::async(th1_id, []() { return 0; });
//			auto f1 = tpp::async(th2_id, []() { return 1; }).share();

//			// Pass by rvalue
//			auto when = tpp::when_all(std::move(f0), std::move(f1))
//            .then(this_th_id, [](auto parent)
//            {
//				auto futures = parent.get();

//				auto f0 = std::move(std::get<0>(futures));
//				auto f1 = std::move(std::get<1>(futures));

//				sout() << "future 0 result = " << f0.get();
//				sout() << "future 1 result = " << f1.get();

//			});

//			// waiting for test purposes
//			when.wait();
//		}
//		{

//			auto f0 = tpp::async(th1_id, []() { return 0; });
//			auto f1 = tpp::async(th2_id, []() { return 1; });

//			tpp::future<int> fs[] = {std::move(f0), std::move(f1)};
//			// std::vector<tpp::future<int>> fs;
//			// fs.reserve(2);
//			// fs.emplace_back(std::move(f0));
//			// fs.emplace_back(std::move(f1));
//			// std::array<tpp::future<int>, 2> fs = { std::move(f0), std::move(f1) };

//			auto when = tpp::when_all(std::begin(fs), std::end(fs))
//            .then(this_th_id, [](auto parent)
//            {
//				auto futures = parent.get();

//				auto f0 = std::move(futures[0]);
//				auto f1 = std::move(futures[1]);

//				sout() << "future 0 result = " << f0.get();
//				sout() << "future 1 result = " << f1.get();

//			});

//			// waiting for test purposes
//			when.wait();
//		}
//		{

//			auto f0 = tpp::async(th1_id, []() { return 0; }).share();
//			auto f1 = tpp::async(th2_id, []() { return 1; }).share();

//			tpp::shared_future<int> fs[] = {f0, f1};
//			// std::vector<itcshared_futurefuture<int>> fs;
//			// fs.reserve(2);
//			// fs.emplace_back(f0);
//			// fs.emplace_back(f1);
//			// std::array<tpp::shared_future<int>, 2> fs = { f0, f1 };

//			auto when = tpp::when_all(std::begin(fs), std::end(fs))
//            .then(this_th_id, [](auto parent)
//            {
//				auto futures = parent.get();

//				auto f0 = std::move(futures[0]);
//				auto f1 = std::move(futures[1]);

//				sout() << "future 0 result = " << f0.get();
//				sout() << "future 1 result = " << f1.get();

//			});

//			// waiting for test purposes
//			when.wait();
//		}

//		{
//			auto f0 = tpp::async(th1_id, []() { throw 12; return 0; });
//			auto f1 = tpp::async(th2_id, []() { return 1; }).share();

//			// Pass by reference
//			auto when = tpp::when_any(f0, f1)
//            .then(this_th_id, [](auto parent)
//            {
//				auto result = parent.get();
//				auto index = result.index;

//				// facility to access tuple element by runtime index
//				tpp::visit_at(result.futures, index, [index](auto& chain) {
//					try
//					{
//						auto result = chain.get();
//						sout() << "woke up on future " << index << " with result " << result;
//					}
//					catch(const std::exception& e)
//					{
//						sout() << e.what() << " from " << index;
//					}
//				});
//			});

//			// waiting for test purposes
//			when.wait();
//		}
//		{
//			auto f0 = tpp::async(th1_id, []() { return 0; });
//			auto f1 = tpp::async(th2_id, []() { return 1; }).share();

//			// Pass by rvalue
//			auto when = tpp::when_any(std::move(f0), std::move(f1))
//            .then(this_th_id, [](auto parent)
//            {
//				auto result = parent.get();
//				auto index = result.index;

//				// facility to access tuple element by runtime index
//				tpp::visit_at(result.futures, index, [index](auto& chain) {
//					try
//					{
//						auto result = chain.get();
//						sout() << "woke up on future " << index << " with result " << result;
//					}
//					catch(const std::exception& e)
//					{
//						sout() << e.what() << " from " << index;
//					}
//				});
//			});

//			// waiting for test purposes
//			when.wait();
//		}
//		{
//			auto f0 = tpp::async(th1_id, []() { return 0; });
//			auto f1 = tpp::async(th2_id, []() { return 1; });

//			tpp::future<int> fs[] = {std::move(f0), std::move(f1)};
//			// std::vector<tpp::future<int>> fs;
//			// fs.reserve(2);
//			// fs.emplace_back(std::move(f0));
//			// fs.emplace_back(std::move(f1));
//			// std::array<tpp::future<int>, 2> fs = { std::move(f0), std::move(f1) };

//			auto when = tpp::when_any(std::begin(fs), std::end(fs))
//            .then(this_th_id, [](auto parent)
//            {
//				auto result = parent.get();
//				auto f = std::move(result.futures[result.index]);

//				sout() << "woke up on future " << result.index << " with result " << f.get();
//			});

//			// waiting for test purposes
//			when.wait();
//		}
//		{
//			auto f0 = tpp::async(th1_id, []() { return 0; }).share();
//			auto f1 = tpp::async(th2_id, []() { return 1; }).share();

//			tpp::shared_future<int> fs[] = {f0, f1};
//			// std::vector<itcshared_futurefuture<int>> fs;
//			// fs.reserve(2);
//			// fs.emplace_back(f0);
//			// fs.emplace_back(f1);
//			// std::array<tpp::shared_future<int>, 2> fs = { f0, f1 };

//			auto when = tpp::when_any(std::begin(fs), std::end(fs))
//            .then(this_th_id, [](auto parent)
//            {
//				auto result = parent.get();
//				auto f = std::move(result.futures[result.index]);

//				sout() << "woke up on future " << result.index << " with result " << f.get();
//			});
//			// waiting for test purposes
//			when.wait();
//		}
//        //empty test
//        {
//            auto when = tpp::when_all();
//            when.get();

//        }
//        {
//            auto when = tpp::when_any();
//            auto res = when.get();

//            if(res.index != size_t(-1))
//            {
//                throw std::runtime_error("wrong");
//            }
//        }
//        {
//            std::vector<tpp::future<void>> fs{};
//            auto when = tpp::when_any(std::begin(fs), std::end(fs));
//            auto res = when.get();

//            if(res.index != size_t(-1))
//            {
//                throw std::runtime_error("wrong");
//            }
//        }
//        {
//            tpp::thread_pool pool({{tpp::priority::category::normal, 2},
//                                   {tpp::priority::category::high, 1},
//                                   {tpp::priority::category::critical, 1}});

//            auto f0 = pool.schedule(tpp::priority::normal(), []()
//            {
//                return 0;
//            });

//            auto f1 = pool.schedule(tpp::priority::normal(), []()
//            {
//                return 1;
//            });

//            // Pass by reference
//			auto when = tpp::when_all(f0, f1)
//            .then(this_th_id, [](auto parent)
//            {
//				auto futures = parent.get();

//				auto f0 = std::move(std::get<0>(futures));
//				auto f1 = std::move(std::get<1>(futures));

//				sout() << "future 0 result = " << f0.get();
//				sout() << "future 1 result = " << f1.get();

//			});

//			// waiting for test purposes
//			when.wait();
//        }


        {
            auto chain = tpp::async(th1_id, []() -> int
            {
                throw std::runtime_error("propagate");
            })
            .then(this_th_id, [](auto parent)
            {
                auto result = parent.get();
                sout() << "chain then " << result;
                return result;
            })
            .then(th2_id, [](auto parent)
            {
                auto result = parent.get();
                sout() << "chain then " << result;
                return result;
            });
            auto when = tpp::when_all(chain);
            try
            {
                sout() << "wait on chain\n";
                auto result = when.get();
                std::get<0>(result).get();
            }
            catch(const std::exception& e)
            {
                sout() << e.what();
            }
        }


		sout() << "WHEN TEST " << i << " completed\n";
	}
	// clang-format on
}
} // namespace async_tests
