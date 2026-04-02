#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include "nekobox/js/message_queue.h"


MessageQueue * newMessageQueue(){
    return new MessageQueue();
}
