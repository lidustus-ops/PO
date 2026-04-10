#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadPool {
public:
    ThreadPool(size_t threads) : m_stop(false) {
        for (size_t i = 0; i < threads; ++i) {
            m_workers.emplace_back(&ThreadPool::worker_routine, this);
        }
    }

    ~ThreadPool() {
        { std::lock_guard<std::mutex> lock(m_queue_mutex); m_stop = true; }
        m_condition.notify_all();
        for (std::thread& worker : m_workers) if (worker.joinable()) worker.join();
    }

    void add_task(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            m_tasks.push(std::move(task));
        }
        m_condition.notify_one();
    }

private:
    void worker_routine() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                m_condition.wait(lock, [this] { return m_stop || !m_tasks.empty(); });
                if (m_stop && m_tasks.empty()) return;
                task = std::move(m_tasks.front());
                m_tasks.pop();
            }
            task();
        }
    }

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_queue_mutex;
    std::condition_variable m_condition;
    bool m_stop;
};

int main() {
    ThreadPool pool(4);
    pool.add_task([] { std::cout << "Core pool is working\n"; });
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}