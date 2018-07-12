#pragma once
#include "itc/thread.h"

namespace lib1
{
itc::thread::id create_detached_thread();
itc::shared_thread create_shared_thread();
}
