#include <coroutine>
#include <iostream>
#include <thread>
#include <chrono>

// ============================================================================
// EXAMPLE 1: Simple Generator - Produces a sequence of values
// ============================================================================

template<typename T>
struct Generator {
    struct promise_type {
        T current_value;
        std::exception_ptr exception;

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

        void unhandled_exception() {
            exception = std::current_exception();
        }
    };

    std::coroutine_handle<promise_type> handle;

    Generator(std::coroutine_handle<promise_type> h) : handle(h) {}

    ~Generator() {
        if (handle) handle.destroy();
    }

    // Move-only type
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    Generator(Generator&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }

    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }

    bool next() {
        handle.resume();
        if (handle.promise().exception) {
            std::rethrow_exception(handle.promise().exception);
        }
        return !handle.done();
    }

    T value() const {
        return handle.promise().current_value;
    }
};

// Generates Fibonacci numbers
Generator<int> fibonacci(int count) {
    std::cout << "[Coroutine] Starting Fibonacci generator\n";

    int a = 0, b = 1;
    for (int i = 0; i < count; ++i) {
        std::cout << "[Coroutine] About to yield: " << a << "\n";
        co_yield a;  // Suspend here and return value
        std::cout << "[Coroutine] Resumed after yielding " << a << "\n";

        int temp = a;
        a = b;
        b = temp + b;
    }

    std::cout << "[Coroutine] Fibonacci generator finishing\n";
}

// Generates a simple range of numbers
Generator<int> range(int start, int end) {
    std::cout << "[Coroutine] Range starting from " << start << " to " << end << "\n";

    for (int i = start; i < end; ++i) {
        co_yield i;
    }

    std::cout << "[Coroutine] Range complete\n";
}

// ============================================================================
// EXAMPLE 2: Task - Represents an async computation
// ============================================================================

template<typename T>
struct Task {
    struct promise_type {
        T value;
        std::exception_ptr exception;

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() { return {}; }  // Start immediately
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_value(T v) {
            std::cout << "[Task Promise] Storing return value: " << v << "\n";
            value = v;
        }

        void unhandled_exception() {
            exception = std::current_exception();
        }
    };

    std::coroutine_handle<promise_type> handle;

    Task(std::coroutine_handle<promise_type> h) : handle(h) {}

    ~Task() {
        if (handle) handle.destroy();
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
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

    bool is_ready() const {
        return handle.done();
    }
};

// Simple async computation
Task<int> compute_answer() {
    std::cout << "[Task] Starting computation...\n";
    std::cout << "[Task] Performing complex calculations...\n";

    // Simulate some work
    int result = 0;
    for (int i = 1; i <= 10; ++i) {
        result += i;
    }

    std::cout << "[Task] Computation complete!\n";
    co_return result;  // Return the final value
}

// Task that uses another value
Task<std::string> format_result(int value) {
    std::cout << "[Format Task] Formatting value: " << value << "\n";
    co_return "The answer is: " + std::to_string(value);
}

// ============================================================================
// EXAMPLE 3: Custom Awaiter - Sleep operation
// ============================================================================

struct SleepAwaiter {
    std::chrono::milliseconds duration;

    // Check if we can skip suspension
    bool await_ready() const noexcept {
        std::cout << "[Awaiter] Checking if sleep is needed...\n";
        return duration.count() <= 0;
    }

    // Called when suspending - schedule resume
    void await_suspend(std::coroutine_handle<> handle) const {
        std::cout << "[Awaiter] Suspending for " << duration.count() << "ms\n";

        std::thread([handle, d = duration]() {
            std::this_thread::sleep_for(d);
            std::cout << "[Awaiter Thread] Sleep complete, resuming coroutine\n";
            handle.resume();
        }).detach();
    }

    // Called when resuming - return result
    void await_resume() const noexcept {
        std::cout << "[Awaiter] Resumed after sleep\n";
    }
};

// Helper function to create awaiter
SleepAwaiter sleep_for(std::chrono::milliseconds ms) {
    return SleepAwaiter{ms};
}

// Task that uses co_await
Task<int> delayed_computation() {
    std::cout << "[Delayed Task] Starting...\n";

    std::cout << "[Delayed Task] About to sleep for 500ms\n";
    co_await sleep_for(std::chrono::milliseconds(500));

    std::cout << "[Delayed Task] Woke up! Computing result...\n";
    co_return 42;
}

// ============================================================================
// EXAMPLE 4: Demonstrating coroutine lifecycle
// ============================================================================

Generator<std::string> lifecycle_demo() {
    std::cout << "  [Lifecycle] Coroutine body starts executing\n";

    co_yield "First";
    std::cout << "  [Lifecycle] Between first and second yield\n";

    co_yield "Second";
    std::cout << "  [Lifecycle] Between second and third yield\n";

    co_yield "Third";
    std::cout << "  [Lifecycle] After last yield, before return\n";

    // When co_return or end of function is reached, final_suspend is called
}

// ============================================================================
// Main function - Run all examples
// ============================================================================

int main() {
    std::cout << "=== C++20 Coroutines Demo ===\n\n";

    // Example 1: Generator - Fibonacci
    std::cout << "--- Example 1: Fibonacci Generator ---\n";
    {
        auto fib = fibonacci(7);
        std::cout << "\n[Main] Created Fibonacci generator\n";
        std::cout << "[Main] Fibonacci numbers: ";

        while (fib.next()) {
            std::cout << fib.value() << " ";
        }
        std::cout << "\n";
    }
    std::cout << "[Main] Generator destroyed\n\n";

    // Example 2: Generator - Range
    std::cout << "--- Example 2: Range Generator ---\n";
    {
        auto r = range(5, 10);
        std::cout << "\n[Main] Numbers in range: ";

        while (r.next()) {
            std::cout << r.value() << " ";
        }
        std::cout << "\n\n";
    }

    // Example 3: Task - Simple computation
    std::cout << "--- Example 3: Simple Task ---\n";
    {
        auto task = compute_answer();
        std::cout << "[Main] Task created (started immediately due to suspend_never)\n";

        int result = task.get();
        std::cout << "[Main] Got result: " << result << "\n\n";
    }

    // Example 4: Task chain
    std::cout << "--- Example 4: Task Chain ---\n";
    {
        auto task1 = compute_answer();
        int value = task1.get();

        auto task2 = format_result(value);
        std::string message = task2.get();

        std::cout << "[Main] Final message: " << message << "\n\n";
    }

    // Example 5: Async with co_await
    std::cout << "--- Example 5: Async Task with co_await ---\n";
    {
        auto task = delayed_computation();
        std::cout << "[Main] Delayed task created, waiting for result...\n";

        int result = task.get();
        std::cout << "[Main] Got delayed result: " << result << "\n\n";

        // Give background thread time to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Example 6: Coroutine lifecycle
    std::cout << "--- Example 6: Coroutine Lifecycle ---\n";
    {
        std::cout << "[Main] Creating lifecycle demo coroutine\n";
        auto demo = lifecycle_demo();

        std::cout << "[Main] Calling next() #1\n";
        if (demo.next()) {
            std::cout << "  [Main] Got value: " << demo.value() << "\n";
        }

        std::cout << "[Main] Calling next() #2\n";
        if (demo.next()) {
            std::cout << "  [Main] Got value: " << demo.value() << "\n";
        }

        std::cout << "[Main] Calling next() #3\n";
        if (demo.next()) {
            std::cout << "  [Main] Got value: " << demo.value() << "\n";
        }

        std::cout << "[Main] Calling next() #4 (should complete)\n";
        if (demo.next()) {
            std::cout << "  [Main] Got value: " << demo.value() << "\n";
        } else {
            std::cout << "  [Main] Coroutine completed\n";
        }

        std::cout << "[Main] Lifecycle demo ending\n";
    }
    std::cout << "[Main] Lifecycle demo destroyed\n\n";

    std::cout << "=== All Examples Complete ===\n";

    return 0;
}
