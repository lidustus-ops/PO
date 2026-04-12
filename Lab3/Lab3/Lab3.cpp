#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <atomic>
#include <random>
#include <algorithm>
#include <iomanip>

using namespace std;

mutex cout_mutex;

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

    ~ThreadPool() { terminate(); }

    bool add_task(function<void()> task) {
        unique_lock<mutex> lock(m_queue_mutex);
        if (m_tasks.size() >= m_max_queue_size) {
            m_metrics.dropped_tasks++;
            return false;
        }

        m_tasks.push(move(task));

        if (m_tasks.size() == m_max_queue_size && !m_is_full) {
            m_is_full = true;
            m_full_since = chrono::steady_clock::now();
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
            if (m_stop) return;
            m_stop = true;
        }
        m_condition.notify_all();
        for (thread& worker : m_workers) {
            if (worker.joinable()) worker.join();
        }
    }

    void print_statistics() {
        lock_guard<mutex> io_lock(cout_mutex);
        cout << "\n--- Final Summary ---\n";
        cout << "Count of queue full: " << m_metrics.dropped_tasks.load() << "\n";

        double avg_wait = 0.0;
        {
            lock_guard<mutex> lock(m_metrics.metrics_mutex);
            if (m_metrics.wait_counts > 0) {
                avg_wait = m_metrics.total_wait_time / m_metrics.wait_counts;
            }
        }
        cout << "Average worker wait time: " << fixed << setprecision(4) << avg_wait << "s\n";

        lock_guard<mutex> lock(m_metrics.metrics_mutex);
        if (!m_metrics.queue_full_durations.empty()) {
            auto result = minmax_element(m_metrics.queue_full_durations.begin(), m_metrics.queue_full_durations.end());
            cout << "Queue full(min): " << fixed << setprecision(2) << *result.first << "s\n";
            cout << "Queue full(max): " << fixed << setprecision(2) << *result.second << "s\n";
        }
        else {
            cout << "Queue never reached full capacity.\n";
        }
        cout << "---------------------\n";
    }

private:
    void worker_routine() {
        while (true) {
            function<void()> task;
            {
                unique_lock<mutex> lock(m_queue_mutex);

                auto wait_start = chrono::steady_clock::now();

                m_condition.wait(lock, [this] {
                    return (m_stop || (!m_tasks.empty() && !m_paused));
                    });

                auto wait_end = chrono::steady_clock::now();
                double wait_duration = chrono::duration<double>(wait_end - wait_start).count();

                m_metrics.add_wait_time(wait_duration);

                if (m_stop && m_tasks.empty()) return;

                if (m_is_full) {
                    auto duration = chrono::duration<double>(chrono::steady_clock::now() - m_full_since).count();
                    m_metrics.add_full_duration(duration);
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
    chrono::steady_clock::time_point m_full_since;
    PoolMetrics m_metrics;
};

void run_task(int producer_id, int task_id, ThreadPool& pool) {
    static random_device rd;
    static mt19937 gen(rd());
    uniform_int_distribution<> dis(5, 10);
    int work_time = dis(gen);

    {
        lock_guard<mutex> lock(cout_mutex);
        cout << "Active task " << task_id << " from p" << producer_id
            << " (wait: " << pool.current_queue_size() << ", time: " << work_time << "s)\n";
    }

    this_thread::sleep_for(chrono::seconds(work_time));

    {
        lock_guard<mutex> lock(cout_mutex);
        cout << "Task " << task_id << " from p" << producer_id << " finished\n";
    }
}

int main() {
    const int threads = 6;
    const int queue_limit = 15;
    ThreadPool pool(threads, queue_limit);

    cout << "Threads: " << threads << ", Queue limit: " << queue_limit << "\n\n";

    vector<thread> producers;
    for (int p = 1; p <= 3; ++p) {
        producers.emplace_back([&pool, p]() {
            for (int i = 1; i <= 8; ++i) {
                if (pool.add_task([p, i, &pool] { run_task(p, i, pool); })) {
                    lock_guard<mutex> lock(cout_mutex);
                    cout << "Added  task " << i << " from p" << p << "\n";
                }
                else {
                    lock_guard<mutex> lock(cout_mutex);
                    cerr << "Mistake: p" << p << " task " << i << "\n";
                }
                this_thread::sleep_for(chrono::milliseconds(250));
            }
            });
    }

    this_thread::sleep_for(chrono::seconds(2));
    cout << "\n--- Paused ---\n";
    pool.set_paused(true);
    this_thread::sleep_for(chrono::seconds(3));
    cout << "--- Resumed ---\n\n";
    pool.set_paused(false);

    for (auto& t : producers) if (t.joinable()) t.join();

    pool.terminate();
    pool.print_statistics();

    return 0;
}