#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "TaskQueue.h"
#include <semaphore.h>

class ThreadPool
{
private:
    int m_minSize;           // 池中最小线程数
    int m_maxSize;           // 池中最大线程数
    TaskQueue* m_taskQueue;  // 任务队列
    int m_busyTaskNum;       // 正在执行任务的线程个数
    int m_aliveTaskNum;      // 线程池中的总线程个数
    int m_exitTaskNum;       // 需要销毁的线程个数
    
    pthread_t* m_threadIDs;  // 池中线程的线程号数组 
    pthread_t m_managerID;   // 管理者线程的线程号

    pthread_mutex_t m_mutex; // 互斥锁
    pthread_cond_t m_cond;   // 条件变量

    bool m_shutdown = false; // 标志位，判断是不是要销毁线程池, 销毁为1, 不销毁为0

private:
    /*注意 ：pthread_create的第三个参数，C++里必须是静态函数
      因为如果不是静态函数，在类被实例化化完成之前其成员函数
      是没有地址的，也就无法传入，而静态成员函数不属于一个具体
      对象，所以有地址*/
    static void* worker(void* arg);  // 工作的线程的任务函数
    static void* manager(void* arg); // 管理者线程的任务函数
    void threadExit();

public:
    ThreadPool(int minSize, int maxSize);
    ~ThreadPool();
    int getBusyNum();
    int getAliveNum();
    void addTask(Task task);  // 添加任务
};


#endif