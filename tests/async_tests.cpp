#include "fututre_promise_tests.h"
#include "itc/when_all_any.hpp"

#include "utils.hpp"
#include <chrono>
#include <future>

namespace async_tests
{
using namespace std::chrono_literals;

void run_tests(int iterations)
{
	auto thread1 = itc::make_thread();
	auto thread2 = itc::make_thread();

	auto th1_id = thread1.get_id();
	auto th2_id = thread2.get_id();
	auto this_th_id = itc::this_thread::get_id();

	for(int i = 0; i < iterations; ++i)
	{

		// clang-format off

        // async + chaining continuations
        {
            // some move only object
            // can pass it by move either to the capture list or as a parameter to async
            std::unique_ptr<int> up;
            auto future = itc::async(th1_id, [u = std::move(up)](int i)
            {
                itc::this_thread::sleep_for(20ms);

                if(i % 10 == 0)
                {
                    throw std::runtime_error("propagated exception");
                }
                return i;
            }, i);

            auto shared_future = itc::async(th2_id, [u = std::move(up)](int i)
            {
                itc::this_thread::sleep_for(20ms);

                if(i % 10 == 0)
                {
                    throw std::runtime_error("propagated exception");
                }
                return i;
            }, i).share();

            {
                auto chain = future.then(th1_id, [u = std::move(up)](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain then " << result << "\n";
                    return result;
                })
                .then(this_th_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain then " << result << "\n";
                    return result;
                })
                .then(th2_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain then " << result << "\n";
                    return result;
                });

                try
                {
                    sout() << "wait on chain\n";
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
                    sout() << "chain 0.1 then " << result << "\n";
                    return result;
                })
                .then(this_th_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 0.2 then " << result << "\n";
                    return result;
                })
                .then(th2_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 0.3 then " << result << "\n";
                    return result;
                });

                //chain 1
                auto chain1 = shared_future.then(th1_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 1.1 then " << result << "\n";
                    return result;
                })
                .then(this_th_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 1.2 then " << result << "\n";
                    return result;
                })
                .then(th2_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 1.3 then " << result << "\n";
                    return result;
                });

                //chain 2
                auto chain2 = shared_future.then(th1_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 2.1 then " << result << "\n";
                    return result;
                })
                .then(this_th_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 2.2 then " << result << "\n";
                    return result;
                })
                .then(th2_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 2.3 then " << result << "\n";
                    return result;
                });

                auto chain3 = shared_future.then(th1_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 3.1 then " << result << "\n";
                    return result;
                })
                .then(this_th_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 3.2 then " << result << "\n";
                    return result;
                })
                .then(th2_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 3.3 then " << result << "\n";
                    return result;
                });

                auto chain4 = shared_future.then(th1_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 4.1 then " << result << "\n";
                    return result;
                })
                .then(this_th_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 4.2 then " << result << "\n";
                    return result;
                })
                .then(th2_id, [](auto parent)
                {
                    auto result = parent.get();
                    sout() << "chain 4.3 then " << result << "\n";
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

//                itc::future<int> futures[] = { std::move(chain0), std::move(chain1) };
//                itc::when_all(std::begin(futures), std::end(futures)).then(this_th_id, [](auto parent)
//                {
//                    auto chains = parent.get();

//                    auto chain0 = std::move(chains[0]);
//                    auto chain1 = std::move(chains[1]);

//                    sout() << "chain 0 result = " << chain0.get() << "\n";
//                    sout() << "chain 1 result = " << chain1.get() << "\n";

//                });

                itc::when_any(chain2, chain3).then(this_th_id, [](auto parent)
                {
                    auto result = parent.get();
                    auto chains = std::move(result.futures);
                    auto index = result.index;

                    // facility to access tuple element by runtime index
                    itc::visit_at(chains, index, [index](auto& chain)
                    {
                        try
                        {
                            auto result = chain.get();
                            sout() << "woke up on chain " << index << " with result " << result << "\n";
                        }
                        catch(const std::exception& e)
                        {
                            sout() << e.what() << " from " << index << "\n";
                        }
                    });
                });

                // Wait for the rest of the chains
                try
                {
                    sout() << "wait on chain 4\n";
                    auto result = chain4.get();
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
		sout() << "ASYNC TEST " << i << " completed\n";
	}
}
} // namespace async_tests
