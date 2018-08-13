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


struct Informer
{
    Informer()
    {
        sout() << "Informer()\n";
    }
    Informer(Informer&&) noexcept
    {
        sout() << "Informer(Informer&&)\n";
    }
    Informer& operator=(Informer&&) noexcept
    {
        sout() << "operator=(Informer&&)\n";
        return *this;
    }
    Informer(const Informer&) = delete;
    Informer& operator=(const Informer&) = delete;
};
