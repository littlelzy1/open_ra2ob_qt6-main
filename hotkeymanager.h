#ifndef HOTKEYMANAGER_H
#define HOTKEYMANAGER_H

#include <QAbstractNativeEventFilter>
#include <QObject>

#include "./globalsetting.h"

class NEFilter : public QAbstractNativeEventFilter {
public:
    NEFilter();
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    Globalsetting *gls;
    void hideOb();
    void hideBottomPanel();
    void swapTeams();
    void resetTeamSwap();
};

class HotkeyManager {
public:
    HotkeyManager();
    void registerHotkey(const NEFilter &filter);
    void releaseHotkey();

private:
    bool registerSingle(const quint32 &mod, const quint32 &kc);
    bool releaseSingle(const quint32 &mod, const quint32 &kc);
};

#endif  // HOTKEYMANAGER_H
