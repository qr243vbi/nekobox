#ifndef MESSAGE_QUEUE
#define MESSAGE_QUEUE
#include "blocking_queue.h"
#include <QString>
class MessagePart{
public:
    QString message;
    QString title;
    uint type;
};


template <typename T>
BlockingQueue<T>::BlockingQueue() {}

template <typename T>
void BlockingQueue<T>::Push(const T& item) {
    {
        QMutexLocker locker(&_sync);
        _qu.enqueue(item);
    }
    _cvCanPop.wakeOne(); // Notify one waiting thread
}

template <typename T>
void BlockingQueue<T>::RequestShutdown() {
    {
        QMutexLocker locker(&_sync);
        _bShutdown = true;
    }
    _cvCanPop.wakeAll(); // Notify all waiting threads
}

template <typename T>
bool BlockingQueue<T>::Pop(T &item) {
    QMutexLocker locker(&_sync);
    for (;;) {
        if (_qu.isEmpty()) {
            if (_bShutdown) {
                return false; // Shutdown requested
            }
        } else {
            break; // Queue is not empty
        }
        _cvCanPop.wait(&_sync); // Wait until notified
    }
    item = std::move(_qu.dequeue());
    return true;
}

class MessageQueue: public BlockingQueue<MessagePart>{

};

MessageQueue * newMessageQueue();

#endif
