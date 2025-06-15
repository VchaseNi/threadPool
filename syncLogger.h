#ifndef __VC_SYNC_LOGGER_H__
#define __VC_SYNC_LOGGER_H__
#include <iostream>
#include <mutex>

namespace vc {
/**
 * @brief 用于多线程输出日志
 *
 */
class SyncLogger {
public:
    /**
     * @brief 构造
     *
     * @param stream
     */
    explicit SyncLogger(std::ostream &stream = std::cout) : out_stream(stream) {}

    SyncLogger(const SyncLogger &) = delete;
    SyncLogger(SyncLogger &&) = delete;
    SyncLogger &operator=(const SyncLogger &) = delete;
    SyncLogger &operator=(SyncLogger &&) = delete;

    /**
     * @brief print可变参
     *
     * @tparam T
     * @param items 可变参数
     */
    template <typename... T>
    void print(T &&...items)
    {
        const std::lock_guard stream_lock(m_mutex);
        (out_stream << ... << std::forward<T>(items));
    }

    /**
     * @brief print可变参，末尾添加换行符
     *
     * @tparam T
     * @param items 可变参数
     */
    template <typename... T>
    void println(T &&...items)
    {
        print(std::forward<T>(items)..., '\n');
    }

private:
    /**
     * @brief The output stream to print to.
     */
    std::ostream &out_stream;

    /**
     * @brief A mutex to synchronize printing.
     */
    mutable std::mutex m_mutex = {};
}; // class SyncLogger
};

#endif