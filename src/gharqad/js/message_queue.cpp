#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/js/message_queue.h>


MessageQueue * newMessageQueue(){
    return new MessageQueue();
}
