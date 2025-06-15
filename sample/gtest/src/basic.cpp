#include "syncLogger.h"
#include "threadPool.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <mutex>
#include <numeric>
#include "Logger.h"

using namespace vc;

    void longTask(int id)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        Logger::println("Long task ", id, " completed.");
    }

std::string getStr(std::string &str) { return str + "." + str; }
/// @brief 测试wait函数
TEST(basic, wait)
{
    ThreadPool pool(4);
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 10; ++i) {
        pool.detach(longTask, i);
    }

    pool.wait();                    // Wait for all tasks to complete
    ASSERT_EQ(pool.waitedCnt(), 0); // No tasks should be left in the queue
    ASSERT_EQ(pool.runningCnt(), 0); 
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    Logger::println("All tasks completed in ", duration.count(), " ms.");
}

/// @brief 测试waitFor函数
TEST(basic, waitFor)
{
    ThreadPool pool(4);
    for (int i = 0; i < 10; ++i) {
        pool.detach(longTask, i);
    }

    bool result = pool.waitFor(std::chrono::milliseconds(500));
    ASSERT_FALSE(result);
    ASSERT_EQ(pool.waitedCnt(), 6);
    ASSERT_EQ(pool.runningCnt(), 4);

    result = pool.waitFor(std::chrono::milliseconds(3010));
    ASSERT_TRUE(result);
    ASSERT_EQ(pool.waitedCnt(), 0);
    ASSERT_EQ(pool.runningCnt(), 0);
}

/// @brief 测试waitUntil函数
TEST(basic, waitUntil)
{
    ThreadPool pool(4);
    for (int i = 0; i < 10; ++i) {
        pool.detach(longTask, i);
    }

    auto timePoint = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    ASSERT_FALSE(pool.waitUntil(timePoint));

    bool result = pool.waitUntil(timePoint + std::chrono::milliseconds(2505));
    ASSERT_TRUE(result);
    ASSERT_EQ(pool.waitedCnt(), 0);
    ASSERT_EQ(pool.runningCnt(), 0);

    std::string str = "waitUntil";
    auto fut = pool.submit(getStr, std::ref(str));
    ASSERT_EQ(fut.get(), str + "." + str);
}

TEST(basic, count)
{
    ThreadPool pool(4);
    for (int i = 0; i < 10; ++i) {
        pool.detach(longTask, i);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(pool.waitedCnt(), 6);  // All tasks should be detached and running
    ASSERT_EQ(pool.runningCnt(), 4); // 4 threads should be running tasks

    pool.wait();                    // Wait for all tasks to complete
    ASSERT_EQ(pool.waitedCnt(), 0); // No tasks should be left in the queue
}

TEST(basic, stop) {
    ThreadPool pool(2);
    for (int i = 0; i < 10; ++i) {
        pool.detach(longTask, i);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    ASSERT_EQ(pool.waitedCnt(), 8); // All tasks should be stopped
    ASSERT_EQ(pool.runningCnt(), 2);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pool.stop();
    ASSERT_EQ(pool.waitedCnt(), 0); // All tasks should be stopped
    ASSERT_EQ(pool.runningCnt(), 2);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    ASSERT_EQ(pool.waitedCnt(), 0); // All tasks should be stopped
    ASSERT_EQ(pool.runningCnt(), 0);
}

TEST(basic, pause) {
    ThreadPool pool(2);
    for (int i = 0; i < 10; ++i) {
        pool.detach(longTask, i);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pool.pause(true); // Pause the thread pool

    ASSERT_TRUE(pool.isPaused()); // All tasks should be paused
    ASSERT_EQ(pool.waitedCnt(), 8); // All tasks should be in the queue
    ASSERT_EQ(pool.runningCnt(), 2); // 2 threads should be running tasks
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    ASSERT_TRUE(pool.isPaused()); // All tasks should be paused
    ASSERT_EQ(pool.waitedCnt(), 8); // All tasks should be in the queue
    ASSERT_EQ(pool.runningCnt(), 0); // 2 threads should be running tasks
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    ASSERT_EQ(pool.waitedCnt(), 8); // All tasks should be in the queue

    pool.pause(false); // Resume the thread pool

    pool.wait();                    // Wait for all tasks to complete
    ASSERT_EQ(pool.waitedCnt(), 0); // No tasks should be left in the queue
    ASSERT_EQ(pool.runningCnt(), 0); 
}

TEST(basic, priority) {
    #ifdef PRIORITY_QUEUE
    ThreadPool pool(2);
    for (int i = 0; i < 10; ++i) {
        pool.detach(10 - i, longTask, i);
    }   

    pool.wait();

    for (int i = 0; i < 10; i++) {
        pool.submit(10 - i, longTask, i);
    }
    pool.wait();
    #endif
}