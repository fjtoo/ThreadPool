#ifndef TASKQUEUE_H
#define TASKQUEUE_H

#include <queue>
#include <pthread.h>

using callback = void(*)(void*);

// 任务类，即每个线程要执行的任务
struct Task
{
    Task() {
        func = nullptr;
        arg = nullptr;
    }
    Task(callback func, void* arg) {
        this->func = func;
        this->arg = arg;
    }
    callback func;    // 回调函数，该任务的执行函数
    void* arg;        // 回调函数的参数
};

// 任务队列，用于存储任务，线程将从任务队列中取出任务执行
class TaskQueue
{
private:
    std::queue<Task> m_queue;
    pthread_mutex_t m_mutex;
public:
    TaskQueue();
    ~TaskQueue();
    void pushTask(callback func, void* arg);  // 向队尾压入任务
    void pushTask(Task task);
    Task takeTask();                          // 取出并返回队头任务
    int taskNum();                            // 队列当前任务数
};

#endif


