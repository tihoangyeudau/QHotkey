#include <QApplication>  // Use QApplication to test auto-detect
#include <QDebug>
#include <QTimer>
#include "QHotkey/qhotkey.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);  // This should NOT trigger warning

    // Hide from dock for console-like behavior
    app.setQuitOnLastWindowClosed(false);

    qDebug() << "=== QHotkey Auto-Detect Test (GUI) ===";
    qDebug() << "Using QApplication (should NOT show warning on macOS)";
    qDebug() << "Platform supported:" << QHotkey::isPlatformSupported();

    // Try to create hotkey - should work fine
    QHotkey hotkey(QKeySequence("F3"), true);

    if (hotkey.isRegistered()) {
        qDebug() << "✅ Hotkey registered:" << hotkey.shortcut();
        qDebug() << "Press F3 to test...";
    } else {
        qDebug() << "❌ Failed to register hotkey";
    }

    QObject::connect(&hotkey, &QHotkey::activated, [](){
        qDebug() << "🎯 *** F3 ACTIVATED! ***";
    });

    // Auto quit after 10 seconds
    QTimer::singleShot(10000, [&app](){
        qDebug() << "Auto-quitting...";
        app.quit();
    });

    qDebug() << "Running for 10 seconds. Press F3 to test...";
    return app.exec();
}