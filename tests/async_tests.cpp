#include "fututre_promise_tests.h"
#include "itc/when_all_any.hpp"

#include "utils.hpp"
#include <chrono>
#include <future>

namespace async_tests
{

void run_tests(int iterations)
{
	auto thread1 = itc::run_thread();
	auto thread2 = itc::run_thread();

	auto th1_id = thread1->get_id();
	auto th2_id = thread2->get_id();
	auto this_th_id = itc::this_thread::get_id();

	for(int i = 0; i < iterations; ++i)
	{

		// clang-format off

        // async + chaining continuations
        {
            // some move only object
            std::unique_ptr<int> up;

            auto future = itc::async(th1_id, [u = std::move(up)](int i) mutable
            {
                itc::this_thread::sleep_for(std::chrono::milliseconds(20));

                if(rand() % 10 > 7)
                {
                    throw std::runtime_error("propagated exception");
                }
                return i;
            }, i);

            auto shared_future = itc::async(th2_id, [u = std::move(up)](int i) mutable
            {
                itc::this_thread::sleep_for(std::chrono::milliseconds(20));

                if(rand() % 10 > 7)
                {
                    throw std::runtime_error("propagated exception");
                }
                return i;
            }, i).share();

            {
                auto chain = future.then(th1_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain then " << "\n";
                    return result;
                })
                .then(this_th_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain then " << "\n";
                    return result;
                })
                .then(th2_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain then " << "\n";
                    return result;
                });

                try
                {
                    sout() << "wait on chain" << "\n";
                    auto result = chain.get();
                    sout() << "woke up on chain with result " << result << "\n";
                }
                catch(const std::exception& e)
                {
                    sout() << e.what() << "\n";
                }
            }

            {
                //chain 0
                auto chain0 = shared_future.then(th1_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 0.1 then " << "\n";
                    return result;
                })
                .then(this_th_id, [](auto parent)
                {
                        auto result = parent.get();
                        sout() << "chain 0.2 then " << "\n";
                        return result;
                })
                .then(th2_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 0.3 then " << "\n";
                    return result;
                });

                //chain 1
                auto chain1 = shared_future.then(th1_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 1.1 then " << "\n";
                    return result;
                })
                .then(this_th_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 1.2 then " << "\n";
                    return result;
                })
                .then(th2_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 1.3 then " << "\n";
                    return result;
                });

                //chain 2
                auto chain2 = shared_future.then(th1_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 2.1 then " << "\n";
                    return result;
                })
                .then(this_th_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 2.2 then " << "\n";
                    return result;
                })
                .then(th2_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 2.3 then " << "\n";
                    return result;
                });

                auto chain3 = shared_future.then(th1_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 3.1 then " << "\n";
                    return result;
                })
                .then(this_th_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 3.2 then " << "\n";
                    return result;
                })
                .then(th2_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 3.3 then " << "\n";
                    return result;
                });


                // Use an applicative pattern. Attach a continuation depending
                // on multiple futures
                itc::when_all(chain0, chain1).then(this_th_id, [](auto parent)
                {
                    auto chains = parent.get();

                    auto chain0 = std::move(std::get<0>(chains));
                    auto chain1 = std::move(std::get<1>(chains));

                    sout() << "chain 0 result = " << chain0.get() << "\n";
                    sout() << "chain 1 result = " << chain1.get() << "\n";

                });

                // Wait for the rest of the chains
                try
                {
                    sout() << "wait on chain 2" << "\n";
                    auto result = chain2.get();
                    sout() << "woke up on chain 2 with result " << result << "\n";
                }
                catch(const std::exception& e)
                {
                    sout() << e.what() << "\n";
                }

                try
                {
                    sout() << "wait on chain 3" << "\n";
                    auto result = chain3.get();
                    sout() << "woke up on chain 3 with result " << result << "\n";
                }
                catch(const std::exception& e)
                {
                    sout() << e.what() << "\n";
                }
            }

        }

		// clang-format on

		sout() << "future woke up for" << i << "\n";
		sout() << "ASYNC TEST " << i << " completed"
			   << "\n";
	}
}
} // namespace async_tests
