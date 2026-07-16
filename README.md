# tinyState

**Write state machines. Forget threads.**

tinyState is a C++ framework for cooperative concurrency built on explicit,
always-traceable state machines. Each object is a coroutine-like state machine;
the framework owns a worker-thread pool and serializes state transitions so that
*"undefined state"* never arises from asynchronous actions. You write short
state functions that yield; the framework handles thread creation, scheduling,
join, and graceful shutdown.

It is less a library than a **programming strategy**: the same model has been
implemented in Perl and JavaScript. This repository is the C++ implementation
(v2), packaged as a reusable, installable library.

```cpp
TS_STATE(ACT_START) {
    child = thNEW(ChildClass, (ifThis, args));
    return ACT_WAIT_RETURN;          // yield, wait for an event
}

TS_STATE(ACT_WAIT_RETURN) {
    if ( ev->type != TSE_RETURN ) return 0;
    if ( ev->source != child )     return 0;
    return rDO | ACT_NEXT;           // transition immediately
}
```

## Why

- **Traceability over raw speed** — every state transition is a named value you
  can log and reason about. No hidden control flow.
- **Threads without the footguns** — no manual `pthread_create` / `join`; the
  framework guarantees each state machine a chance to shut down cleanly (a
  dedicated GC thread runs destructors at stack-top, so destructors may take
  locks without deadlocking).
- **Type-safe codegen** — a small generator (`tscpp2`) keeps C++ type safety
  while encoding transitions as named states.
- **Portable** — Linux, plus Windows via Cygwin (POSIX) and MSYS2 / MinGW-w64
  (native PE). Self-hosting CMake build on every platform; no boost, no curl,
  no OpenSSL dependency.

## Quick start

```sh
cmake -B build .
cmake --build build -j
sudo cmake --install build                 # → /usr/local (or --prefix <PREFIX>)

# build and run the hello-world example
cmake -S example/hello-world -B example/hello-world/build
cmake --build example/hello-world/build -j
./example/hello-world/build/hello-world
```

Full instructions for Linux, Cygwin, and MSYS2/MinGW-w64 are in
[docs/INSTALL.md](docs/INSTALL.md). Consuming tinyState from your own CMake
project is three lines:

```cmake
find_package(tinyState REQUIRED)
add_tinystate_example(NAME myapp
  SOURCES src/main/main.cpp src/classes/hw/c++/hwMyClass.cpp)
```

## Documentation

- **[docs/MENTAL_MODEL.md](docs/MENTAL_MODEL.md)** — start here: what tinyState
  is, its philosophy, the lifecycle (INI → ACT → FIN → ZOM), and how it compares
  to C++20 coroutines / boost::asio / protothreads / Erlang.
- **[docs/COOKBOOK.md](docs/COOKBOOK.md)** — recipes and patterns.
- **[docs/MACROS.md](docs/MACROS.md)** — macro reference (`TS_STATE`,
  `TS_THREAD`, `thNEW`, argument-setup macros, …).
- **[docs/REFERENCE.md](docs/REFERENCE.md)** — event types (`TSE_*`), utility
  classes, error handling, tracing.
- **[docs/GOTCHAS.md](docs/GOTCHAS.md)** — non-obvious behaviors and how to
  avoid the common traps.
- **[docs/INSTALL.md](docs/INSTALL.md)** / **[docs/BUILD_INTERNAL.md](docs/BUILD_INTERNAL.md)**
  — installing, and build-system internals.
- **[CLAUDE.md](CLAUDE.md)** — rules for AI assistants (and new contributors)
  adding code to tinyState.

Rendered API reference (Doxygen):
<https://project.globalbase.org/releases/tinyState/>

## Origin

tinyState grew from a personal portability library the author kept from around
2000, and became the backbone of **GLOBALBASE**, a distributed geographic
information system — where many server connections had to be managed *in a form
whose state is always traceable*. It evolved independently in the same era and
problem space as `boost::asio`.

Author: Hirohisa Mori — GLOBALBASE Project.

## License

BSD 3-Clause License. See [LICENSE](LICENSE).
