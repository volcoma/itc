# itc - inter-thread communication
[![Build Status](https://travis-ci.org/volcoma/itc.svg?branch=master)](https://travis-ci.org/volcoma/itc)
[![Build status](https://ci.appveyor.com/api/projects/status/v8hg9lp8irous3jj?svg=true)](https://ci.appveyor.com/project/volcoma/itc)

C++14 library providing easy interface for inter-thread communication in a single process.
It has no dependencies except the standard library.

The whole idea behind it is that all the blocking calls like future.wait or this_thread::sleep_for
can actually process different tasks if someone invokes into that thread while it is waiting on something.
It provides abstractions like condition_variable, promise, future, shared_future, future continuations, async
which are standard conforming. 
It provides a very easy interface to attach any std thread to it and start invoking into it.
