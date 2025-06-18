#include "qhotkey.h"
#include "qhotkey_p.h"
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <QDebug>
#include <QGuiApplication>

class QHotkeyPrivateMac : public QHotkeyPrivate
{
public:
	QHotkeyPrivateMac();
	~QHotkeyPrivateMac();

	// QAbstractNativeEventFilter interface
	bool nativeEventFilter(const QByteArray &eventType, void *message, _NATIVE_EVENT_RESULT *result) override;

	static CGEventRef eventTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon);
	static bool checkAccessibilityPermissions();
	static bool requestAccessibilityPermissions();

protected:
	// QHotkeyPrivate interface
	quint32 nativeKeycode(Qt::Key keycode, bool &ok) Q_DECL_OVERRIDE;
	quint32 nativeModifiers(Qt::KeyboardModifiers modifiers, bool &ok) Q_DECL_OVERRIDE;
	bool registerShortcut(QHotkey::NativeShortcut shortcut) Q_DECL_OVERRIDE;
	bool unregisterShortcut(QHotkey::NativeShortcut shortcut) Q_DECL_OVERRIDE;

private:
	struct HotkeyInfo {
		QHotkey::NativeShortcut shortcut;
		bool isPressed;
		HotkeyInfo() : isPressed(false) {}
		HotkeyInfo(const QHotkey::NativeShortcut &s) : shortcut(s), isPressed(false) {}
	};

	void setupEventTap();
	void cleanupEventTap();
	bool checkApplicationType();
	bool matchesShortcut(CGEventRef event, const QHotkey::NativeShortcut &shortcut);

	static CFMachPortRef eventTap;
	static CFRunLoopSourceRef runLoopSource;
	static QHash<QHotkey::NativeShortcut, HotkeyInfo> registeredHotkeys;
	static QHotkeyPrivateMac* instance;
};
NATIVE_INSTANCE(QHotkeyPrivateMac)

bool QHotkeyPrivate::isPlatformSupported()
{
	return QHotkeyPrivateMac::checkAccessibilityPermissions();
}

// Static member definitions
CFMachPortRef QHotkeyPrivateMac::eventTap = nullptr;
CFRunLoopSourceRef QHotkeyPrivateMac::runLoopSource = nullptr;
QHash<QHotkey::NativeShortcut, QHotkeyPrivateMac::HotkeyInfo> QHotkeyPrivateMac::registeredHotkeys;
QHotkeyPrivateMac* QHotkeyPrivateMac::instance = nullptr;

QHotkeyPrivateMac::QHotkeyPrivateMac()
{
	instance = this;
	if (!checkAccessibilityPermissions()) {
		qWarning() << "QHotkey: Accessibility permissions not granted. Global hotkeys may not work.";
		qWarning() << "QHotkey: Please grant accessibility permissions in System Preferences > Security & Privacy > Privacy > Accessibility";
	}
	setupEventTap();
}

QHotkeyPrivateMac::~QHotkeyPrivateMac()
{
	cleanupEventTap();
	instance = nullptr;
}

bool QHotkeyPrivateMac::nativeEventFilter(const QByteArray &eventType, void *message, _NATIVE_EVENT_RESULT *result)
{
	Q_UNUSED(eventType)
	Q_UNUSED(message)
	Q_UNUSED(result)
	return false;
}

bool QHotkeyPrivateMac::checkAccessibilityPermissions()
{
	// Check if we have accessibility permissions
	CFStringRef keys[] = { kAXTrustedCheckOptionPrompt };
	CFBooleanRef values[] = { kCFBooleanFalse };
	CFDictionaryRef options = CFDictionaryCreate(kCFAllocatorDefault,
		(const void**)keys, (const void**)values, 1,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	bool result = AXIsProcessTrustedWithOptions(options);
	CFRelease(options);
	return result;
}

bool QHotkeyPrivateMac::requestAccessibilityPermissions()
{
	// Request accessibility permissions with prompt
	CFStringRef keys[] = { kAXTrustedCheckOptionPrompt };
	CFBooleanRef values[] = { kCFBooleanTrue };
	CFDictionaryRef options = CFDictionaryCreate(kCFAllocatorDefault,
		(const void**)keys, (const void**)values, 1,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	bool result = AXIsProcessTrustedWithOptions(options);
	CFRelease(options);
	return result;
}

bool QHotkeyPrivateMac::checkApplicationType()
{
	QCoreApplication* app = QCoreApplication::instance();
	if (!app) {
		qWarning() << "QHotkey: No QCoreApplication instance found!";
		return false;
	}

	// Check if we have a GUI application (QApplication or QGuiApplication)
	bool isGuiApp = qobject_cast<QGuiApplication*>(app) != nullptr;

	if (!isGuiApp) {
		qWarning() << "QHotkey: macOS requires QApplication or QGuiApplication for global hotkeys!";
		qWarning() << "QHotkey: CGEventTap needs GUI event loop to function properly.";
		qWarning() << "QHotkey: Please change QCoreApplication to QApplication in your main.cpp";
		qWarning() << "QHotkey: Example:";
		qWarning() << "QHotkey:   QApplication app(argc, argv);  // Instead of QCoreApplication";
		return false;
	}

	qDebug() << "QHotkey: GUI application detected - CGEventTap should work properly";
	return true;
}

void QHotkeyPrivateMac::setupEventTap()
{
	if (eventTap != nullptr) {
		qDebug() << "QHotkey: Event tap already set up";
		return; // Already set up
	}

	// Check application type first
	if (!checkApplicationType()) {
		qWarning() << "QHotkey: Cannot setup event tap - wrong application type";
		return;
	}

	qDebug() << "QHotkey: Setting up CGEventTap...";

	// Create event tap for key down and key up events
	CGEventMask eventMask = (1 << kCGEventKeyDown) | (1 << kCGEventKeyUp) | (1 << kCGEventFlagsChanged);
	qDebug() << "QHotkey: Event mask:" << eventMask;

	eventTap = CGEventTapCreate(
		kCGSessionEventTap,
		kCGHeadInsertEventTap,
		kCGEventTapOptionDefault,
		eventMask,
		eventTapCallback,
		nullptr
	);

	if (eventTap == nullptr) {
		qWarning() << "QHotkey: Failed to create event tap. Make sure accessibility permissions are granted.";
		qWarning() << "QHotkey: Go to System Preferences > Security & Privacy > Privacy > Accessibility";
		qWarning() << "QHotkey: Add Terminal.app (or your app) to the allowed list";
		return;
	}

	qDebug() << "QHotkey: Event tap created successfully";

	// Create run loop source and add to current run loop
	runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
	if (runLoopSource == nullptr) {
		qWarning() << "QHotkey: Failed to create run loop source";
		return;
	}

	qDebug() << "QHotkey: Adding to run loop...";
	CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);

	// Enable the event tap
	CGEventTapEnable(eventTap, true);
	qDebug() << "QHotkey: Event tap enabled and ready";
}

void QHotkeyPrivateMac::cleanupEventTap()
{
	if (runLoopSource != nullptr) {
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
		CFRelease(runLoopSource);
		runLoopSource = nullptr;
	}

	if (eventTap != nullptr) {
		CGEventTapEnable(eventTap, false);
		CFRelease(eventTap);
		eventTap = nullptr;
	}
}

CGEventRef QHotkeyPrivateMac::eventTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon)
{
	Q_UNUSED(proxy)
	Q_UNUSED(refcon)

	if (instance == nullptr) {
		return event;
	}

	// Debug: Log all key events
	if (type == kCGEventKeyDown || type == kCGEventKeyUp) {
		CGKeyCode keyCode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
		CGEventFlags flags = CGEventGetFlags(event);
		qDebug() << "Key event:" << (type == kCGEventKeyDown ? "DOWN" : "UP")
		         << "keyCode:" << keyCode << "flags:" << flags;
	}

	// Handle key events
	if (type == kCGEventKeyDown || type == kCGEventKeyUp) {
		// Check all registered hotkeys
		for (auto it = registeredHotkeys.begin(); it != registeredHotkeys.end(); ++it) {
			if (instance->matchesShortcut(event, it.key())) {
				if (type == kCGEventKeyDown && !it.value().isPressed) {
					it.value().isPressed = true;
					instance->activateShortcut(it.key());
				} else if (type == kCGEventKeyUp && it.value().isPressed) {
					it.value().isPressed = false;
					instance->releaseShortcut(it.key());
				}
			}
		}
	}

	return event;
}

bool QHotkeyPrivateMac::matchesShortcut(CGEventRef event, const QHotkey::NativeShortcut &shortcut)
{
	// Get key code from event
	CGKeyCode keyCode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

	// Get modifier flags from event
	CGEventFlags flags = CGEventGetFlags(event);

	// Convert CGEventFlags to our modifier format
	quint32 eventModifiers = 0;
	if (flags & kCGEventFlagMaskShift) {
		eventModifiers |= shiftKey;
	}
	if (flags & kCGEventFlagMaskCommand) {
		eventModifiers |= cmdKey;
	}
	if (flags & kCGEventFlagMaskAlternate) {
		eventModifiers |= optionKey;
	}
	if (flags & kCGEventFlagMaskControl) {
		eventModifiers |= controlKey;
	}

	// Check if key and modifiers match
	return (keyCode == shortcut.key) && (eventModifiers == shortcut.modifier);
}

quint32 QHotkeyPrivateMac::nativeKeycode(Qt::Key keycode, bool &ok)
{
	// Constants found in NSEvent.h from AppKit.framework
	ok = true;
	switch (keycode) {
	case Qt::Key_Return:
		return kVK_Return;
	case Qt::Key_Enter:
		return kVK_ANSI_KeypadEnter;
	case Qt::Key_Tab:
		return kVK_Tab;
	case Qt::Key_Space:
		return kVK_Space;
	case Qt::Key_Backspace:
		return kVK_Delete;
	case Qt::Key_Escape:
		return kVK_Escape;
	case Qt::Key_CapsLock:
		return kVK_CapsLock;
	case Qt::Key_Option:
		return kVK_Option;
	case Qt::Key_F17:
		return kVK_F17;
	case Qt::Key_VolumeUp:
		return kVK_VolumeUp;
	case Qt::Key_VolumeDown:
		return kVK_VolumeDown;
	case Qt::Key_F18:
		return kVK_F18;
	case Qt::Key_F19:
		return kVK_F19;
	case Qt::Key_F20:
		return kVK_F20;
	case Qt::Key_F5:
		return kVK_F5;
	case Qt::Key_F6:
		return kVK_F6;
	case Qt::Key_F7:
		return kVK_F7;
	case Qt::Key_F3:
		return kVK_F3;
	case Qt::Key_F8:
		return kVK_F8;
	case Qt::Key_F9:
		return kVK_F9;
	case Qt::Key_F11:
		return kVK_F11;
	case Qt::Key_F13:
		return kVK_F13;
	case Qt::Key_F16:
		return kVK_F16;
	case Qt::Key_F14:
		return kVK_F14;
	case Qt::Key_F10:
		return kVK_F10;
	case Qt::Key_F12:
		return kVK_F12;
	case Qt::Key_F15:
		return kVK_F15;
	case Qt::Key_Help:
		return kVK_Help;
	case Qt::Key_Home:
		return kVK_Home;
	case Qt::Key_PageUp:
		return kVK_PageUp;
	case Qt::Key_Delete:
		return kVK_ForwardDelete;
	case Qt::Key_F4:
		return kVK_F4;
	case Qt::Key_End:
		return kVK_End;
	case Qt::Key_F2:
		return kVK_F2;
	case Qt::Key_PageDown:
		return kVK_PageDown;
	case Qt::Key_F1:
		return kVK_F1;
	case Qt::Key_Left:
		return kVK_LeftArrow;
	case Qt::Key_Right:
		return kVK_RightArrow;
	case Qt::Key_Down:
		return kVK_DownArrow;
	case Qt::Key_Up:
		return kVK_UpArrow;
	default:
		ok = false;
		break;
	}

	UTF16Char ch = keycode;

	CFDataRef currentLayoutData;
	TISInputSourceRef currentKeyboard = TISCopyCurrentASCIICapableKeyboardLayoutInputSource();

	if (currentKeyboard == NULL)
		return 0;

	currentLayoutData = (CFDataRef)TISGetInputSourceProperty(currentKeyboard, kTISPropertyUnicodeKeyLayoutData);
	CFRelease(currentKeyboard);
	if (currentLayoutData == NULL)
		return 0;

	UCKeyboardLayout* header = (UCKeyboardLayout*)CFDataGetBytePtr(currentLayoutData);
	UCKeyboardTypeHeader* table = header->keyboardTypeList;

	uint8_t *data = (uint8_t*)header;
	for (quint32 i=0; i < header->keyboardTypeCount; i++) {
		UCKeyStateRecordsIndex* stateRec = 0;
		if (table[i].keyStateRecordsIndexOffset != 0) {
			stateRec = reinterpret_cast<UCKeyStateRecordsIndex*>(data + table[i].keyStateRecordsIndexOffset);
			if (stateRec->keyStateRecordsIndexFormat != kUCKeyStateRecordsIndexFormat) stateRec = 0;
		}

		UCKeyToCharTableIndex* charTable = reinterpret_cast<UCKeyToCharTableIndex*>(data + table[i].keyToCharTableIndexOffset);
		if (charTable->keyToCharTableIndexFormat != kUCKeyToCharTableIndexFormat) continue;

		for (quint32 j=0; j < charTable->keyToCharTableCount; j++) {
			UCKeyOutput* keyToChar = reinterpret_cast<UCKeyOutput*>(data + charTable->keyToCharTableOffsets[j]);
			for (quint32 k=0; k < charTable->keyToCharTableSize; k++) {
				if (keyToChar[k] & kUCKeyOutputTestForIndexMask) {
					long idx = keyToChar[k] & kUCKeyOutputGetIndexMask;
					if (stateRec && idx < stateRec->keyStateRecordCount) {
						UCKeyStateRecord* rec = reinterpret_cast<UCKeyStateRecord*>(data + stateRec->keyStateRecordOffsets[idx]);
						if (rec->stateZeroCharData == ch) {
							ok = true;
							return k;
						}
					}
				}
				else if (!(keyToChar[k] & kUCKeyOutputSequenceIndexMask) && keyToChar[k] < 0xFFFE) {
					if (keyToChar[k] == ch) {
						ok = true;
						return k;
					}
				}
			}
		}
	}
	return 0;
}

quint32 QHotkeyPrivateMac::nativeModifiers(Qt::KeyboardModifiers modifiers, bool &ok)
{
	quint32 nMods = 0;
	if (modifiers & Qt::ShiftModifier)
		nMods |= shiftKey;
	if (modifiers & Qt::ControlModifier)
		nMods |= controlKey;  // Qt Ctrl → macOS Control (⌃)
	if (modifiers & Qt::AltModifier)
		nMods |= optionKey;   // Qt Alt → macOS Option (⌥)
	if (modifiers & Qt::MetaModifier)
		nMods |= cmdKey;      // Qt Meta → macOS Command (⌘)
	if (modifiers & Qt::KeypadModifier)
		nMods |= kEventKeyModifierNumLockMask;
	ok = true;
	return nMods;
}

bool QHotkeyPrivateMac::registerShortcut(QHotkey::NativeShortcut shortcut)
{
	if (!shortcut.isValid()) {
		error = "Invalid shortcut";
		return false;
	}

	if (registeredHotkeys.contains(shortcut)) {
		error = "Shortcut already registered";
		return false;
	}

	// Check if we have accessibility permissions
	if (!checkAccessibilityPermissions()) {
		error = "Accessibility permissions not granted";
		qWarning() << "QHotkey: Accessibility permissions required for global hotkeys";
		// Try to request permissions
		requestAccessibilityPermissions();
		return false;
	}

	// Ensure event tap is set up
	if (eventTap == nullptr) {
		setupEventTap();
		if (eventTap == nullptr) {
			error = "Failed to create event tap";
			return false;
		}
	}

	// Add to registered hotkeys
	registeredHotkeys.insert(shortcut, HotkeyInfo(shortcut));
	return true;
}

bool QHotkeyPrivateMac::unregisterShortcut(QHotkey::NativeShortcut shortcut)
{
	if (!registeredHotkeys.contains(shortcut)) {
		error = "Shortcut not registered";
		return false;
	}

	registeredHotkeys.remove(shortcut);

	// If no more hotkeys are registered, we could clean up the event tap
	// but we'll keep it running for performance reasons

	return true;
}


