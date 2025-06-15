#ifndef __VC_THREAD_POOL_H__
#define __VC_THREAD_POOL_H__
#include "syncLogger.h"
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
namespace vc {
enum class PriorityLevel {
    HIGH = 0,
    LOW = 9,
};
#ifdef PRIORITY_QUEUE
struct priorityTask {
    std::function<void()> task;
    int priority;

    bool operator<(const priorityTask &other) const
    {
        return priority < other.priority; // Higher priority tasks come first
    }

    priorityTask(std::function<void()> &t, int p) : task(t), priority(p) {}
    priorityTask(std::function<void()> &&t, int p) : task(std::move(t)), priority(p) {}
};
#endif
using concurrency_t = std::invoke_result_t<decltype(std::thread::hardware_concurrency)>;
#define PAUSED_OR_TASK_FINISHED (m_isPaused || m_queue.empty())
class ThreadPool {

#ifdef PRIORITY_QUEUE
    using vcQueue = std::priority_queue<priorityTask>;
    #define PRIORITY_PARAM int priority,
    #define PRIORITY_ARG , priority
    #define PRIORITY_DEFAULT_VALUE 9

#else
    using vcQueue = std::queue<std::function<void()>>;
#define PRIORITY_PARAM
#define PRIORITY_ARG
#endif
public:
    explicit ThreadPool(concurrency_t cnt = std::thread::hardware_concurrency())
        : m_threadCnt(detectThreadCnt(cnt)), m_threads(std::make_unique<std::thread[]>(m_threadCnt))
    {
        m_taskRunningCnt = m_threadCnt;
        for (concurrency_t i = 0; i < m_threadCnt; i++) {
            m_threads[i] = std::thread(&ThreadPool::worker, this, i);
        }
    }

    ~ThreadPool()
    {
        stop();
        for (concurrency_t i = 0; i < m_threadCnt; i++) {
            if (m_threads[i].joinable()) {
                m_threads[i].join();
            }
        }
    }

    void worker(concurrency_t index)
    {
        while (true) {
            std::unique_lock<std::mutex> lck(m_tasksMutex);
            m_taskRunningCnt--;
            // 1. 等待线程池完成任务；2. 当前无task在运行; 3. 队列为空或暂停。通知wait相关函数
            if (m_isWaitting && m_taskRunningCnt == 0 && PAUSED_OR_TASK_FINISHED) {
                m_taskDone.notify_all();
            }
            m_taskCreated.wait(lck, [this] { return !PAUSED_OR_TASK_FINISHED || !m_isRunning; });
            // 如果线程池已停止运行，退出循环
            if (!m_isRunning) {
                break;
            }

            m_taskRunningCnt++;
#ifdef PRIORITY_QUEUE
            auto task = std::move(std::remove_const_t<priorityTask &>(m_queue.top()).task);
#else
            auto task = std::move(m_queue.front());
#endif
            m_queue.pop();
            lck.unlock();

            if (task)
                task();
        }
    }
    /**
     * @brief 提交一个任务到线程池，并返回一个future对象
     *
     * @tparam F 可调用对象
     * @tparam Args 可以调用对象参数
     * @tparam Ret 返回值类型
     * @param f [in] 可调用对象
     * @param args [in] 可以调用对象参数
     * @return std::future<Ret> 返回值
     */
    template <typename F, typename... Args, typename Ret = std::invoke_result_t<F, Args...>>
    std::future<Ret> submit(PRIORITY_PARAM F &&f, Args &&...args)
    {
        auto promise = std::make_shared<std::promise<Ret>>();
        
        std::unique_lock lck(m_tasksMutex);
        m_queue.emplace([f = std::forward<F>(f), ... args = std::forward<Args>(args), promise]() {
            if constexpr (std::is_void_v<Ret>) {
                std::invoke(f, args...);
                promise->set_value();
            }
            else {
                promise->set_value(std::invoke(f, args...));
            }
        } PRIORITY_ARG);
        m_taskCreated.notify_one();
        return promise->get_future();
    }

    /**
     * @brief 提交一个任务到线程池，不返回future对象
     *
     * @tparam F 可调用对象
     * @tparam Args 可以调用对象参数
     * @param f [in] 可调用对象
     * @param args [in] 可以调用对象参数
     */
    template <typename F, typename... Args>
    void detach(PRIORITY_PARAM F &&f, Args &&...args)
    {
        std::unique_lock lck(m_tasksMutex);
        m_queue.emplace(
            [f = std::forward<F>(f), ... args = std::forward<Args>(args)]() { std::invoke(f, args...); } PRIORITY_ARG);
        m_taskCreated.notify_one();
    }
#ifdef PRIORITY_QUEUE
    template <typename F, typename... Args>
    void detach(F &&f, Args &&...args)
    {
        detach(PRIORITY_DEFAULT_VALUE, std::forward<F>(f), std::forward<Args>(args)...);
    }

    template <typename F, typename... Args, typename Ret = std::invoke_result_t<F, Args...>>
    std::future<Ret> submit(F &&f, Args &&...args)
    {
        return submit(PRIORITY_DEFAULT_VALUE, std::forward<F>(f), std::forward<Args>(args)...);
    }
#endif // DEBUG
    /**
     * @brief 正在运行的任务数量
     *
     * @return size_t
     */
    size_t runningCnt()
    {
        std::lock_guard<std::mutex> lck(m_tasksMutex);
        return m_taskRunningCnt;
    }

    /**
     * @brief 等待执行的任务数量
     *
     * @return size_t
     */
    size_t waitedCnt()
    {
        std::lock_guard<std::mutex> lck(m_tasksMutex);
        return m_queue.size();
    }

    /**
     * @brief 停止所有任务(清空队列中所有任务，导致任务丢失),用于线程池不再使用时调用
     *
     */
    void stop()
    {
        // 清空队列
        std::lock_guard<std::mutex> lck(m_tasksMutex);
        decltype(m_queue) emptyQueue;
        m_queue.swap(emptyQueue);

        // 停止所有线程
        m_isRunning = false;
        m_taskCreated.notify_all();
    }

    /**
     * @brief 暂停/恢复 任务
     *
     * @param value true：暂停任务，false：恢复任务
     */
    void pause(bool value)
    {
        m_isPaused.store(value);
        std::unique_lock lck(m_tasksMutex);
        m_taskCreated.notify_all();
    }

    bool isPause() { return m_isPaused.load(); }

    /**
     * @brief 等待线程池执行完毕所有任务
     *
     */
    void wait()
    {
        std::unique_lock<std::mutex> lck(m_tasksMutex);
        m_isWaitting = true;
        m_taskDone.wait(lck, [this] { return !m_isRunning || PAUSED_OR_TASK_FINISHED; });
        m_isWaitting = false;
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
    template <typename R, typename P>
    bool wait_for(const std::chrono::duration<R, P> &duration)
    {
        std::unique_lock<std::mutex> lck(m_tasksMutex);
        m_isWaitting = true;
        bool result = m_taskDone.wait_for(lck, duration, [this] { return !m_isRunning || PAUSED_OR_TASK_FINISHED; });
        m_isWaitting = false;
        return result;
    }

    /**
     * @brief 在指定时间点内，等待线程池执行完毕所有任务
     *
     * @tparam C
     * @tparam D
     * @param timeout_time 时间
     * @return true 时间内完成所有任务
     * @return false 时间内未完成所有任务
     */
    template <typename C, typename D>
    bool wait_until(const std::chrono::time_point<C, D> &timeout_time)
    {
        std::unique_lock<std::mutex> lck(m_tasksMutex);
        m_isWaitting = true;
        bool result = m_taskDone.wait_until(lck, timeout_time, [this] { return !m_isRunning || PAUSED_OR_TASK_FINISHED; });
        m_isWaitting = false;
        return result;
    }

private:
    /**
     * @brief 检查线程数是否合理，最大值为CPU线程数
     *
     * @param cnt 期望线程数
     * @return concurrency_t 实际线程数
     */
    concurrency_t detectThreadCnt(concurrency_t cnt)
    {
        if (cnt > std::thread::hardware_concurrency()) {
            return std::thread::hardware_concurrency();
        }
        else if (cnt <= 0) {
            return 1;
        }

        return cnt;
    }

private:
    bool m_isRunning = true;                            // 线程池是否正在运行
    std::atomic<bool> m_isPaused = false;               // 线程池是否暂停
    bool m_isWaitting = false;                          // 是否等待任务完成
    concurrency_t m_threadCnt;                          // 线程池数量
    size_t m_taskRunningCnt = 0;                        // 正在运行的任务数量
    std::unique_ptr<std::thread[]> m_threads = nullptr; // 线程池
    mutable std::mutex m_tasksMutex = {};               // 任务队列互斥锁
    vcQueue m_queue;                                    // 任务队列
    std::condition_variable m_taskCreated = {};         // 新任务被创建
    std::condition_variable m_taskDone = {};            // 任务完成
};

}; // namespace vc

#endif