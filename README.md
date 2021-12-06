# ThreadPool

C++ 实现的线程池。

线程池中的线程分为多个普通工作线程和一个管理者线程。

普通线程负责执行任务，不断地从任务队列中取出任务并执行任务的回调函数。

管理者线程每隔一段时间检测线程池中的线程繁忙程度，如果线程池中过半的线程都处于无任务执行的阻塞状态，就销毁掉部分线程；如果线程池中所有的线程都在执行任务，就创建出更多的线程加入线程池。

代码中的几点不好理解的细节如下：

1. 线程池的构造函数中通过pthread_create()创建出的线程，都执行一个同一个成员函数worker，这个函数必须设置为静态函数，因为pthread_create()的第三个参数要传函数地址，如果定义成非静态的，在该类实例化之前其成员函数是没有地址的，但是定义成类属的静态函数就解决了这一问题。

2. 但是1中这样带来一个问题：静态函数只能调用静态成员，为了在worker函数中调用类的非静态成员，我们把this指针作为参数作为pthread_create()的第四个参数给worker()传入，这样就可以通过this指针来调用非静态成员。

3. ThreadPool中私有成员m_shutdown的作用是：标志整个线程池是否已经被销毁。m_shutdown的初值为false，在ThreadPool的析构函数中将m_shutdown赋值为false，同时，worker函数中对m_shutdown进行了判断，一旦m_shutdown == true，就让线程结束，这样避免了线程池被销毁后其中的线程仍然在不断运行。
