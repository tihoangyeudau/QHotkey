# QHotkey Auto-Detect Feature

## Overview
QHotkey now includes automatic application type detection for macOS to prevent common issues with CGEventTap.

## Problem
On macOS, QHotkey uses `CGEventTap` for global hotkey detection, which requires a GUI event loop to function properly. Using `QCoreApplication` instead of `QApplication` causes hotkeys to not work.

## Solution
QHotkey now automatically detects the application type and provides clear warnings when an incompatible application type is used.

## Behavior

### ✅ Compatible Applications (macOS)
```cpp
#include <QApplication>  // ✅ Works
// or
#include <QGuiApplication>  // ✅ Works

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);  // GUI event loop available

    QHotkey hotkey(QKeySequence("Ctrl+Shift+H"), true);
    // Hotkey will work properly

    return app.exec();
}
```

**Output:**
```
QHotkey: GUI application detected - CGEventTap should work properly
QHotkey: Setting up CGEventTap...
QHotkey: Event tap created successfully
✅ Hotkey registered: QKeySequence("Ctrl+Shift+H")
```

### ❌ Incompatible Applications (macOS)
```cpp
#include <QCoreApplication>  // ❌ Won't work on macOS

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);  // No GUI event loop

    QHotkey hotkey(QKeySequence("Ctrl+Shift+H"), true);
    // Hotkey registration will fail

    return app.exec();
}
```

**Output:**
```
QHotkey: macOS requires QApplication or QGuiApplication for global hotkeys!
QHotkey: CGEventTap needs GUI event loop to function properly.
QHotkey: Please change QCoreApplication to QApplication in your main.cpp
QHotkey: Example:
QHotkey:   QApplication app(argc, argv);  // Instead of QCoreApplication
QHotkey: Cannot setup event tap - wrong application type
❌ Failed to register hotkey
```

## Platform Differences

| Platform | QCoreApplication | QApplication | QGuiApplication |
|----------|------------------|--------------|-----------------|
| **Windows** | ✅ Works | ✅ Works | ✅ Works |
| **Linux** | ✅ Works | ✅ Works | ✅ Works |
| **macOS** | ❌ Fails | ✅ Works | ✅ Works |

## Testing

### Test 1: QCoreApplication (should show warning)
```bash
cd build_autodetect && ./test_autodetect
```

### Test 2: QApplication (should work fine)
```bash
cd build_autodetect && ./test_autodetect_gui
```

## Migration Guide

If you're using `QCoreApplication` on macOS and experiencing hotkey issues:

### Before (doesn't work on macOS):
```cpp
#include <QCoreApplication>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    // ... rest of code
}
```

### After (works on all platforms):
```cpp
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Optional: Hide from dock for console-like behavior
    app.setQuitOnLastWindowClosed(false);

    // ... rest of code
}
```

## Technical Details

The auto-detect feature:
1. Checks if `QGuiApplication` instance exists using `qobject_cast`
2. If not found, displays detailed warning messages
3. Prevents CGEventTap setup to avoid silent failures
4. Only affects macOS - other platforms work with any application type

This ensures developers get clear feedback about platform requirements instead of silent failures.