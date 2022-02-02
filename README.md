# Meson++

An experiment to implement Meson in C++

The project is advancing rapidly, but there's still a *lot* of work to do
before it can be considered production ready.

## What is the status of Meson++? Can I use it for my project?

Currently, Meson++ is capable of compiling simple C++ executables 
and static archives, as well as some custom_targets. There is a lot of
functionality still missing, including dependencies.

Currently, I'm pushing toward getting meson++ to be self hosting, to that
end some features may not be implemented completely, only enough to self host.
Once self hosting is achieved, I'll begin to flesh out support and fix bugs.

## FAQ

### Why another Meson implementation?

I had two reasons for doing this. First, it was an interesting challenge and
a learning experience. I've never written something this big from scratch,
and certainly not in a language that isn't Python.

Second, I wanted to solve Meson's bootstrap problem. Python is a fantastic
language in a lot of ways, but, as a high level language it's easy to end up
in a bootstrap loop, you need Python to build X which you need to build
Python. Meson++ can avoid that be being written in a common low level
language, and use conditional compilation to minimize dependencies.

### Why C++‽ C++ is ...!

C++ is many things, some of them good, some of them bad. I personally feel
that some of its bad reputation comes from those who've only experienced C++
before C++11, modern C++ is much nicer.

I did consider two other languages before I started writing, Rust and
Haskell. Both of these languages have their strengths, and would have made
some things easier. However, in both cases they would complicate my primary
motivation, solving the boostrapping issue.

Given that goal, I really only had two choices: C and C++. I will choose C++
every time given those choices. It provides better cross platform interfaces,
resource management (RAII), and programming features: generics, overloading,
namespaces, strings that aren't char arrays, etc.

## Differences between Meson and Meson++

### Feature not bug compatibility

Meson++ strives for feature parity (currently aiming for Meson 0.58), but not
bug parity. So Meson++ may advertise support for Meson >= 0.58.0, but
may not have all of the bugs of 0.58.0.

### Modules

Meson's modules represent a huge amount of time and energy to produce and
maintain. I simply do not have the time and energy to re-implement every one
of them to feature parity. I will make an effort to keep a few of the very
common and important ones working, but help is very much welcome for the rest.

### Command line and env variables

Though not yet implemented, Meson++ will likely not provide a drop-in
replacement for Meson's cli interface. If there is need for that a wrapper
script could be provided.
