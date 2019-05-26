# itc - inter-thread communication
[![Build Status](https://travis-ci.org/volcoma/itc.svg?branch=master)](https://travis-ci.org/volcoma/itc)
[![Build status](https://ci.appveyor.com/api/projects/status/v8hg9lp8irous3jj?svg=true)](https://ci.appveyor.com/project/volcoma/itc)

C++14 library providing easy interface for inter-thread communication in a single process.
It has no dependencies except the standard library.

The whole idea behind it is that all the blocking calls like future::wait or this_thread::sleep_for
can actually process different tasks if someone invokes into that thread while it is waiting on something.
It provides abstractions like condition_variable, promise, future, shared_future, future continuations, async
which are standard conforming. 
It provides a very easy interface to attach any std thread to it and start invoking into it.

```c++
std::thread make_std_thread()
{
	std::thread th([]() {
		itc::this_thread::register_this_thread();

		while(!itc::this_thread::notified_for_exit())
		{
			itc::this_thread::wait();
		}

		itc::this_thread::unregister_this_thread();
	});
	itc::register_thread(th.get_id());
	return th;
}
itc::thread::id make_detached_std_thread()
{
	std::thread th([]() {
		itc::this_thread::register_this_thread();

		while(!itc::this_thread::notified_for_exit())
		{
			itc::this_thread::wait();
		}

		itc::this_thread::unregister_this_thread();
	});
	auto id = itc::register_thread(th.get_id());
	th.detach();

	return id;
}

itc::thread::id create_detached_itc_thread()
{
	auto th = itc::make_thread();
	th.detach();
	return th.get_id();
}

int main()
{
	itc::init_data data;
	data.log_error = [](const std::string& msg) { sout() << msg << "\n"; };
	data.log_info = [](const std::string& msg) { sout() << msg << "\n"; };
	data.set_thread_name = [](std::thread& thread, const std::string& name) { /*some platform specific code to name a thread*/ };
	itc::init(data);
	
	{
		// you can register any std::thread
		auto std_thread = make_std_thread();
		auto std_thread_mapped_id = itc::register_thread(std_thread.get_id());
		auto std_thread_detached_mapped_id = make_detached_std_thread();

		// or you can use itc's make_thread which will 
		// basically do the same as make_std_thread
		auto itc_thread = itc::make_thread("some_name");
		auto itc_thread_detached_id = create_detached_itc_thread();

		while(!itc::this_thread::notified_for_exit())
		{
			// process this_thread's pending tasks if any.
			// Also any other blocking call will try to do so
			itc::this_thread::process();
			
			/// Sleeps for the specified duration and allow tasks to be processed during
			/// that time.
			//itc::this_thread::sleep_for(16ms);
			
			/// Blocks until notified with an event and process it.
			//itc::this_thread::wait();
			
			/// Blocks until specified timeout_duration has elapsed or
			/// notified, whichever comes first.
			//itc::this_thread::wait_for(16ms);

			int arg = 12;
			// clang-format off
			itc::invoke(std_thread_mapped_id,
			[](int arg)
			{
				sout() << "on std::thread " << arg;

				itc::invoke(itc::main_thread::get_id(),
				[](int arg)
				{
					sout() << "on main_thread from std::thread " << arg;
				}, arg);

			}, i);

			itc::invoke(std_thread_detached_mapped_id,
			[](int arg)
			{
				sout() << "on detached std::thread " << arg;

				itc::invoke(itc::main_thread::get_id(),
				[](int arg)
				{
					sout() << "on main_thread from detached std::thread " << arg;
				}, arg);

			}, arg);
			itc::invoke(itc_thread.get_id(),
			[](int arg)
			{
				sout() << "on itc::thread " << arg;

				itc::invoke(itc::main_thread::get_id(),
				[](int arg)
				{
					sout() << "on main_thread from itc::thread " << arg;
				}, arg);

			}, arg);
			itc::invoke(itc_thread_detached_id,
			[](int arg)
			{
				sout() << "on detached itc::thread " << arg;

				itc::invoke(itc::main_thread::get_id(),
				[](int arg)
				{
					sout() << "on main_thread from detached itc::thread " << arg;
				}, arg);

			}, arg);
			// clang-format on
		}

		// for std::thread we have to do this
		itc::notify_for_exit(std_thread_mapped_id);
		if(std_thread.joinable())
		{
			std_thread.join();
		}
		
		// itc::thread will behave as a proper RAII type and join in its destructor
	}

	//you can also make use of future/promise and continuations
	{
		auto thread1 = itc::make_thread("some_name1");
		auto thread2 = itc::make_thread();

		auto th1_id = thread1.get_id();
		auto th2_id = thread2.get_id();
		auto this_th_id = itc::this_thread::get_id();

		int iterations = 15;
		for(int i = 0; i < iterations; ++i)
		{

			// clang-format off

			// async + chaining continuations
			{
				// some move only object
				// can pass it by move either to the capture list or as a parameter to async
				auto up = std::make_unique<int>(5);
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
						sout() << "chain then " << result;
						return result;
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

					try
					{
						sout() << "wait on chain\n";
						auto result = chain.get();
						sout() << "woke up on chain with result " << result;
					}
					catch(const std::exception& e)
					{
						sout() << e.what();
					}
				}
			}

			// clang-format on

			sout() << "future woke up for" << i;
			sout() << "ASYNC TEST " << i << " completed\n";
		}
		
		// itc::thread will behave as a proper RAII type and join in its destructor
	}

	return itc::shutdown();
}
