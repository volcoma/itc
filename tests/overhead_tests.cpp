#include "overhead_tests.h"
#include "itc/future.hpp"

#include "utils.hpp"
#include <chrono>

namespace overhead_tests
{

struct Informer
{
	Informer()
	{
		puts("Informer()");
	}
	Informer(const Informer&) = delete;
	//	{
	//		puts("Informer(const Informer&)");
	//	}
	Informer(Informer&&) noexcept
	{
		puts("Informer(Informer&&)");
	}
	Informer& operator=(const Informer&) = delete;
	//	{
	//		puts("operator=(const Informer&)");
	//		return *this;
	//	}
	Informer& operator=(Informer&&) noexcept
	{
		puts("operator=(Informer&&)");
		return *this;
	}
};

void run_tests(int iterations)
{
	for(int i = 0; i < iterations; ++i)
	{
		sout() << "-------------------\n";
		sout() << "invoke via argument\n";
		itc::invoke(itc::main_id(), [](const Informer&) {}, Informer{});
		itc::this_thread::process();

		sout() << "-------------------\n";
		sout() << "invoke via capture\n";
		itc::invoke(itc::main_id(), [arg = Informer{}](){});
		itc::this_thread::process();

		sout() << "-------------------\n";
		sout() << "async via argument\n";
		itc::async(itc::main_id(), std::launch::async, [](const Informer&) {}, Informer{});
		itc::this_thread::process();

		sout() << "-------------------\n";
		sout() << "async via capture\n";
		itc::async(itc::main_id(), std::launch::async, [arg = Informer{}](){});
		itc::this_thread::process();
	}
}
} // namespace overhead
