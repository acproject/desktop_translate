#include "GlobalShortcut.h"
#include <QDebug>
#include <QCoreApplication>
#include <QTimer>

// X11 headers
#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <X11/XKBlib.h>

namespace DesktopTranslate {

struct GlobalShortcut::Impl {
    Display* display{nullptr};
    Window root{0};
    QHash<QString, int> registered_keys;
    QTimer* event_timer{nullptr};
};

GlobalShortcut& GlobalShortcut::instance() {
    static GlobalShortcut instance;
    return instance;
}

GlobalShortcut::GlobalShortcut() 
    : impl_(std::make_unique<Impl>())
{
    impl_->display = XOpenDisplay(nullptr);
    if (!impl_->display) {
        qWarning() << "Failed to open X display for global shortcuts";
        return;
    }
    
    impl_->root = DefaultRootWindow(impl_->display);
    
    // 创建定时器来检查 X11 事件
    impl_->event_timer = new QTimer(this);
    connect(impl_->event_timer, &QTimer::timeout, this, [this]() {
        if (!impl_->display) return;
        
        XEvent event;
        while (XPending(impl_->display) > 0) {
            XNextEvent(impl_->display, &event);
            
            if (event.type == KeyPress) {
                XKeyEvent* keyEvent = &event.xkey;
                
                // 查找匹配的快捷键
                for (auto it = impl_->registered_keys.begin(); 
                     it != impl_->registered_keys.end(); ++it) {
                    if (it.value() == keyEvent->keycode) {
                        qDebug() << "Global shortcut activated:" << it.key();
                        emit shortcutActivated(it.key());
                        break;
                    }
                }
            }
        }
    });
    impl_->event_timer->start(50);  // 每50ms检查一次
}

GlobalShortcut::~GlobalShortcut() {
    unregisterAll();
    
    if (impl_->event_timer) {
        impl_->event_timer->stop();
        delete impl_->event_timer;
    }
    
    if (impl_->display) {
        XCloseDisplay(impl_->display);
    }
}

// 解析快捷键字符串
static bool parseShortcut(const QString& shortcut, Display* display, 
                          KeySym* keysym, unsigned int* modifiers) {
    QStringList parts = shortcut.split('+');
    *modifiers = 0;
    QString key;
    
    for (const QString& part : parts) {
        QString trimmed = part.trimmed().toLower();
        
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
    
    if (key.isEmpty()) return false;
    
    // 解析按键
    if (key.length() == 1 && key[0].isLetter()) {
        *keysym = XStringToKeysym(key.toUpper().toLatin1().data());
    } else if (key.startsWith('f') && key.length() <= 3) {
        // F1-F35
        *keysym = XStringToKeysym(key.toUpper().toLatin1().data());
    } else {
        *keysym = XStringToKeysym(key.toLatin1().data());
    }
    
    return (*keysym != NoSymbol);
}

bool GlobalShortcut::registerShortcut(const QString& shortcut, const QString& id) {
    if (!impl_->display) {
        qWarning() << "X display not available";
        return false;
    }
    
    KeySym keysym;
    unsigned int modifiers;
    
    if (!parseShortcut(shortcut, impl_->display, &keysym, &modifiers)) {
        qWarning() << "Failed to parse shortcut:" << shortcut;
        return false;
    }
    
    KeyCode keycode = XKeysymToKeycode(impl_->display, keysym);
    if (keycode == 0) {
        qWarning() << "Invalid key:" << shortcut;
        return false;
    }
    
    // 抓取按键
    XGrabKey(impl_->display, keycode, modifiers, impl_->root, 
             True, GrabModeAsync, GrabModeAsync);
    
    // 同时抓取 NumLock 和 CapsLock 的组合
    XGrabKey(impl_->display, keycode, modifiers | Mod2Mask, impl_->root,
             True, GrabModeAsync, GrabModeAsync);
    XGrabKey(impl_->display, keycode, modifiers | LockMask, impl_->root,
             True, GrabModeAsync, GrabModeAsync);
    XGrabKey(impl_->display, keycode, modifiers | Mod2Mask | LockMask, impl_->root,
             True, GrabModeAsync, GrabModeAsync);
    
    impl_->registered_keys[id] = keycode;
    
    qDebug() << "Registered global shortcut:" << shortcut << "as" << id;
    return true;
}

void GlobalShortcut::unregisterShortcut(const QString& id) {
    if (!impl_->display) return;
    
    if (impl_->registered_keys.contains(id)) {
        KeyCode keycode = impl_->registered_keys.value(id);
        XUngrabKey(impl_->display, keycode, AnyModifier, impl_->root);
        impl_->registered_keys.remove(id);
    }
}

void GlobalShortcut::unregisterAll() {
    if (!impl_->display) return;
    
    for (auto it = impl_->registered_keys.begin(); 
         it != impl_->registered_keys.end(); ++it) {
        XUngrabKey(impl_->display, it.value(), AnyModifier, impl_->root);
    }
    impl_->registered_keys.clear();
}

} // namespace DesktopTranslate
