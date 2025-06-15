#ifndef VC_THREAD_POOL_H
#define VC_THREAD_POOL_H

#include "syncLogger.h"
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

namespace vc {
using concurrency_t = std::invoke_result_t<decltype(std::thread::hardware_concurrency)>;
#define PAUSED_OR_TASK_FINISHED (m_isPaused || m_queue.empty())
class ThreadPool {
private:
    // 任务结构体封装
    struct Task {
        std::function<void()> func;
        int priority = DEFAULT_PRIORITY; // 默认优先级

        // 优先级队列比较函数
        bool operator<(const Task &other) const
        {
            return priority < other.priority; // 数值小的优先级高
        }
    };

#ifdef PRIORITY_QUEUE
    using TaskQueue = std::priority_queue<Task>;
#else
    using TaskQueue = std::queue<Task>;
#endif

public:
    explicit ThreadPool(concurrency_t cnt = std::thread::hardware_concurrency())
        : m_threadCnt(detectThreadCnt(cnt)), m_threads(std::make_unique<std::thread[]>(m_threadCnt))
    {
        startWorkerThreads();
    }

    ~ThreadPool()
    {
        stop();
        joinThreads();
    }

    // 禁止拷贝和移动
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

    /**
     * @brief 提交一个任务到线程池(默认最低优先级)，不返回future对象
     *
     * @tparam F 可调用对象
     * @tparam Args 可以调用对象参数
     * @param f [in] 可调用对象
     * @param args [in] 可以调用对象参数
     */
    template <typename F, typename... Args>
    void detach(F &&f, Args &&...args)
    {
        detach(DEFAULT_PRIORITY, std::forward<F>(f), std::forward<Args>(args)...);
    }

    /**
     * @brief 提交一个任务到线程池，不返回future对象
     *
     * @tparam F 可调用对象
     * @tparam Args 可以调用对象参数
     * @param priority [in] 优先级(越小优先级越高)
     * @param f [in] 可调用对象
     * @param args [in] 可以调用对象参数
     */
    template <typename F, typename... Args>
    void detach(int priority, F &&f, Args &&...args)
    {
        auto task = createTask(std::forward<F>(f), std::forward<Args>(args)...);
        addTaskToQueue(priority, std::move(task));
    }

    /**
     * @brief 提交一个任务到线程池，返回future对象
     *
     * @tparam F 可调用对象
     * @tparam Args 可以调用对象参数
     * @param f [in] 可调用对象
     * @param args [in] 可以调用对象参数
     */
    template <typename F, typename... Args, typename Ret = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
    std::future<Ret> submit(F &&f, Args &&...args)
    {
        return submit(DEFAULT_PRIORITY, std::forward<F>(f), std::forward<Args>(args)...);
    }

    /**
     * @brief 提交一个任务到线程池，返回future对象
     *
     * @tparam F 可调用对象
     * @tparam Args 可以调用对象参数
     * @param priority [in] 优先级(越小优先级越高)
     * @param f [in] 可调用对象
     * @param args [in] 可以调用对象参数
     */
    template <typename F, typename... Args, typename Ret = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
    std::future<Ret> submit(int priority, F &&f, Args &&...args)
    {
        auto [task, future] = createPromiseTask<Ret>(std::forward<F>(f), std::forward<Args>(args)...);
        addTaskToQueue(priority, std::move(task));
        return std::move(future);
    }

    /**
     * @brief 正在运行的任务数量
     *
     * @return size_t
     */
    size_t runningTasks() const
    {
        std::lock_guard lock(m_mutex);
        return m_taskRunningCnt;
    }

    /**
     * @brief 等待执行的任务数量
     *
     * @return size_t
     */
    size_t waitingTasks() const
    {
        std::lock_guard lock(m_mutex);
        return m_queue.size();
    }

    /**
     * @brief 停止所有任务(清空队列中所有任务，导致任务丢失),用于线程池不再使用时调用
     *
     */
    void stop()
    {
        std::lock_guard lock(m_mutex);
        m_isRunning = false;
        // 清空任务队列
        TaskQueue empty;
        std::swap(m_queue, empty);
        m_taskCreated.notify_all();
    }

    /**
     * @brief 暂停/恢复 任务
     *
     * @param value true：暂停任务，false：恢复任务
     */
    void pause(bool pause)
    {
        m_isPaused.store(pause);
        std::lock_guard lock(m_mutex);
        m_taskCreated.notify_all();
    }

    /**
     * @brief 暂停状态
     *
     * @return true
     * @return false
     */
    bool isPaused() const { return m_isPaused; }

    /**
     * @brief 等待线程池执行完毕所有任务
     *
     */
    void wait()
    {
        std::unique_lock lock(m_mutex);
        m_taskDone.wait(lock, [this] { return !m_isRunning || (PAUSED_OR_TASK_FINISHED && m_taskRunningCnt == 0); });
    }

    /**
     * @brief 在一定时间内，等待线程池执行完毕所有任务
     *
     * @tparam R
     * @tparam P
     * @param duration 时间段
     * @return true 时间内完成所有任务
     * @return false 时间内未完成所有任务
     */
    template <typename Rep, typename Period>
    bool waitFor(const std::chrono::duration<Rep, Period> &duration)
    {
        std::unique_lock lock(m_mutex);
        return m_taskDone.wait_for(lock, duration,
                                   [this] { return !m_isRunning || (PAUSED_OR_TASK_FINISHED && m_taskRunningCnt == 0); });
    }

    template <typename Clock, typename Duration>
    bool waitUntil(const std::chrono::time_point<Clock, Duration> &timeout)
    {
        std::unique_lock lock(m_mutex);
        return m_taskDone.wait_until(
            lock, timeout, [this] { return !m_isRunning || (PAUSED_OR_TASK_FINISHED && m_taskRunningCnt == 0); });
    }

    /**
     * @brief 正在运行的任务数量
     *
     * @return size_t
     */
    size_t runningCnt()
    {
        std::lock_guard<std::mutex> lck(m_mutex);
        return m_taskRunningCnt;
    }

    /**
     * @brief 等待执行的任务数量
     *
     * @return size_t
     */
    size_t waitedCnt()
    {
        std::lock_guard<std::mutex> lck(m_mutex);
        return m_queue.size();
    }

private:
    // 辅助函数：创建普通任务
    template <typename F, typename... Args>
    auto createTask(F &&f, Args &&...args)
    {
#if __cplusplus >= 202002L
        return [f = std::forward<F>(f), ... args = std::forward<Args>(args)]() mutable { std::invoke(f, args...); };
#elif __cplusplus >= 201703L
        return [f = std::bind(std::forward<F>(f), std::forward<Args>(args)...)]() mutable {f();};
#endif
    }

    // 辅助函数：创建带返回值的任务
    template <typename Ret, typename F, typename... Args>
    auto createPromiseTask(F &&f, Args &&...args)
    {
        auto promise = std::make_shared<std::promise<Ret>>();
        auto future = promise->get_future();
        return std::make_pair(
#if __cplusplus >= 202002L
            [f = std::forward<F>(f), ... args = std::forward<Args>(args), promise]() mutable{
                try {
                    if constexpr (std::is_void_v<Ret>) {
                        std::invoke(f, args...);
                        promise->set_value();
                    }
                    else {
                        promise->set_value(std::invoke(f, args...));
                    }
                }
#elif __cplusplus >= 201703L
            [f = std::bind(std::forward<F>(f), std::forward<Args>(args)...), promise]() mutable {
                try {
                    if constexpr (std::is_void_v<Ret>) {
                        f();    
                        promise->set_value();
                    }
                    else {
                        promise->set_value(f());
                    }
                }
#endif
                catch (...) {
                    promise->set_exception(std::current_exception());
                }
            },
            std::move(future));
    }

    /**
     * @brief 加入到待执行队列
     *
     * @param priority [in] task优先级
     * @param task [] task任务
     */
    void addTaskToQueue(int priority, std::function<void()> task)
    {
        std::lock_guard lock(m_mutex);
        m_queue.push({std::move(task), priority});
        m_taskCreated.notify_one();
    }

    /**
     * @brief 线程池构造时，创建线程
     *
     */
    void startWorkerThreads()
    {
        m_taskRunningCnt = m_threadCnt;
        for (concurrency_t i = 0; i < m_threadCnt; ++i) {
            m_threads[i] = std::thread([this] { worker(); });
        }
    }

    /**
     * @brief 线程池析构时，join等待线程结束
     *
     */
    void joinThreads()
    {
        for (concurrency_t i = 0; i < m_threadCnt; ++i) {
            if (m_threads[i].joinable()) {
                m_threads[i].join();
            }
        }
    }

    /**
     * @brief 线程池的线程函数
     *
     */
    void worker()
    {
        while (true) {
            Task task;

            {
                std::unique_lock lock(m_mutex);
                --m_taskRunningCnt;

                // 通知等待任务完成的线程
                if ((m_isPaused || m_queue.empty()) && m_taskRunningCnt == 0) {
                    m_taskDone.notify_all();
                }

                // 等待新任务或停止信号
                m_taskCreated.wait(lock, [this] { return !m_isRunning || !PAUSED_OR_TASK_FINISHED; });

                if (!m_isRunning)
                    break;

#ifdef PRIORITY_QUEUE
                task = std::move(m_queue.top());
#else
                task = std::move(m_queue.front());
#endif
                m_queue.pop();
                ++m_taskRunningCnt;
            }

            // 执行任务
            task.func();
        }
    }

    /**
     * @brief 检查线程数是否合理，最大值为CPU线程数
     *
     * @param cnt 期望线程数
     * @return concurrency_t 实际线程数
     */
    concurrency_t detectThreadCnt(concurrency_t cnt) const
    {
        const auto max_threads = std::thread::hardware_concurrency();
        if (cnt == 0)
            return 1;
        return cnt > max_threads ? max_threads : cnt;
    }

private:
    // 同步原语
    mutable std::mutex m_mutex;
    std::condition_variable m_taskCreated; // 新任务被创建的信号
    std::condition_variable m_taskDone;    // 任务完成的信号

    // 线程管理
    concurrency_t m_threadCnt;
    std::unique_ptr<std::thread[]> m_threads;

    // 任务管理
    TaskQueue m_queue;           // 任务队列
    size_t m_taskRunningCnt = 0; // 正在执行的task数量

    // 状态标志
    std::atomic<bool> m_isRunning{true};       // 线程池运行与否
    std::atomic<bool> m_isPaused{false};       // 是否暂停
    static constexpr int DEFAULT_PRIORITY = 9; // 默认优先级，最低9
};

} // namespace vc

#endif // VC_THREAD_POOL_H