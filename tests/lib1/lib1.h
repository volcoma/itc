#pragma once
#include "itc/itc.h"

namespace lib1
{
    std::thread::id create_detached_thread();
    itc::shared_thread create_shared_thread();
}
