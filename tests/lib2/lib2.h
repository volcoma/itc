#pragma once
#include "itc/thread.h"

namespace lib2
{
std::thread::id create_detached_thread();
itc::shared_thread create_shared_thread();

}
