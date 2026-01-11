# C++20 Coroutines - Complete Guide

A comprehensive guide to understanding and using C++20 coroutines with practical examples, best practices, and common pitfalls.

## Table of Contents
1. [Introduction](#introduction)
2. [Core Concepts](#core-concepts)
3. [Typical Use Cases](#typical-use-cases)
4. [Code Examples](#code-examples)
5. [Recommendations & Best Practices](#recommendations--best-practices)
6. [Common Errors & Pitfalls](#common-errors--pitfalls)
7. [Building the Project](#building-the-project)

## Introduction

C++20 introduced coroutines as a language feature for writing asynchronous and generator-style code. Unlike traditional functions, coroutines can suspend execution and resume later, maintaining their state between suspensions.

### What Makes a Function a Coroutine?

A function becomes a coroutine when it uses one of these keywords:
- `co_await` - Suspend execution until an awaitable completes
- `co_yield` - Suspend execution and produce a value
- `co_return` - Complete the coroutine and optionally return a value

## Core Concepts

### 1. Promise Type
The promise type controls the coroutine's behavior. It defines:
- How the coroutine is created (`get_return_object()`)
- Initial suspension behavior (`initial_suspend()`)
- Final suspension behavior (`final_suspend()`)
- How values are returned (`return_value()` or `return_void()`)
- Exception handling (`unhandled_exception()`)

### 2. Coroutine Handle
`std::coroutine_handle<PromiseType>` is a low-level handle to control coroutine execution:
- `resume()` - Resume the suspended coroutine
- `done()` - Check if coroutine has completed
- `destroy()` - Destroy the coroutine frame

### 3. Awaitables
Objects that can be used with `co_await`. Must implement:
- `await_ready()` - Returns true if result is immediately available
- `await_suspend(handle)` - Called when suspending
- `await_resume()` - Called when resuming, returns the result

## Typical Use Cases

### 1. Generators
Generate sequences of values lazily without allocating the entire sequence in memory.

**Use when:**
- Generating infinite or large sequences
- Values are computed on-demand
- Memory efficiency is important

**Example:** Fibonacci sequence, range generators, tree traversals

### 2. Asynchronous Operations
Handle async I/O, network requests, or long-running operations without blocking.

**Use when:**
- Performing I/O operations
- Making network requests
- Coordinating multiple async tasks
- Avoiding callback hell

**Example:** HTTP clients, database queries, file operations

### 3. Cooperative Multitasking
Implement user-space task scheduling without OS threads.

**Use when:**
- Building custom task schedulers
- Game engine update loops
- Event-driven systems
- Need fine-grained control over execution

### 4. State Machines
Express complex state machines in a linear, readable way.

**Use when:**
- Parsing protocols
- Implementing game AI
- UI flow control
- Complex business logic with multiple states

## Code Examples

### Example 1: Simple Generator

```cpp
#include <coroutine>
#include <iostream>

template<typename T>
struct Generator {
    struct promise_type {
        T current_value;

        Generator get_return_object() {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        std::suspend_always yield_value(T value) {
            current_value = value;
            return {};
        }

        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> handle;

    Generator(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~Generator() { if (handle) handle.destroy(); }

    // Delete copy, allow move
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    Generator(Generator&& other) : handle(other.handle) { other.handle = nullptr; }
    Generator& operator=(Generator&& other) {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }

    bool next() {
        handle.resume();
        return !handle.done();
    }

    T value() const {
        return handle.promise().current_value;
    }
};

// Usage: Generate Fibonacci numbers
Generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield a;
        auto temp = a;
        a = b;
        b = temp + b;
    }
}
```

### Example 2: Async Task

```cpp
#include <coroutine>
#include <future>
#include <thread>

template<typename T>
struct Task {
    struct promise_type {
        T value;
        std::exception_ptr exception;

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_value(T v) { value = v; }

        void unhandled_exception() { exception = std::current_exception(); }
    };

    std::coroutine_handle<promise_type> handle;

    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~Task() { if (handle) handle.destroy(); }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& other) : handle(other.handle) { other.handle = nullptr; }
    Task& operator=(Task&& other) {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }

    T get() {
        if (!handle.done()) {
            handle.resume();
        }
        if (handle.promise().exception) {
            std::rethrow_exception(handle.promise().exception);
        }
        return handle.promise().value;
    }
};

// Awaitable for std::future
template<typename T>
struct FutureAwaiter {
    std::future<T> future;

    bool await_ready() {
        return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    void await_suspend(std::coroutine_handle<> handle) {
        std::thread([this, handle]() mutable {
            future.wait();
            handle.resume();
        }).detach();
    }

    T await_resume() { return future.get(); }
};

// Usage example
Task<int> async_computation() {
    // Simulate async work
    std::promise<int> promise;
    auto future = promise.get_future();

    std::thread([p = std::move(promise)]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        p.set_value(42);
    }).detach();

    int result = co_await FutureAwaiter<int>{std::move(future)};
    co_return result * 2;
}
```

### Example 3: Custom Awaiter

```cpp
#include <coroutine>
#include <chrono>
#include <thread>

struct SleepAwaiter {
    std::chrono::milliseconds duration;

    bool await_ready() const noexcept { return duration.count() <= 0; }

    void await_suspend(std::coroutine_handle<> handle) const {
        std::thread([handle, d = duration]() {
            std::this_thread::sleep_for(d);
            handle.resume();
        }).detach();
    }

    void await_resume() const noexcept {}
};

// Helper function
SleepAwaiter sleep_for(std::chrono::milliseconds ms) {
    return SleepAwaiter{ms};
}
```

## Recommendations & Best Practices

### 1. Memory Management
- **Always implement proper RAII** for coroutine handles
- **Use move semantics** for coroutine return types (never copy)
- **Destroy handles** in destructors to prevent leaks
- **Be careful with lifetimes** of captured references

```cpp
// GOOD: RAII wrapper
struct Generator {
    ~Generator() { if (handle) handle.destroy(); }
    Generator(Generator&&) = default;  // Move is OK
    Generator(const Generator&) = delete;  // Copy is dangerous
};

// BAD: Raw handle without cleanup
std::coroutine_handle<> handle;  // Who destroys this?
```

### 2. Exception Safety
- **Always implement `unhandled_exception()`** in promise_type
- **Never let exceptions escape** from `final_suspend()`
- **Store exceptions** in the promise for later retrieval

```cpp
void unhandled_exception() {
    exception_ptr = std::current_exception();  // Store for later
}

std::suspend_always final_suspend() noexcept {  // Must be noexcept!
    return {};
}
```

### 3. Suspension Points
- **Use `std::suspend_always`** to suspend unconditionally
- **Use `std::suspend_never`** to continue without suspension
- **Return by value** from suspension functions (they're cheap)

```cpp
std::suspend_always initial_suspend() { return {}; }  // Lazy start
std::suspend_never initial_suspend() { return {}; }   // Eager start
```

### 4. Performance Considerations
- **Coroutines have overhead** - not always faster than callbacks
- **Compiler optimizations matter** - use `-O2` or higher
- **Heap allocation** - coroutine frames are typically heap-allocated
- **Consider HALO** (Heap Allocation eLision Optimization) for small frames

### 5. Design Patterns
- **Separate promise and return type** for clarity
- **Make return types move-only** to prevent dangling references
- **Provide iterator interfaces** for generators
- **Use symmetric transfer** for efficient coroutine chains

```cpp
// Symmetric transfer example (C++20)
std::coroutine_handle<> await_suspend(std::coroutine_handle<> handle) {
    return next_coroutine_handle;  // Direct transfer, no stack growth
}
```

## Common Errors & Pitfalls

### 1. Dangling References

**Problem:** Capturing local variables by reference that outlive the coroutine frame.

```cpp
// WRONG - Dangling reference!
Generator<int> bad_generator() {
    int local = 42;
    co_yield local;  // OK - copies the value

    int& ref = local;
    co_yield ref;  // DANGER - yields reference to local variable
}

std::string get_name() { return "Alice"; }

Task<void> bad_task() {
    const std::string& name = get_name();  // Dangling!
    co_await something();
    std::cout << name;  // Use-after-free!
}
```

**Solution:** Copy values or ensure referenced objects outlive the coroutine.

```cpp
// CORRECT
Task<void> good_task() {
    std::string name = get_name();  // Own the data
    co_await something();
    std::cout << name;  // Safe!
}
```

### 2. Forgetting to Resume

**Problem:** Creating a coroutine but never calling `resume()`.

```cpp
// WRONG - Memory leak!
auto gen = fibonacci();
// Never call gen.next() or gen.resume()
// Coroutine frame leaks!
```

**Solution:** Ensure coroutines are driven to completion or explicitly destroyed.

```cpp
// CORRECT
auto gen = fibonacci();
while (gen.next()) {
    std::cout << gen.value() << "\n";
}
// Destructor cleans up
```

### 3. Use-After-Destroy

**Problem:** Resuming a destroyed or completed coroutine.

```cpp
// WRONG
auto gen = fibonacci();
gen.next();
gen.handle.destroy();
gen.next();  // CRASH! Use-after-free
```

**Solution:** Check `done()` status and never resume after destruction.

```cpp
// CORRECT
if (!gen.handle.done()) {
    gen.next();
}
```

### 4. Missing Promise Type Members

**Problem:** Incomplete promise_type definition.

```cpp
// WRONG - Missing required methods
struct promise_type {
    auto get_return_object() { return Task{...}; }
    // Missing initial_suspend, final_suspend, unhandled_exception, etc.
};
```

**Solution:** Implement all required promise_type methods:

```cpp
// CORRECT - Complete promise_type
struct promise_type {
    Task get_return_object() { return Task{...}; }
    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void unhandled_exception() { std::terminate(); }
    void return_void() {}  // or return_value(T)
};
```

### 5. Mixing co_return Styles

**Problem:** Using both `return_value()` and `return_void()` incorrectly.

```cpp
// WRONG
struct promise_type {
    void return_void() {}
    void return_value(int) {}  // Can't have both!
};
```

**Solution:** Choose one based on your coroutine's return behavior:

```cpp
// For coroutines that return a value
void return_value(T value) { result = value; }

// For coroutines that don't return a value
void return_void() {}
```

### 6. Exceptions in final_suspend

**Problem:** Allowing exceptions to escape `final_suspend()`.

```cpp
// WRONG - Not noexcept
std::suspend_always final_suspend() {
    may_throw();  // Undefined behavior if this throws!
    return {};
}
```

**Solution:** Mark `final_suspend()` as `noexcept` and don't throw:

```cpp
// CORRECT
std::suspend_always final_suspend() noexcept {
    return {};
}
```

### 7. Coroutine Frame Lifetime Issues

**Problem:** Not understanding that the coroutine frame must outlive all co_await operations.

```cpp
// WRONG
Task<int> create_task() {
    co_return 42;
}

void bad() {
    auto task = create_task();
    // task destroyed here, but coroutine might not be done!
}
```

**Solution:** Ensure proper lifetime management:

```cpp
// CORRECT
void good() {
    auto task = create_task();
    int result = task.get();  // Wait for completion
    // Now safe to destroy
}
```

### 8. Forgetting to co_await

**Problem:** Calling a coroutine without co_await treats it as a regular function call.

```cpp
// WRONG
Task<void> async_operation() {
    async_work();  // If this returns a Task, it's not awaited!
    co_return;
}
```

**Solution:** Always co_await coroutine calls:

```cpp
// CORRECT
Task<void> async_operation() {
    co_await async_work();
    co_return;
}
```

## Building the Project

### Requirements
- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019 16.8+)
- CMake 3.15 or higher

### Build Instructions

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Compiler-Specific Notes

**GCC:**
```bash
g++ -std=c++20 -fcoroutines main.cpp -o coroutine_prj
```

**Clang:**
```bash
clang++ -std=c++20 -stdlib=libc++ main.cpp -o coroutine_prj
```

**MSVC:**
```bash
cl /std:c++20 /EHsc main.cpp
```

## Further Reading

- [C++ Reference: Coroutines](https://en.cppreference.com/w/cpp/language/coroutines)
- [Lewis Baker's Coroutine Theory](https://lewissbaker.github.io/)
- [CppCon talks on Coroutines](https://www.youtube.com/results?search_query=cppcon+coroutines)
- [P0057R8: Coroutines TS](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p0057r8.html)

## License

This educational project is provided as-is for learning purposes.
