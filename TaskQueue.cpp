#include <TaskQueue.h>

TaskQueue::TaskQueue() {
    pthread_mutex_init(&m_mutex, NULL);
}

TaskQueue::~TaskQueue() {
    pthread_mutex_destroy(&m_mutex);
}

void TaskQueue::pushTask(callback func, void* arg) {
    Task task(func, arg);
    pthread_mutex_lock(&m_mutex);
    m_queue.push(task);
    pthread_mutex_unlock(&m_mutex);
}

Task TaskQueue::takeTask() {
    Task temp;
    pthread_mutex_lock(&m_mutex);
    if(m_queue.size() > 0) {
        temp = m_queue.front();
        m_queue.pop();
    }
    pthread_mutex_unlock(&m_mutex);
    return temp;
}

int TaskQueue::taskNum() {
    return m_queue.size();
}