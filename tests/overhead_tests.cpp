#include "overhead_tests.h"
#include "itc/future.hpp"

#include "utils.hpp"
#include <chrono>

namespace overhead_tests
{

void run_tests()
{
	Informer info;

	sout() << "-------------------\n";
	sout() << "invoke via argument (rvalue)\n";
	itc::invoke(itc::main_id(), [](const Informer&) {}, Informer{});
	itc::this_thread::process();

	sout() << "-------------------\n";
	sout() << "invoke via capture (rvalue)\n";
	itc::invoke(itc::main_id(), [arg = Informer{}](){});
	itc::this_thread::process();

	sout() << "-------------------\n";
	sout() << "async via argument (rvalue)\n";
	itc::async(itc::main_id(), std::launch::async, [](const Informer&) {}, Informer{}).wait();

	sout() << "-------------------\n";
	sout() << "std::async via argument (rvalue)\n";
	std::async(std::launch::async, [](const Informer&) {}, Informer{}).wait();

	sout() << "-------------------\n";
	sout() << "async via capture (rvalue)\n";
	itc::async(itc::main_id(), std::launch::async, [arg = Informer{}](){}).wait();

	sout() << "-------------------\n";
	sout() << "std::async via capture (rvalue)\n";
	std::async(std::launch::async, [arg = Informer{}](){}).wait();

	sout() << "-------------------\n";
	sout() << "async via argument  (lvalue)\n";
	itc::async(itc::main_id(), std::launch::async, [](const Informer&) {}, info).wait();

	sout() << "-------------------\n";
	sout() << "std::async via argument (lvalue)\n";
	std::async(std::launch::async, [](const Informer&) {}, info).wait();

	sout() << "-------------------\n";
	sout() << "async via capture (lvalue)\n";
	itc::async(itc::main_id(), std::launch::async, [info]() {}).wait();

	sout() << "-------------------\n";
	sout() << "std::async via capture (lvalue)\n";
	std::async(std::launch::async, [info]() {}).wait();
}
/*
 gcc results
-------------------
invoke via argument (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
invoke via capture (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
async via argument (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
std::async via argument (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
async via capture (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
std::async via capture (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
-------------------
async via argument  (lvalue)
Informer(const Informer&&)
Informer(Informer&&)
-------------------
std::async via argument (lvalue)
Informer(const Informer&&)
Informer(Informer&&)
-------------------
async via capture (lvalue)
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
invoke via argument (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
-------------------
invoke via capture (rvalue)
Informer()
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
Informer(Informer&&)
-------------------
async via argument (rvalue)
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
async via capture (rvalue)
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
async via argument  (lvalue)
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
async via capture (lvalue)
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
