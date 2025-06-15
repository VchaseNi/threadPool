# ThreadPool 线程池

## 功能概述

vc::ThreadPool 是一个高性能的 C++17/20 线程池实现，支持优先级任务队列、任务暂停/恢复、任务状态监控等功能。该线程池设计用于高效管理并发任务执行，优化资源利用率。

## 功能特性

1. 任务提交方式
- detach()：提交无返回值的任务
- submit()：提交带返回值的任务，返回 std::future
- 支持普通函数、成员函数、lambda 表达式
- 支持带参数和返回值的任务

2. 优先级任务队列
- 通过编译选项 -DENABLE_PRIORITY_QUEUE=ON 启用
- 优先级数值越小优先级越高（0 最高）
- 默认优先级为 9（最低优先级）
- 未启用时使用普通 FIFO 队列

3. 线程池控制
- pause()：暂停/恢复任务执行
- stop()：停止所有任务并清空队列
- wait()：等待所有任务完成
- waitFor()：限时等待任务完成
- waitUntil()：等待到指定时间点

4. 状态监控
- runningTasks()：查询运行中任务数量
- waitingTasks()：查询等待中任务数量
- isPaused()：检查暂停状态

## 开发环境

- 操作系统：Linux
- C++标准：C++17（推荐 C++20）

## 代码结构

```
项目根目录/
├── syncLogger.h        // 同步打印辅助函数
├── threadPool.h        // 线程池
└── sample/       // 示例代码
    ├── gtest/    // 单元测试用例
    └── usecase/  // 使用示例
```
## 快速开始

1. 包含头文件：
```cpp
   #include "threadPool.h"
````

2. 参考 sample/usecase 目录中的示例代码实现线程池任务

3. 运行 sample/gtest 中的单元测试验证功能

4. 编译命令：

```bash
cmake -B build -DCMAKE_CXX_STANDARD=17 && cmake --build build
cmake -B build -DCMAKE_CXX_STANDARD=20 && cmake --build build
// -DENABLE_PRIORITY_QUEUE=ON/OFF 开启或关闭优先级队列
```
