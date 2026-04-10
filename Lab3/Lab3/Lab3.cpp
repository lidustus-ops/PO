#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    ThreadPool(size_t threads, size_t max_queue_size)
        : m_max_queue_size(max_queue_size), m_stop(false) {
        for (size_t i = 0; i < threads; ++i) {
            m_workers.emplace_back(&ThreadPool::worker_routine, this);
        }
    }

    ~ThreadPool() { terminate(); }

    bool add_task(std::function<void()> task) {
        std::unique_lock<std::mutex> lock(m_queue_mutex);
        if (m_tasks.size() >= m_max_queue_size) return false;
        m_tasks.push(std::move(task));
        m_condition.notify_one();
        return true;
    }

    void terminate() {
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            if (m_stop) return;
            m_stop = true;
        }
        m_condition.notify_all();
        for (std::thread& worker : m_workers) if (worker.joinable()) worker.join();
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
    size_t m_max_queue_size;
    std::atomic<bool> m_stop;
};

int main() {

    const int threads = 6;
    const int queue_limit = 15;
    ThreadPool pool(threads, queue_limit);

    pool.add_task([] { std::cout << "Thread pool core is active\n"; });
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}