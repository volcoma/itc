#include "fututre_promise_tests.h"
#include "itc/thread.h"

#include "lib1/lib1.h"
#include "lib2/lib2.h"

#include <chrono>
#include <iostream>
namespace invoke_tests
{

void run_tests()
{
    auto th1 = lib1::create_detached_thread();
    auto th1_sh = lib1::create_shared_thread();
    auto th11 = th1_sh->get_id();

    auto th2 = lib2::create_detached_thread();
    auto th2_sh = lib2::create_shared_thread();
    auto th22 = th2_sh->get_id();

    auto all_threads = itc::get_all_registered_threads();
    std::cout << "registered threads = " << all_threads.size() << std::endl;


    int i = 0;
    while(!itc::this_thread::notified_for_exit())
    {
        if(i++ == 1000)
        {
            break;
        }
        itc::notify(th1);
        itc::notify(th11);
        itc::notify(th2);
        itc::notify(th22);

        std::cout << "th0 waiting ... " << i << std::endl;
        itc::this_thread::wait();
        std::cout << "th0 woke up ... " << i << std::endl;
    }

}

}
