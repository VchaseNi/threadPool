
假设cpu线程数为N

pool.submit 用于将任务提交到队列 返回future
pool.detach 用于将任务提交到队列 返回void，无法得知任务是否执行完(不关心何时结束)

通过将循环拆分为块并将其作为独立任务提交到队列中来实现循环的并行化：
用M次调用举例
将M次调用拆分为N个任务，每个任务包含M/N放到队列中执行，每个线程执行M/N个调用
1. pool.submit_loop 将任务（start， end）同时N个线程执行， 返回multi_future（multi_future也支持get，返回vector<T> 每个子任务的结果）
2. pool.detach_loop 将任务start， end）分为多个指定个数子任务，子任务并行执行， 返回void 无法得知任务是否执行完(不关心何时结束)

将M次调用拆分为N个任务，每个任务包含M/N放到队列中执行，每个线程自己控制循环，可减少函数调用
3. pool.submit_blocks 自己控制循环
4. pool.detach_blocks 自己控制循环

将每个单独的索引作为独立任务提交到池的队列中，适用于小范围（M较小）：
5. pool.submit_sequeue 
6. pool.detach_sequeue  


pool 支持以下三种方式等待线程池中所有任务完成
    wait(): 一直等待
    wait_for(): 等待时间
    wait_until(): 等待某一时刻

get_tasks_queued() gets the number of tasks currently waiting in the queue to be executed by the threads.
get_tasks_running() gets the number of tasks currently being executed by the threads.
get_tasks_total() gets the total number of unfinished tasks: either still in the queue, or running in a thread.
Note that get_tasks_total() == get_tasks_queued() + get_tasks_running().

4. 编译命令：
```bash
cmake -B build -DCMAKE_CXX_STANDARD=17 && cmake --build build
cmake -B build -DCMAKE_CXX_STANDARD=20 && cmake --build build

```