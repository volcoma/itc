#pragma once
#include <mutex>
#include <iostream>
#include <sstream>
struct sout
{
    ~sout()
    {
        static std::mutex guard;
        std::lock_guard<std::mutex> lock(guard);
        std::cout << str.str();
    }
    template<typename T>
    sout& operator<<(T&& val)
    {
        str << val;
        return *this;
    }

    std::stringstream str;
};
