#include <nekobox/ui/utils/QtExtKeySequenceEdit.h>
#include <QAction>
#include <QApplication>
#include <QFocusEvent>
#include <QSignalSpy>
#include <QtTest>

class KeySequenceEditTest : public QObject {
    Q_OBJECT
private:
    static void finishEditing(QtExtKeySequenceEdit &editor) {
        QFocusEvent event(QEvent::FocusOut);
        QApplication::sendEvent(&editor, &event);
    }
private slots:
    void recordsKeys_data() {
        QTest::addColumn<QString>("initial");
        QTest::addColumn<int>("key");
        QTest::addColumn<int>("modifiers");
        QTest::addColumn<QKeySequence>("expected");
        const QList<Qt::KeyboardModifiers> allModifiers = {
            Qt::NoModifier, Qt::ControlModifier, Qt::AltModifier, Qt::ShiftModifier,
            Qt::ControlModifier | Qt::ShiftModifier, Qt::MetaModifier
        };
        for (const auto &initial : {QString(), QString("Ctrl+K")}) {
            for (auto key : {Qt::Key_Backspace, Qt::Key_Delete, Qt::Key_K}) {
                for (auto modifiers : allModifiers) {
                    const QKeySequence combination(QKeyCombination(modifiers, key));
                    const auto expected = key == Qt::Key_Backspace && modifiers == Qt::NoModifier
                        ? QKeySequence() : combination;
                    const auto name = initial + ":" + combination.toString(QKeySequence::PortableText);
                    QTest::newRow(qPrintable(name)) << initial << int(key) << int(modifiers) << expected;
                }
            }
        }
    }
    void recordsKeys() {
        QFETCH(QString, initial);
        QFETCH(int, key);
        QFETCH(int, modifiers);
        QFETCH(QKeySequence, expected);
        QtExtKeySequenceEdit editor(nullptr);
        editor.setKeySequence(QKeySequence(initial));
        editor.show();
        editor.setFocus();
        QApplication::processEvents();
        QTest::keyClick(&editor, Qt::Key(key), Qt::KeyboardModifiers(modifiers));
        finishEditing(editor);
        QCOMPARE(editor.keySequence(), expected);
        QAction action;
        action.setShortcut(QKeySequence(editor.keySequence().toString(QKeySequence::PortableText), QKeySequence::PortableText));
        QCOMPARE(action.shortcut(), expected);
    }
    void retainsModifiedBackspaceAsFirstChord() {
        QtExtKeySequenceEdit editor(nullptr);
        QTest::keyClick(&editor, Qt::Key_Backspace, Qt::ControlModifier);
        QTest::keyClick(&editor, Qt::Key_K, Qt::ControlModifier);
        finishEditing(editor);
        QCOMPARE(editor.keySequence(), QKeySequence("Ctrl+Backspace, Ctrl+K"));
    }
    void bareBackspaceStillClears() {
        QtExtKeySequenceEdit editor(nullptr);
        editor.setKeySequence(QKeySequence("Ctrl+Backspace"));
        QTest::keyClick(&editor, Qt::Key_Backspace);
        finishEditing(editor);
        QVERIFY(editor.keySequence().isEmpty());
    }
    void recordingTimerRetainsModifiedBackspace() {
        QtExtKeySequenceEdit editor(nullptr);
        QSignalSpy finished(&editor, &QKeySequenceEdit::editingFinished);
        QTest::keyClick(&editor, Qt::Key_Backspace, Qt::ControlModifier);
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 2500);
        QCOMPARE(editor.keySequence(), QKeySequence("Ctrl+Backspace"));
    }
};

QTEST_MAIN(KeySequenceEditTest)
#include "tst_keysequenceedit.moc"
