#include "GlobalShortcut.h"
#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QDebug>
#include <QHash>
#include <QStringList>
#include <QTimer>

#if defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_UNIX)
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#endif

namespace DesktopTranslate {

#if defined(Q_OS_WIN)
struct ShortcutRegistration {
    int native_id{0};
    UINT modifiers{0};
    UINT virtual_key{0};
};

class WindowsHotkeyEventFilter : public QAbstractNativeEventFilter {
public:
    explicit WindowsHotkeyEventFilter(GlobalShortcut* owner)
        : owner_(owner) {
    }

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override {
        if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") {
            return false;
        }

        auto* nativeMessage = static_cast<MSG*>(message);
        if (!nativeMessage || nativeMessage->message != WM_HOTKEY) {
            return false;
        }

        owner_->activateShortcutByNativeId(static_cast<int>(nativeMessage->wParam));
        if (result) {
            *result = 0;
        }
        return true;
    }

private:
    GlobalShortcut* owner_{nullptr};
};

struct GlobalShortcut::Impl {
    QHash<QString, ShortcutRegistration> registered_keys;
    int next_hotkey_id{1};
    std::unique_ptr<WindowsHotkeyEventFilter> event_filter;
};

static bool parseShortcut(const QString& shortcut, UINT* virtual_key, UINT* modifiers) {
    *virtual_key = 0;
    *modifiers = 0;

    const QStringList parts = shortcut.split('+', Qt::SkipEmptyParts);
    QString key;

    for (const QString& part : parts) {
        const QString trimmed = part.trimmed().toLower();
        if (trimmed == "ctrl" || trimmed == "control") {
            *modifiers |= MOD_CONTROL;
        } else if (trimmed == "alt") {
            *modifiers |= MOD_ALT;
        } else if (trimmed == "shift") {
            *modifiers |= MOD_SHIFT;
        } else if (trimmed == "super" || trimmed == "meta" || trimmed == "win") {
            *modifiers |= MOD_WIN;
        } else {
            key = trimmed;
        }
    }

    if (key.isEmpty()) {
        return false;
    }

    if (key.length() == 1) {
        const QChar c = key[0].toUpper();
        if (c.isLetterOrNumber()) {
            *virtual_key = static_cast<UINT>(c.unicode());
            return true;
        }
    }

    if (key.startsWith('f')) {
        bool ok = false;
        const int index = key.mid(1).toInt(&ok);
        if (ok && index >= 1 && index <= 24) {
            *virtual_key = static_cast<UINT>(VK_F1 + index - 1);
            return true;
        }
    }

    static const QHash<QString, UINT> key_map = {
        {"tab", VK_TAB},
        {"space", VK_SPACE},
        {"enter", VK_RETURN},
        {"return", VK_RETURN},
        {"esc", VK_ESCAPE},
        {"escape", VK_ESCAPE},
        {"left", VK_LEFT},
        {"right", VK_RIGHT},
        {"up", VK_UP},
        {"down", VK_DOWN},
        {"home", VK_HOME},
        {"end", VK_END},
        {"pageup", VK_PRIOR},
        {"pagedown", VK_NEXT},
        {"insert", VK_INSERT},
        {"delete", VK_DELETE},
        {"backspace", VK_BACK}
    };

    const auto it = key_map.find(key);
    if (it == key_map.end()) {
        return false;
    }

    *virtual_key = it.value();
    return true;
}
#elif defined(Q_OS_UNIX)
struct GlobalShortcut::Impl {
    Display* display{nullptr};
    Window root{0};
    QHash<QString, int> registered_keys;
    QTimer* event_timer{nullptr};
};

static bool parseShortcut(const QString& shortcut, KeySym* keysym, unsigned int* modifiers) {
    const QStringList parts = shortcut.split('+', Qt::SkipEmptyParts);
    *modifiers = 0;
    QString key;

    for (const QString& part : parts) {
        const QString trimmed = part.trimmed().toLower();
        if (trimmed == "ctrl" || trimmed == "control") {
            *modifiers |= ControlMask;
        } else if (trimmed == "alt") {
            *modifiers |= Mod1Mask;
        } else if (trimmed == "shift") {
            *modifiers |= ShiftMask;
        } else if (trimmed == "super" || trimmed == "meta" || trimmed == "win") {
            *modifiers |= Mod4Mask;
        } else {
            key = trimmed;
        }
    }

    if (key.isEmpty()) {
        return false;
    }

    if (key.length() == 1 && key[0].isLetter()) {
        *keysym = XStringToKeysym(key.toUpper().toLatin1().data());
    } else if (key.startsWith('f') && key.length() <= 3) {
        *keysym = XStringToKeysym(key.toUpper().toLatin1().data());
    } else {
        *keysym = XStringToKeysym(key.toLatin1().data());
    }

    return *keysym != NoSymbol;
}
#else
struct GlobalShortcut::Impl {
    QHash<QString, int> registered_keys;
};
#endif

GlobalShortcut& GlobalShortcut::instance() {
    static GlobalShortcut instance;
    return instance;
}

GlobalShortcut::GlobalShortcut()
    : impl_(std::make_unique<Impl>())
{
#if defined(Q_OS_WIN)
    impl_->event_filter = std::make_unique<WindowsHotkeyEventFilter>(this);
    QCoreApplication::instance()->installNativeEventFilter(impl_->event_filter.get());
#elif defined(Q_OS_UNIX)
    impl_->display = XOpenDisplay(nullptr);
    if (!impl_->display) {
        qWarning() << "Failed to open X display for global shortcuts";
        return;
    }

    impl_->root = DefaultRootWindow(impl_->display);

    impl_->event_timer = new QTimer(this);
    connect(impl_->event_timer, &QTimer::timeout, this, [this]() {
        if (!impl_->display) {
            return;
        }

        XEvent event;
        while (XPending(impl_->display) > 0) {
            XNextEvent(impl_->display, &event);
            if (event.type != KeyPress) {
                continue;
            }

            auto* key_event = &event.xkey;
            for (auto it = impl_->registered_keys.cbegin(); it != impl_->registered_keys.cend(); ++it) {
                if (it.value() == key_event->keycode) {
                    emit shortcutActivated(it.key());
                    break;
                }
            }
        }
    });
#else
    qWarning() << "Global shortcuts are not implemented on this platform";
    return;
#endif
}

GlobalShortcut::~GlobalShortcut() {
    unregisterAll();

#if defined(Q_OS_WIN)
    if (impl_->event_filter && QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(impl_->event_filter.get());
    }
#elif defined(Q_OS_UNIX)
    if (impl_->event_timer) {
        impl_->event_timer->stop();
        delete impl_->event_timer;
    }
#endif

#if defined(Q_OS_UNIX)
    if (impl_->display) {
        XCloseDisplay(impl_->display);
    }
#endif
}

bool GlobalShortcut::registerShortcut(const QString& shortcut, const QString& id) {
#if defined(Q_OS_WIN)
    UINT virtual_key = 0;
    UINT modifiers = 0;
    if (!parseShortcut(shortcut, &virtual_key, &modifiers)) {
        qWarning() << "Failed to parse shortcut:" << shortcut;
        return false;
    }

    unregisterShortcut(id);

    const int native_id = impl_->next_hotkey_id++;
    if (!RegisterHotKey(nullptr, native_id, modifiers | MOD_NOREPEAT, virtual_key)) {
        qWarning() << "Failed to register global shortcut:" << shortcut << "error:" << GetLastError();
        return false;
    }

    impl_->registered_keys[id] = ShortcutRegistration{native_id, modifiers, virtual_key};
    qDebug() << "Registered global shortcut:" << shortcut << "as" << id;
    return true;
#elif defined(Q_OS_UNIX)
    if (!impl_->display) {
        qWarning() << "X display not available";
        return false;
    }

    KeySym keysym;
    unsigned int modifiers;
    if (!parseShortcut(shortcut, &keysym, &modifiers)) {
        qWarning() << "Failed to parse shortcut:" << shortcut;
        return false;
    }

    const KeyCode keycode = XKeysymToKeycode(impl_->display, keysym);
    if (keycode == 0) {
        qWarning() << "Invalid key:" << shortcut;
        return false;
    }

    unregisterShortcut(id);

    XGrabKey(impl_->display, keycode, modifiers, impl_->root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(impl_->display, keycode, modifiers | Mod2Mask, impl_->root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(impl_->display, keycode, modifiers | LockMask, impl_->root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(impl_->display, keycode, modifiers | Mod2Mask | LockMask, impl_->root, True, GrabModeAsync, GrabModeAsync);

    impl_->registered_keys[id] = keycode;
    qDebug() << "Registered global shortcut:" << shortcut << "as" << id;
    return true;
#else
    Q_UNUSED(shortcut)
    Q_UNUSED(id)
    return false;
#endif
}

void GlobalShortcut::unregisterShortcut(const QString& id) {
#if defined(Q_OS_WIN)
    if (!impl_->registered_keys.contains(id)) {
        return;
    }

    const auto registration = impl_->registered_keys.take(id);
    UnregisterHotKey(nullptr, registration.native_id);
#elif defined(Q_OS_UNIX)
    if (!impl_->display || !impl_->registered_keys.contains(id)) {
        return;
    }

    const KeyCode keycode = impl_->registered_keys.take(id);
    XUngrabKey(impl_->display, keycode, AnyModifier, impl_->root);
#else
    Q_UNUSED(id)
#endif
}

void GlobalShortcut::unregisterAll() {
#if defined(Q_OS_WIN)
    for (auto it = impl_->registered_keys.cbegin(); it != impl_->registered_keys.cend(); ++it) {
        UnregisterHotKey(nullptr, it.value().native_id);
    }
    impl_->registered_keys.clear();
#elif defined(Q_OS_UNIX)
    if (!impl_->display) {
        impl_->registered_keys.clear();
        return;
    }

    for (auto it = impl_->registered_keys.cbegin(); it != impl_->registered_keys.cend(); ++it) {
        XUngrabKey(impl_->display, it.value(), AnyModifier, impl_->root);
    }
    impl_->registered_keys.clear();
#else
    impl_->registered_keys.clear();
#endif
}

void GlobalShortcut::activateShortcutByNativeId(int native_id) {
    for (auto it = impl_->registered_keys.cbegin(); it != impl_->registered_keys.cend(); ++it) {
        if (it.value().native_id == native_id) {
            emit shortcutActivated(it.key());
            return;
        }
    }
}

} // namespace DesktopTranslate
