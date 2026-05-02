#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include <QKeySequenceEdit>

class QtExtKeySequenceEdit : public QKeySequenceEdit {
public:
    QtExtKeySequenceEdit(QWidget *parent);

    ~QtExtKeySequenceEdit();

protected:
    virtual void keyPressEvent(QKeyEvent *pEvent);
};
