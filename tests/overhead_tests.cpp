#include "overhead_tests.h"
#include "utils.hpp"

#include <threadpp/future.hpp>
#include <chrono>

namespace overhead_tests
{

void run_tests()
{
	Informer info;

	sout() << "-------------------";
	sout() << "tpp::invoke via argument (rvalue)";
	tpp::invoke(tpp::main_thread::get_id(), [](const Informer&) {}, Informer{});
	tpp::this_thread::process();

	sout() << "-------------------";
	sout() << "tpp::invoke via capture (rvalue)";
	tpp::invoke(tpp::main_thread::get_id(), [arg = Informer{}](){});
	tpp::this_thread::process();

	sout() << "-------------------";
	sout() << "tpp::async via argument (rvalue)";
	tpp::async(tpp::main_thread::get_id(), std::launch::async, [](const Informer&) {}, Informer{}).wait();

	sout() << "-------------------";
	sout() << "std::async via argument (rvalue)";
	std::async(std::launch::async, [](const Informer&) {}, Informer{}).wait();

	sout() << "-------------------";
	sout() << "tpp::async via capture (rvalue)";
	tpp::async(tpp::main_thread::get_id(), std::launch::async, [arg = Informer{}](){}).wait();

	sout() << "-------------------";
	sout() << "std::async via capture (rvalue)";
	std::async(std::launch::async, [arg = Informer{}](){}).wait();

	sout() << "-------------------";
	sout() << "tpp::async via argument  (lvalue)";
	tpp::async(tpp::main_thread::get_id(), std::launch::async, [](const Informer&) {}, info).wait();

	sout() << "-------------------";
	sout() << "std::async via argument (lvalue)";
	std::async(std::launch::async, [](const Informer&) {}, info).wait();

	sout() << "-------------------";
	sout() << "tpp::async via capture (lvalue)";
	tpp::async(tpp::main_thread::get_id(), std::launch::async, [info]() {}).wait();

	sout() << "-------------------";
	sout() << "std::async via capture (lvalue)";
	std::async(std::launch::async, [info]() {}).wait();
}
/*
 libstdc++ results
-------------------
tpp::invoke via argument (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
tpp::invoke via capture (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
tpp::async via argument (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
std::async via argument (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
tpp::async via capture (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
std::async via capture (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
tpp::async via argument  (lvalue)
Informer(const Informer&&)
Informer(Informer&&)
-------------------
std::async via argument (lvalue)
Informer(const Informer&&)
Informer(Informer&&)
-------------------
tpp::async via capture (lvalue)
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
tpp::invoke via argument (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
-------------------
tpp::invoke via capture (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
-------------------
tpp::async via argument (rvalue)
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
tpp::async via capture (rvalue)
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
tpp::async via argument  (lvalue)
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
tpp::async via capture (lvalue)
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
