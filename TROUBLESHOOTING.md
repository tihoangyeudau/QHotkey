# QHotkey CGEventTap Troubleshooting Guide

## 🔧 Common Issues & Solutions

### 1. ❌ Simple Test không nhận hotkey (Ctrl+Shift+H)

**Triệu chứng:**
- Console hiển thị "Hotkey registered successfully"
- Nhưng khi ấn Ctrl+Shift+H không có phản hồi
- Không thấy "*** HOTKEY ACTIVATED! ***"

**Nguyên nhân có thể:**
1. **Accessibility permissions chưa được cấp**
2. **Hotkey bị conflict với app khác**
3. **CGEventTap chưa hoạt động đúng**

**Giải pháp:**

#### Bước 1: Kiểm tra Accessibility Permissions
```bash
cd build_simple && ./simple_test
```
Nếu thấy "Platform supported: false" → Cần cấp quyền:

1. System Preferences > Security & Privacy > Privacy > Accessibility
2. Click 🔒 để unlock
3. Thêm **Terminal.app** (hoặc iTerm.app) vào list
4. ✅ Enable checkbox
5. **Restart terminal** và chạy lại test

#### Bước 2: Test với hotkey khác
```cpp
// Thử thay đổi hotkey trong simple_test.cpp
QHotkey hotkey(QKeySequence("Ctrl+Shift+F1"), true);  // Ít conflict hơn
```

#### Bước 3: Debug CGEventTap
Thêm debug info vào `qhotkey_mac.cpp`:
```cpp
qDebug() << "CGEventTap created successfully";
qDebug() << "Event mask:" << eventMask;
```

### 2. ❌ Systray App hiển thị console trong Dock

**Triệu chứng:**
- Systray icon xuất hiện đúng
- Nhưng vẫn thấy app icon trong Dock
- Không phải true background app

**Nguyên nhân:**
- `TransformProcessType` không hoạt động
- Build configuration chưa đúng

**Giải pháp:**

#### Option 1: Sử dụng Info.plist
Tạo file `Info.plist`:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>LSUIElement</key>
    <true/>
</dict>
</plist>
```

#### Option 2: Build as Bundle
Update CMakeLists.txt:
```cmake
if(APPLE)
    set_target_properties(systray_test PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist
    )
endif()
```

#### Option 3: Runtime hiding (current approach)
Đảm bảo code này được gọi:
```cpp
#ifdef Q_OS_MACOS
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    TransformProcessType(&psn, kProcessTransformToUIElementApplication);
#endif
```

### 3. ❌ "Platform not supported" error

**Triệu chứng:**
```
Platform supported: false
QHotkey: Accessibility permissions not granted
```

**Giải pháp:**
1. **Grant Accessibility permissions** (xem bước 1 ở trên)
2. **Restart application** sau khi cấp quyền
3. **Check system restrictions:**
   - Không chạy trong sandbox
   - Không có parental controls
   - macOS version >= 10.9

### 4. ❌ Build errors

**Triệu chứng:**
```
fatal error: 'ApplicationServices/ApplicationServices.h' file not found
```

**Giải pháp:**
Đảm bảo frameworks được link đúng:
```cmake
if(APPLE)
    find_library(CARBON_LIBRARY Carbon)
    find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices)
    find_library(FOUNDATION_LIBRARY Foundation)

    target_link_libraries(your_target
        ${CARBON_LIBRARY}
        ${APPLICATIONSERVICES_LIBRARY}
        ${FOUNDATION_LIBRARY})
endif()
```

### 5. ❌ Hotkeys conflict với system shortcuts

**Triệu chứng:**
- Hotkey registered thành công
- Nhưng system shortcut được trigger thay vì app

**Giải pháp:**
1. **Sử dụng unique combinations:**
   ```cpp
   // Tốt - ít conflict
   QHotkey("Ctrl+Shift+F1", true);
   QHotkey("Ctrl+Alt+Shift+H", true);

   // Tránh - dễ conflict
   QHotkey("Cmd+Space", true);  // Spotlight
   QHotkey("Cmd+Tab", true);    // App switcher
   ```

2. **Check system shortcuts:**
   - System Preferences > Keyboard > Shortcuts
   - Disable conflicting shortcuts nếu cần

### 6. ❌ Memory leaks hoặc crashes

**Triệu chứng:**
- App crash khi quit
- Memory usage tăng liên tục

**Giải pháp:**
1. **Proper cleanup trong destructor:**
   ```cpp
   ~QHotkey() {
       unregister();
       // Cleanup CGEventTap
   }
   ```

2. **Use proper Qt parent-child relationships:**
   ```cpp
   QHotkey *hotkey = new QHotkey(sequence, true, this);  // 'this' as parent
   ```

## 🧪 Debug Commands

### Test Accessibility Permissions
```bash
# Check if accessibility is enabled for Terminal
osascript -e 'tell application "System Events" to get UI elements enabled'
```

### Monitor CGEventTap
```bash
# Monitor system events (requires admin)
sudo fs_usage -w | grep CGEvent
```

### Check Process Type
```bash
# See if app is properly hidden from dock
ps aux | grep systray_test
```

### Test Hotkey Registration
```bash
# Run with debug output
cd build_simple
QT_LOGGING_RULES="*.debug=true" ./simple_test
```

## 📋 Verification Checklist

### Simple Test
- [ ] Build successful
- [ ] "Platform supported: true"
- [ ] "Hotkey registered successfully"
- [ ] Pressing Ctrl+Shift+H shows "*** HOTKEY ACTIVATED! ***"
- [ ] App auto-quits after 30 seconds

### Systray Test
- [ ] Build successful
- [ ] Blue icon appears in menu bar
- [ ] Startup notification shows
- [ ] Right-click menu works
- [ ] Ctrl+Shift+H shows message
- [ ] Ctrl+Shift+Q quits app
- [ ] **No dock icon** (true background app)

## 🔄 Reset & Clean Build

Nếu gặp vấn đề persistent:

```bash
# Clean everything
rm -rf build build_simple build_systray

# Rebuild from scratch
mkdir build && cd build
cmake .. && make
cd ..

# Rebuild tests
./build_systray_test.sh
cd build_simple && cmake . && make
```

## 📞 Still Having Issues?

1. **Check macOS version:** CGEventTap requires macOS 10.4+
2. **Verify Qt version:** Tested with Qt 5.15+ and Qt 6.2+
3. **Check Xcode tools:** `xcode-select --install`
4. **Try different hotkey combinations**
5. **Test with minimal example**

## 🎯 Success Indicators

**Simple Test Working:**
```
QHotkey CGEventTap Test
Platform supported: true
Hotkey registered successfully: QKeySequence("Ctrl+Shift+H")
Press Ctrl+Shift+H to test...
*** HOTKEY ACTIVATED! ***  ← This should appear when you press the hotkey
```

**Systray Test Working:**
- Blue icon in menu bar (not dock)
- Startup notification appears
- Hotkeys trigger notifications
- Right-click menu functional
- Clean quit with Ctrl+Shift+Q

---

**💡 Pro Tip:** Always test with Accessibility permissions granted first - 90% of issues are permission-related!