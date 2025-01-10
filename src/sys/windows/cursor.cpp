#include "include/sys/windows/cursor.h"

#include <QScreen>
#include <string>

#include "windows.h"

QPoint GetCursorPosition()
{
    POINT P;
    if (!GetCursorPos(&P))
    {
        qDebug((std::to_string(GetLastError())).c_str());
    }
    return {int(P.x), int(P.y)};
}
