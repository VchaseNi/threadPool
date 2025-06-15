#include "callable.h"
#include "Logger.h"

uint32_t g_normalFuncCnt = 0;
uint32_t g_normalParamFuncCnt = 0;
// 普通函数
uint32_t print_hello()
{
    Logger::println("Normal Func Cnt: ", g_normalFuncCnt);
    return ++g_normalFuncCnt;
}

// 普通函数
void print_message_param(std::string arg1, int arg2)
{
    g_normalParamFuncCnt++;
    Logger::println("Normal Func Cnt: ", g_normalParamFuncCnt, " arg1: ", arg1, " arg2: ", arg2);
}


// 成员函数
void MyClass::member_func(const std::string &msg)
{
    memberFuncCnt++;
    Logger::println("Member Func Cnt: ", memberFuncCnt, " msg: ", msg);
}

uint32_t MyClass::staticFuncCnt = 0;
// 静态成员函数
void MyClass::static_func()
{
    staticFuncCnt++;
    Logger::println("Static Func Cnt: ", staticFuncCnt);
}
