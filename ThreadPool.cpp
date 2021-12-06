#include "ThreadPool.h"
#include <iostream>
#include <exception>
#include <strings.h>
#include <string>
#include <unistd.h>

ThreadPool::ThreadPool(int minSize, int maxSize) : 
    m_minSize(minSize), m_maxSize(maxSize),
    m_busyTaskNum(0), m_aliveTaskNum(minSize) {

    m_taskQueue = new TaskQueue;
    m_threadIDs = new pthread_t[maxSize];
    if(m_threadIDs == nullptr) {
        throw std::exception();
    }

    bzero(m_threadIDs, sizeof(m_threadIDs));
    if(pthread_mutex_init(&m_mutex, NULL) != 0 ||
        pthread_cond_init(&m_cond, NULL) != 0) {
        throw std::exception();
    }
    // 创建minSize个线程
    for(int i = 0; i < minSize; ++i) {
        // 这里有个技巧，把this当作参数传给worker，这样能够在worker里调用非静态成员
        if(pthread_create(&m_threadIDs[i], NULL, worker, this) != 0) {
            delete[] m_threadIDs;
            delete m_taskQueue;
            throw std::exception();
        }
        // 设置线程脱离
        if(pthread_detach(m_threadIDs[i]) != 0) {
            delete[] m_threadIDs;
            delete m_taskQueue;
            throw std::exception();
        }
        std::cout << "线程ID" << std::to_string(m_threadIDs[i]) << "已创建" << std::endl;
    }
    // 创建管理者线程
    if(pthread_create(&m_managerID, NULL, manager, this)) {
        delete[] m_threadIDs;
        delete m_taskQueue;
        throw std::exception();
    }
    // 设置线程脱离
    if(pthread_detach(m_managerID) != 0) {
        delete[] m_threadIDs;
        delete m_taskQueue;
        throw std::exception();
    }
}

ThreadPool::~ThreadPool() {
    // 析构前先把标志位置为true，防止还没析构完，其他线程又在操作线程池
    m_shutdown = true;
    // 把所有的活着的线程唤醒，不然线程池销毁后，这些线程还在阻塞
    // 这些线程被唤醒后，在执行work函数中会判断m_shutdown然后退出
    for(int i = 0; i < m_aliveTaskNum; ++i) {
        pthread_cond_signal(&m_cond);
    }
    if(m_taskQueue) delete m_taskQueue;
    if(m_threadIDs) delete[] m_threadIDs;
    
    pthread_cond_destroy(&m_cond);
    pthread_mutex_destroy(&m_mutex);
}

int ThreadPool::getBusyNum() {
    return m_busyTaskNum;
}

int ThreadPool::getAliveNum() {
    return m_aliveTaskNum;
}

void ThreadPool::addTask(Task task) {
    if(m_shutdown) return;
    // 不用加锁，任务队列里有锁
    m_taskQueue->pushTask(task);
    pthread_cond_signal(&m_cond);
}

void* ThreadPool::worker(void* arg) {
    // 传入的是this，强转一下
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    // 循环不停地工作，直到调用threadExit使其退出为止
    while(1) {
        std::cout << pool->m_taskQueue->taskNum() << std::endl;
        pthread_mutex_lock(&pool->m_mutex);
        // 这里用while处理虚假唤醒问题
        while(pool->m_taskQueue->taskNum() == 0 && 
            pool->m_shutdown == false) {
            // 队列为空时对条件变量上锁
            pthread_cond_wait(&pool->m_cond, &pool->m_mutex);

            if(pool->m_exitTaskNum > 0) {
                /*过了这个判断，说明这个线程被唤醒是让他自杀的，而不是有新任务产生
                  去看manager函数的实现就明白了*/
                pool->m_exitTaskNum--;
                if(pool->m_aliveTaskNum > pool->m_minSize) {
                    pool->m_aliveTaskNum--;
                    pthread_mutex_unlock(&pool->m_mutex);
                    pool->threadExit();
                }
            }
        }

        // 判断线程池是否已被关闭了
        if(pool->m_shutdown) {
            // 如果关闭了，所有工作线程都在这里过判断，集体自杀
            pthread_mutex_unlock(&pool->m_mutex);
            pool->threadExit();
        }

        // 从队列里取出一个任务，注意，takeTask()后该任务已经pop
        Task task = pool->m_taskQueue->takeTask();
        pool->m_busyTaskNum++;
        pthread_mutex_unlock(&pool->m_mutex);
        // 执行task的工作函数，并传入参数

        std::cout << "线程ID" << std::to_string(pthread_self()) << "开始工作" << std::endl;
        task.func(task.arg);
        // 注意：task的arg要定义在堆区
        delete(task.arg);
        task.arg = nullptr;

        // 执行到此处以后，表示任务执行完了
        pthread_mutex_lock(&pool->m_mutex);
        pool->m_busyTaskNum--;
        pthread_mutex_unlock(&pool->m_mutex);
    }
    return nullptr;
    }

void* ThreadPool::manager(void* arg) {
    ThreadPool* pool = static_cast<ThreadPool*>(arg);

    // 注意循环条件
    while(!pool->m_shutdown) {
        
        // 每隔5秒检测一次
        sleep(5);
        std::cout << "管理者线程正在检测..." << std::endl;

        pthread_mutex_lock(&pool->m_mutex);
        int aliveNum = pool->getAliveNum();      // 当前存活线程数
        int busyNum = pool->getBusyNum();        // 当前工作线程数量
        int taskNum = pool->m_taskQueue->taskNum();  // 当前任务数量
        pthread_mutex_unlock(&pool->m_mutex);

        /* 当前线程池过于繁忙的时候要往线程池里添加线程，策略如下：
        当前任务个数 > 存活的线程个数（即有任务在任务队列里等待）
        且 存活的线程数没有达到线程池设定的最大线程数*/

        //当条件满足时，要添加的线程数由该常量决定
        const int NUMBER = 2;
        
        if(aliveNum < taskNum && aliveNum < pool->m_maxSize) {
            pthread_mutex_lock(&pool->m_mutex);
            int count = 0;
            for(int i = 0; i < pool->m_maxSize && count < NUMBER &&
                aliveNum < pool->m_maxSize; ++i) {
                
                if(pool->m_threadIDs[i] == 0) {
                    pthread_create(&pool->m_threadIDs[i], NULL, worker, pool);
                    count++;
                    pool->m_aliveTaskNum++;
                    std::cout << "线程ID" << std::to_string(pthread_self()) << "已创建" << std::endl;
                }
            }
            pthread_mutex_unlock(&pool->m_mutex);
        } else if(busyNum*2 < aliveNum && aliveNum > pool->m_minSize) {
            /* 当前线程池过于空闲的时候要删除线程，策略如下：
                忙线程个数*2 < 存活的线程个数
                且存活的线程数多于线程池设定的最小线程数，需要注意的是
                此时肯定有线程在阻塞，因为忙线程 < 存活线程，我们要销毁的应该是
                阻塞的线程*/

            /* 注意这个地方的处理方式，并不是让manager线程去销毁线程，因为我们
               这里无法知道哪些线程正在运行worker，哪些正在阻塞，而我们需要销毁
               的是阻塞的线程，所以这里使用条件变量唤醒NUMBER个阻塞线程，然后让
               被唤醒的线程通过worker函数中对m_exitTaskNum的判断，让线程自杀*/
            pool->m_exitTaskNum = NUMBER;
            for(int i = 0; i < NUMBER; ++i) {
                pthread_cond_signal(&pool->m_cond);
            }
        }
    }
    std::cout << "管理者线程已结束" << std::endl;
    return nullptr;
}

void ThreadPool::threadExit() {
    pthread_t tid = pthread_self();
    for(int i = 0; i < m_maxSize; ++i) {
        if(m_threadIDs[i] == tid) {
            m_threadIDs[i] = 0;
            std::cout << "线程ID" << std::to_string(tid) << "已销毁" << std::endl;
            break;
        }
    }
    pthread_exit(NULL);
}