#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <atomic>

using namespace std;
using namespace std::chrono;

struct PoolMetrics {
    atomic<int> dropped_tasks{ 0 };
    vector<double> queue_full_durations;
    double total_wait_time{ 0.0 };
    int wait_counts{ 0 };
    mutex metrics_mutex;

    void add_full_duration(double duration) {
        lock_guard<mutex> lock(metrics_mutex);
        queue_full_durations.push_back(duration);
    }

    void add_wait_time(double duration) {
        lock_guard<mutex> lock(metrics_mutex);
        total_wait_time += duration;
        wait_counts++;
    }
};

class ThreadPool {
public:
    ThreadPool(size_t threads, size_t max_queue_size)
        : m_max_queue_size(max_queue_size), m_stop(false), m_paused(false), m_is_full(false) {
        for (size_t i = 0; i < threads; ++i) {
            m_workers.emplace_back(&ThreadPool::worker_routine, this);
        }
    }

    ~ThreadPool() {
        terminate();
    }

    bool add_task(function<void()> task) {
        unique_lock<mutex> lock(m_queue_mutex);

        if (m_tasks.size() >= m_max_queue_size) {
            m_metrics.dropped_tasks++;
            return false;
        }

        m_tasks.push(move(task));

        if (m_tasks.size() == m_max_queue_size && !m_is_full) {
            m_is_full = true;
            m_full_since = steady_clock::now();
        }

        m_condition.notify_one();
        return true;
    }

    size_t current_queue_size() {
        lock_guard<mutex> lock(m_queue_mutex);
        return m_tasks.size();
    }

    void set_paused(bool paused) {
        {
            lock_guard<mutex> lock(m_queue_mutex);
            m_paused = paused;
        }
        if (!paused) m_condition.notify_all();
    }

    void terminate() {
        {
            lock_guard<mutex> lock(m_queue_mutex);
            m_stop = true;
        }
        m_condition.notify_all();
        for (auto& w : m_workers) {
            if (w.joinable()) w.join();
        }
    }

private:
    void worker_routine() {
        while (true) {
            function<void()> task;
            {
                unique_lock<mutex> lock(m_queue_mutex);
                auto wait_start = steady_clock::now();

                m_condition.wait(lock, [this] {
                    return m_stop || (!m_tasks.empty() && !m_paused);
                    });

                auto wait_end = steady_clock::now();
                m_metrics.add_wait_time(duration<double>(wait_end - wait_start).count());

                if (m_stop && m_tasks.empty()) return;

                if (m_is_full) {
                    auto duration_val = duration<double>(steady_clock::now() - m_full_since).count();
                    m_metrics.add_full_duration(duration_val);
                    m_is_full = false;
                }

                task = move(m_tasks.front());
                m_tasks.pop();
            }
            task();
        }
    }

    vector<thread> m_workers;
    queue<function<void()>> m_tasks;
    mutex m_queue_mutex;
    condition_variable m_condition;
    size_t m_max_queue_size;
    atomic<bool> m_stop;
    bool m_paused, m_is_full;
    steady_clock::time_point m_full_since;
    PoolMetrics m_metrics;
};

int main() {
    const int threads_count = 6;
    const int queue_limit = 15;

    ThreadPool pool(threads_count, queue_limit);

    pool.add_task([] {
        this_thread::sleep_for(milliseconds(500));
        cout << "Task executed." << endl;
        });

    cout << "Current queue size: " << pool.current_queue_size() << endl;

    this_thread::sleep_for(seconds(1));

    return 0;
}