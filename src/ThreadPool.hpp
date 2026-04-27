#pragma once

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <type_traits>
#include <stdexcept>
#include <thread>

using namespace std;

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    function<void()> task;
                    {
                        unique_lock<mutex> lock(queue_mutex);
                        condition.wait(lock, [this] {
                            return stop || !tasks.empty();
                        });
                        if (stop && tasks.empty())
                            return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            lock_guard<mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        // jthreads auto-join in destructor
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> future<std::invoke_result_t<F, Args...>>
    {
        using return_type = std::invoke_result_t<F, Args...>;
        
        auto task = make_shared<packaged_task<return_type()>>(
            bind(forward<F>(f), forward<Args>(args)...)
        );
        
        future<return_type> result = task->get_future();
        {
            lock_guard<mutex> lock(queue_mutex);
            if (stop)
                throw runtime_error("enqueue on stopped ThreadPool");
            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return result;
    }

    size_t tasks_remaining() {
        lock_guard<mutex> lock(queue_mutex);
        return tasks.size();
    }

private:
    vector<jthread> workers;
    queue<function<void()>> tasks;
    mutex queue_mutex;
    condition_variable condition;
    bool stop;
};
