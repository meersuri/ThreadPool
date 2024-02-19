#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <vector>
#include <thread>
#include <tuple>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <spdlog/spdlog.h>

#include "sync_queue.hpp"

class Task {
    public:
        virtual bool execute() = 0; // if execute returns true we put it back on the task queue
        virtual ~Task() {}
};

template <typename F, typename... Args>
class CallbackTask: public Task {
    public:
        template <typename FnT, typename... Ts>
        CallbackTask(FnT&& function, Ts&&... args): func(std::forward<FnT>(function)), args(std::forward<Ts>(args)...) {}
        bool execute() override { return std::apply(func, args); }
    private:
        F func;
        std::tuple<Args...> args;
};

class ThreadPool {
    public:
        ThreadPool(const std::string& label, size_t workers, size_t max_jobs=0);
        template <typename F, typename... Args>
        void submitTask(F&& function, Args&&... args);
        void wait();
        void stop();
        bool stopped();
    private:
        void workerTarget();
        std::string m_label;
        size_t m_task_count{0};
        size_t m_done_count{0};
        std::atomic<bool> m_stop{false};
        std::mutex m_mutex;
        std::condition_variable m_done_condition;
        SyncQueue<Task*> m_task_queue;
        std::vector<std::thread> m_workers;
};

using namespace std::chrono_literals;

inline ThreadPool::ThreadPool(const std::string& label,
        size_t workers,
        size_t max_jobs):
    m_label(label),
    m_task_queue(SyncQueue<Task*>(max_jobs)) {
    for (size_t i = 0; i < workers; ++i) {
        m_workers.push_back(std::thread(&ThreadPool::workerTarget, this));
    }
}

inline void ThreadPool::workerTarget() {
    while (!m_stop.load()) {
        std::optional<Task*> opt = m_task_queue.pop(500ms);
        if (!opt.has_value()) {
            spdlog::debug("{0} worker get task timed out", m_label);
            continue;
        }
        Task* ptask = opt.value();
        if (ptask->execute()) {
            spdlog::debug("{0} worker executed task", m_label);
            while(!m_task_queue.push(ptask, 500ms)) {
                if (m_stop.load()) {
                    return;
                }
            }
        }
        else {
            spdlog::debug("{0} worker task finished", m_label);
            {
                std::lock_guard lock(m_mutex);
                m_done_count++;
                if (m_done_count == m_task_count) {
                    m_done_condition.notify_one();
                }
            }
        }
    }
    spdlog::debug("{0} worker exiting", m_label);
}

inline void ThreadPool::stop() {
    if (stopped()) {
        return;
    }
    m_stop.store(true);
    for (auto& worker: m_workers) {
        worker.join();
    }
    spdlog::debug("{0} stopped", m_label);
}

inline bool ThreadPool::stopped() {
    return std::none_of(m_workers.begin(), m_workers.end(), [](const std::thread& th){ return th.joinable(); });
}

inline void ThreadPool::wait() {
    if (m_task_count == 0) {
        return;
    }
    std::unique_lock lock(m_mutex);
    m_done_condition.wait(lock, [this]() { return m_done_count == m_task_count; });
    lock.unlock();
    stop();
}

template <typename F, typename... Args>
inline void ThreadPool::submitTask(F&& function, Args&&... args) {
    Task* pt = new CallbackTask<F, Args...>(std::forward<F>(function), std::forward<Args>(args)...);
    m_task_queue.push(pt);
    {
        std::lock_guard lock(m_mutex);
        m_task_count++;
    }
}

#endif
