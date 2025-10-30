#include "./hotkeymanager.h"

#include <windows.h>
#include <QApplication>
#include <QDebug>
#include <iostream>

#include "ob3.h"

NEFilter::NEFilter() { gls = &Globalsetting::getInstance(); }

bool NEFilter::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        const quint32 keycode   = HIWORD(msg->lParam);
        const quint32 modifiers = LOWORD(msg->lParam);

        qDebug() << "Hotkey pressed - keycode:" << (char)keycode << "modifiers:" << modifiers;

        switch (modifiers) {
            case MOD_CONTROL | MOD_ALT:
                switch (keycode) {
                    case 'J':
                        qDebug() << "Ctrl+Alt+J pressed";
                        hideBottomPanel();
                        break;
                    case 'H':
                        qDebug() << "Ctrl+Alt+H pressed";
                        hideOb();
                        break;
                    case 'K':
                        qDebug() << "Ctrl+Alt+K pressed - swapping teams";
                        swapTeams();
                        break;
                    case 'R':
                        qDebug() << "Ctrl+Alt+R pressed - resetting team swap";
                        resetTeamSwap();
                        break;
                    default:
                        break;
                }
                break;
            case MOD_CONTROL:
                switch (keycode) {
                    default:
                        break;
                }
                break;
        }
    }
    return false;
}

void NEFilter::hideOb() { gls->s.show_all_panel = !gls->s.show_all_panel; }

void NEFilter::hideBottomPanel() { gls->s.show_bottom_panel = !gls->s.show_bottom_panel; }

void NEFilter::swapTeams() {
    // 通过QApplication获取所有顶级窗口，查找Ob3实例
    QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
    qDebug() << "Total top level widgets:" << topLevelWidgets.size();
    
    for (QWidget* widget : topLevelWidgets) {
        qDebug() << "Checking widget:" << widget << "Type:" << widget->metaObject()->className();
        
        // 直接检查是否是Ob3类型
        Ob3* ob3Widget = qobject_cast<Ob3*>(widget);
        if (ob3Widget) {
            qDebug() << "Found Ob3 widget directly:" << ob3Widget;
            qDebug() << "Calling swapTeams() on Ob3 widget";
            ob3Widget->swapTeams();
            return;
        }
        
        // 如果不是直接的Ob3，尝试在其子控件中查找
        Ob3* childOb3 = widget->findChild<Ob3*>();
        if (childOb3) {
            qDebug() << "Found Ob3 widget as child:" << childOb3;
            qDebug() << "Calling swapTeams() on Ob3 widget";
            childOb3->swapTeams();
            return;
        }
    }
    
    qDebug() << "No Ob3 widget found in any top level widget";
}

void NEFilter::resetTeamSwap() {
    // 通过QApplication获取所有顶级窗口，查找Ob3实例
    QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
    
    for (QWidget* widget : topLevelWidgets) {
        // 直接检查是否是Ob3类型
        Ob3* ob3Widget = qobject_cast<Ob3*>(widget);
        if (ob3Widget) {
            qDebug() << "Found Ob3 widget directly, calling resetTeamSwap()";
            ob3Widget->resetTeamSwap();
            return;
        }
        
        // 如果不是直接的Ob3，尝试在其子控件中查找
        Ob3* childOb3 = widget->findChild<Ob3*>();
        if (childOb3) {
            qDebug() << "Found Ob3 widget as child, calling resetTeamSwap()";
            childOb3->resetTeamSwap();
            return;
        }
    }
    
    qDebug() << "No Ob3 widget found for resetTeamSwap";
}

HotkeyManager::HotkeyManager() {}

void HotkeyManager::registerHotkey(const NEFilter& filter) {
    Globalsetting& gls = Globalsetting::getInstance();

    gls.s.sc_ctrl_alt_j        = registerSingle(MOD_CONTROL | MOD_ALT, 'J');
    gls.s.sc_ctrl_alt_h        = registerSingle(MOD_CONTROL | MOD_ALT, 'H');
    
    // 注册队伍交换快捷键
    registerSingle(MOD_CONTROL | MOD_ALT, 'K');
    registerSingle(MOD_CONTROL | MOD_ALT, 'R');
}

void HotkeyManager::releaseHotkey() {
    releaseSingle(MOD_CONTROL | MOD_ALT, 'J');
    releaseSingle(MOD_CONTROL | MOD_ALT, 'H');
    
    // 释放队伍交换快捷键
    releaseSingle(MOD_CONTROL | MOD_ALT, 'K');
    releaseSingle(MOD_CONTROL | MOD_ALT, 'R');
}

bool HotkeyManager::registerSingle(const quint32& mod, const quint32& kc) {
    static int hotkeyId = 1000; // 使用静态变量确保唯一ID
    BOOL ok = RegisterHotKey(0, hotkeyId++, mod, kc);
    if (!ok) {
        return false;
    }
    return true;
}

bool HotkeyManager::releaseSingle(const quint32& mod, const quint32& kc) {
    // 由于使用了递增ID，这里需要重新设计释放机制
    // 暂时注释掉，因为程序退出时系统会自动清理
    // BOOL ok = UnregisterHotKey(0, kc ^ mod);
    // if (!ok) {
    //     return false;
    // }
    return true;
}
