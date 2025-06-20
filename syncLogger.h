#ifndef __VC_SYNC_LOGGER_H__
#define __VC_SYNC_LOGGER_H__
#include <iostream>
#include <mutex>

namespace vc {
inline std::mutex m_mutex;
#define DEFAULT_STREAM std::cout
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
    (DEFAULT_STREAM << ... << std::forward<T>(items));
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

};
#endif