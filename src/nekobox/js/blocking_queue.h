#ifndef BLOCKINGQUEUE_H
#define BLOCKINGQUEUE_H

// warning: do not include

#include <QQueue>
#include <QMutex>
#include <QWaitCondition>

template <typename T>
class BlockingQueue {
public:
    BlockingQueue();

    void Push(const T& item);
    void RequestShutdown();
    bool Pop(T &item);

private:
    QWaitCondition _cvCanPop;
    QMutex _sync;
    QQueue<T> _qu;
    bool _bShutdown = false;
};

#endif // BLOCKINGQUEUE_H
