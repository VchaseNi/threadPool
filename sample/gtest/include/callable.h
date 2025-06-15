#ifndef CALLABLE_H
#define CALLABLE_H

#include <iostream>
extern uint32_t g_normalFuncCnt ;
extern uint32_t g_normalParamFuncCnt;

// 普通函数
uint32_t print_hello();

// 带参数的函数
void print_message_param(std::string arg1, int arg2);

// 仿函数
class Functor {
public:
    uint32_t functorCnt = 0;
    void operator()()
    {
        functorCnt++;
        std::cout << "Functor called cnt: " << functorCnt << std::endl;
    }
};

// 成员函数
class MyClass {
public:
    void member_func(const std::string &msg);

    static void static_func();

public:
    uint32_t memberFuncCnt = 0;
    static uint32_t staticFuncCnt;
};
#endif