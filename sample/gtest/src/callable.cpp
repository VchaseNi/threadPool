#include "syncLogger.h"
#include "threadPool.h"
#include <gtest/gtest.h>

uint32_t g_normalFuncCnt = 0;
uint32_t g_normalParamFuncCnt = 0;
// 普通函数
uint32_t print_hello()
{
    vc::println("Normal Func Cnt: ", g_normalFuncCnt);
    return ++g_normalFuncCnt;
}

// 普通函数
void print_message_param(std::string arg1, int arg2)
{
    g_normalParamFuncCnt++;
    vc::println("Normal Func Cnt: ", g_normalParamFuncCnt, " arg1: ", arg1, " arg2: ", arg2);
}

class MyClass {
public:
    void member_func(const std::string &msg)
    {
        memberFuncCnt++;
        vc::println("Member Func Cnt: ", memberFuncCnt, " msg: ", msg);
    }

    static void static_func()
    {
        staticFuncCnt++;
        vc::println("Static Func Cnt: ", staticFuncCnt);
    }

public:
    uint32_t memberFuncCnt = 0;
    static uint32_t staticFuncCnt;
};
using namespace vc;
TEST(callable, print_hello)
{
    ThreadPool pool(4);
    for (int i = 0; i < 10; ++i) {
        pool.detach(print_hello);
    }

    pool.wait();
    ASSERT_EQ(g_normalFuncCnt, 10);
}

TEST(callable, print_hello_submit)
{
    ThreadPool pool(4);

    g_normalFuncCnt = 0;
    ASSERT_EQ(pool.submit(print_hello).get(), 1);
}

TEST(callable, print_message_param)
{
    ThreadPool pool(4);
    pool.submit(print_message_param, "Hello", 2);
    pool.wait();
}

TEST(callable, memberFunc)
{
    MyClass obj;
    ThreadPool pool(4);
    std::string str("Hello");
    for (int i = 0; i < 10; ++i) {
        pool.detach(&MyClass::member_func, &obj, str);
    }
    pool.wait();
    ASSERT_EQ(obj.memberFuncCnt, 10);
}

TEST(callable, lambda)
{
    ThreadPool pool(4);
    uint32_t lambdaCnt = 0;
    for (int i = 0; i < 10; ++i) {
        pool.detach([&]() { lambdaCnt++; });
    }
    pool.wait();
    ASSERT_EQ(lambdaCnt, 10);
    std::string str("lambda");
    pool.submit([](std::string &str) { vc::println(str); }, std::ref(str));
}