#include "syncLogger.h"
#include "threadPool.h"

bool normalFunc(std::string &str, int a) {
    vc::println(__func__, " ",str, " ", a);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    return true;
}

void highFunc(bool value) {
    vc::println(__func__, std::boolalpha, value);
}

int main() {
    vc::ThreadPool pool(2);

    // 添加4个优先级低的任务
    std::string str("Hello World");
    for (int i = 0; i < 4; i++) {
        pool.detach(normalFunc, str, i);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    pool.pause(true);
    vc::println("wait: ", pool.waitingTasks(), " running: ", pool.runningTasks());
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    // 这里可以看到，两个task一直在等待
    vc::println("wait: ", pool.waitingTasks(), " running: ", pool.runningTasks());

    pool.pause(false);

    // 如果在支持优先级队列时，highFunc会优先执行
    auto fut = pool.submit(0, highFunc, true);
    fut.wait();

    vc::println("wait: ", pool.waitingTasks(), " running: ", pool.runningTasks());
    pool.wait();

    return 0;
}