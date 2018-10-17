#include "overhead_tests.h"
#include "itc/future.hpp"

#include "utils.hpp"
#include <chrono>

namespace overhead_tests
{

void run_tests()
{
	Informer info;

	sout() << "-------------------";
	sout() << "itc::invoke via argument (rvalue)";
	itc::invoke(itc::main_thread::get_id(), [](const Informer&) {}, Informer{});
	itc::this_thread::process();

	sout() << "-------------------";
	sout() << "itc::invoke via capture (rvalue)";
	itc::invoke(itc::main_thread::get_id(), [arg = Informer{}](){});
	itc::this_thread::process();

	sout() << "-------------------";
	sout() << "itc::async via argument (rvalue)";
	itc::async(itc::main_thread::get_id(), std::launch::async, [](const Informer&) {}, Informer{}).wait();

	sout() << "-------------------";
	sout() << "std::async via argument (rvalue)";
	std::async(std::launch::async, [](const Informer&) {}, Informer{}).wait();

	sout() << "-------------------";
	sout() << "itc::async via capture (rvalue)";
	itc::async(itc::main_thread::get_id(), std::launch::async, [arg = Informer{}](){}).wait();

	sout() << "-------------------";
	sout() << "std::async via capture (rvalue)";
	std::async(std::launch::async, [arg = Informer{}](){}).wait();

	sout() << "-------------------";
	sout() << "itc::async via argument  (lvalue)";
	itc::async(itc::main_thread::get_id(), std::launch::async, [](const Informer&) {}, info).wait();

	sout() << "-------------------";
	sout() << "std::async via argument (lvalue)";
	std::async(std::launch::async, [](const Informer&) {}, info).wait();

	sout() << "-------------------";
	sout() << "itc::async via capture (lvalue)";
	itc::async(itc::main_thread::get_id(), std::launch::async, [info]() {}).wait();

	sout() << "-------------------";
	sout() << "std::async via capture (lvalue)";
	std::async(std::launch::async, [info]() {}).wait();
}
/*
 libstdc++ results
-------------------
itc::invoke via argument (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
itc::invoke via capture (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
itc::async via argument (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
std::async via argument (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
itc::async via capture (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
std::async via capture (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
itc::async via argument  (lvalue)
Informer(const Informer&&)
Informer(Informer&&)
-------------------
std::async via argument (lvalue)
Informer(const Informer&&)
Informer(Informer&&)
-------------------
itc::async via capture (lvalue)
Informer(const Informer&&)
Informer(Informer&&)
Informer(Informer&&)
-------------------
std::async via capture (lvalue)
Informer(const Informer&&)
Informer(Informer&&)
Informer(Informer&&)


msvc results
-------------------
itc::invoke via argument (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
-------------------
itc::invoke via capture (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
-------------------
itc::async via argument (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
-------------------
std::async via argument (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
-------------------
itc::async via capture (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
-------------------
std::async via capture (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
-------------------
itc::async via argument  (lvalue)
Informer(const Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
-------------------
std::async via argument (lvalue)
Informer(const Informer&&)
Informer(Informer&&)
Informer(Informer&&)
-------------------
itc::async via capture (lvalue)
Informer(const Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
-------------------
std::async via capture (lvalue)
Informer(const Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
*/
} // namespace overhead
