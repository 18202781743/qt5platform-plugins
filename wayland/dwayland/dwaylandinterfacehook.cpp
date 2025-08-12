// SPDX-FileCopyrightText: 2017 - 2022 Uniontech Software Technology Co.,Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#define private public
#include "QtWaylandClient/private/qwaylandnativeinterface_p.h"
#undef private

#include "dwaylandinterfacehook.h"
#include "dhighdpi.h"
#include "dxsettings.h"
#include "dnotitlebarwindowhelper_wl.h"

#include <qpa/qplatformnativeinterface.h>
#include <private/qguiapplication_p.h>
#include <QtWaylandClientVersion>

#include <QDebug>
#include <QLoggingCategory>

#ifndef QT_DEBUG
Q_LOGGING_CATEGORY(dwli, "dtk.wayland.interface" , QtInfoMsg);
#else
Q_LOGGING_CATEGORY(dwli, "dtk.wayland.interface");
#endif

DPP_BEGIN_NAMESPACE

static QFunctionPointer getFunction(const QByteArray &function)
{
    qCDebug(dwli) << "Getting function:" << function;
    static QHash<QByteArray, QFunctionPointer> functionCache = {
        {buildNativeSettings, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::buildNativeSettings)},
        {clearNativeSettings, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::clearNativeSettings)},
        {setEnableNoTitlebar, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::setEnableNoTitlebar)},
        {isEnableNoTitlebar, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::isEnableNoTitlebar)},
        {setWindowRadius, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::setWindowRadius)},
        {setBorderColor, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::setBorderColor)},
        {setBorderWidth, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::setBorderWidth)},
        {setShadowColor, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::setShadowColor)},
        {setShadowOffset, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::setShadowOffset)},
        {setShadowRadius, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::setShadowRadius)},
        {setWindowEffect, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::setWindowEffect)},
        {setWindowStartUpEffect, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::setWindowStartUpEffect)},
        {setWindowProperty, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::setWindowProperty)},
        {popupSystemWindowMenu, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::popupSystemWindowMenu)},
        {enableDwayland, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::enableDwayland)},
        {isEnableDwayland, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::isEnableDwayland)},
        {splitWindowOnScreen, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::splitWindowOnScreen)},
        {supportForSplittingWindow, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::supportForSplittingWindow)},
        {splitWindowOnScreenByType, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::splitWindowOnScreenByType)},
        {supportForSplittingWindowByType, reinterpret_cast<QFunctionPointer>(&DWaylandInterfaceHook::supportForSplittingWindowByType)}
    };
    const auto &result = functionCache.value(function);
    qCDebug(dwli) << "Function found:" << (result != nullptr);
    return result;
}

QFunctionPointer DWaylandInterfaceHook::platformFunction(QPlatformNativeInterface *interface, const QByteArray &function)
{
    qCDebug(dwli) << "Platform function requested:" << function;
    QFunctionPointer f = getFunction(function);

#if QTWAYLANDCLIENT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
    if (Q_UNLIKELY(!f)) {
        qCDebug(dwli) << "Function not found in cache, delegating to QtWaylandClient";
        f = static_cast<QtWaylandClient::QWaylandNativeInterface*>(interface)->QtWaylandClient::QWaylandNativeInterface::platformFunction(function);
    }
#endif
    qCDebug(dwli) << "Returning function pointer:" << (f != nullptr);
    return f;
}

bool DWaylandInterfaceHook::buildNativeSettings(QObject *object, quint32 settingWindow) {
    qCDebug(dwli) << "Building native settings for window:" << settingWindow;
    const auto &result = dXSettings->buildNativeSettings(object, settingWindow);
    qCDebug(dwli) << "Build native settings result:" << result;
    return result;
}

void DWaylandInterfaceHook::clearNativeSettings(quint32 settingWindow) {
    qCDebug(dwli) << "Clearing native settings for window:" << settingWindow;
    dXSettings->clearNativeSettings(settingWindow);
}

bool DWaylandInterfaceHook::setEnableNoTitlebar(QWindow *window, bool enable)
{
    qCDebug(dwli) << "Setting enable no titlebar:" << enable << "for window:" << window;
    if (enable) {
        if (DNoTitlebarWlWindowHelper::mapped.value(window)) {
            qCDebug(dwli) << "Window already has no titlebar helper";
            return true;
        }
        if (window->type() == Qt::Desktop) {
            qCDebug(dwli) << "Desktop window type, cannot enable no titlebar";
            return false;
        }
        window->setProperty(noTitlebar, true);
        Q_UNUSED(new DNoTitlebarWlWindowHelper(window))
        qCDebug(dwli) << "No titlebar helper created successfully";
        return true;
    } else {
        if (auto helper = DNoTitlebarWlWindowHelper::mapped.value(window)) {
            qCDebug(dwli) << "Removing no titlebar helper";
            helper->deleteLater();
        }
        window->setProperty(noTitlebar, false);
    }

    return true;
}

bool DWaylandInterfaceHook::isEnableNoTitlebar(QWindow *window)
{
    const auto &result = window->property(noTitlebar).toBool();
    qCDebug(dwli) << "Checking no titlebar enabled:" << result << "for window:" << window;
    return result;
}

void DWaylandInterfaceHook::setWindowRadius(QWindow *window, int value)
{
    qCDebug(dwli) << "Setting window radius:" << value << "for window:" << window;
    if (!window) {
        qCWarning(dwli) << "Window is null, cannot set radius";
        return;
    }
    DNoTitlebarWlWindowHelper::setWindowProperty(window, ::windowRadius, value);
}

void DWaylandInterfaceHook::setBorderColor(QWindow *window, const QColor &value)
{
    qCDebug(dwli) << "Setting border color:" << value << "for window:" << window;
    if (!window) {
        qCWarning(dwli) << "Window is null, cannot set border color";
        return;
    }
    DNoTitlebarWlWindowHelper::setWindowProperty(window, ::borderColor, value);
}

void DWaylandInterfaceHook::setShadowColor(QWindow *window, const QColor &value)
{
    qCDebug(dwli) << "Setting shadow color:" << value << "for window:" << window;
    if (!window) {
        qCWarning(dwli) << "Window is null, cannot set shadow color";
        return;
    }
    DNoTitlebarWlWindowHelper::setWindowProperty(window, ::shadowColor, value);
}

void DWaylandInterfaceHook::setShadowRadius(QWindow *window, int value)
{
    qCDebug(dwli) << "Setting shadow radius:" << value << "for window:" << window;
    if (!window) {
        qCWarning(dwli) << "Window is null, cannot set shadow radius";
        return;
    }
    DNoTitlebarWlWindowHelper::setWindowProperty(window, ::shadowRadius, value);
}

void DWaylandInterfaceHook::setShadowOffset(QWindow *window, const QPoint &value)
{
    qCDebug(dwli) << "Setting shadow offset:" << value << "for window:" << window;
    if (!window) {
        qCWarning(dwli) << "Window is null, cannot set shadow offset";
        return;
    }

    QPoint offect  = value;
    if (window->screen()) {
        const auto &ratio = window->screen()->devicePixelRatio();
        qCDebug(dwli) << "Applying device pixel ratio:" << ratio;
        offect *= ratio;
    }

    DNoTitlebarWlWindowHelper::setWindowProperty(window, ::shadowOffset, offect);
}

void DWaylandInterfaceHook::setBorderWidth(QWindow *window, int value)
{
    qCDebug(dwli) << "Setting border width:" << value << "for window:" << window;
    if (!window) {
        qCWarning(dwli) << "Window is null, cannot set border width";
        return;
    }
    DNoTitlebarWlWindowHelper::setWindowProperty(window, ::borderWidth, value);
}

void DWaylandInterfaceHook::setWindowEffect(QWindow *window, const QVariant &value)
{
    qCDebug(dwli) << "Setting window effect:" << value << "for window:" << window;
    if (!window) {
        qCWarning(dwli) << "Window is null, cannot set window effect";
        return;
    }
    DNoTitlebarWlWindowHelper::setWindowProperty(window, ::windowEffect, value);
}

void DWaylandInterfaceHook::setWindowStartUpEffect(QWindow *window, const QVariant &value)
{
    qCDebug(dwli) << "Setting window startup effect:" << value << "for window:" << window;
    if (!window) {
        qCWarning(dwli) << "Window is null, cannot set startup effect";
        return;
    }
    DNoTitlebarWlWindowHelper::setWindowProperty(window, ::windowStartUpEffect, value);
}

void DWaylandInterfaceHook::setWindowProperty(QWindow *window, const char *name, const QVariant &value)
{
    qCDebug(dwli) << "Setting window property:" << name << "=" << value << "for window:" << window;
    DNoTitlebarWlWindowHelper::setWindowProperty(window, name, value);
}

void DWaylandInterfaceHook::popupSystemWindowMenu(WId wid)
{
    qCDebug(dwli) << "Popping up system window menu for window ID:" << wid;
    DNoTitlebarWlWindowHelper::popupSystemWindowMenu(wid);
}

bool DWaylandInterfaceHook::enableDwayland(QWindow *window)
{
    qCDebug(dwli) << "Enabling dwayland for window:" << window;
    static bool xwayland = QByteArrayLiteral("wayland") == qgetenv("XDG_SESSION_TYPE")
            && !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");

    if (xwayland) {
        qCDebug(dwli) << "XWayland session detected, dwayland not supported";
        // for xwayland
        return false;
    }

    if (window->type() == Qt::Desktop) {
        qCDebug(dwli) << "Desktop window type, dwayland not supported";
        return false;
    }

    QPlatformWindow *xw = static_cast<QPlatformWindow*>(window->handle());

    if (!xw) {
        qCDebug(dwli) << "No platform window handle, setting dwayland property";
        window->setProperty(useDwayland, true);
        return true;
    }
    if (DNoTitlebarWlWindowHelper::mapped.value(window)) {
        qCDebug(dwli) << "Window already has dwayland helper";
        return true;
    }

    if (xw->isExposed()) {
        qCDebug(dwli) << "Window is exposed, cannot enable dwayland";
        return false;
    }

#ifndef USE_NEW_IMPLEMENTING
    qCDebug(dwli) << "USE_NEW_IMPLEMENTING not defined, dwayland disabled";
    return false;
#endif

    window->setProperty(useDwayland, true);
    qCDebug(dwli) << "Dwayland enabled successfully";
    // window->setProperty("_d_dwayland_TransparentBackground", window->format().hasAlpha());

    return true;
}

bool DWaylandInterfaceHook::isEnableDwayland(const QWindow *window)
{
    const auto &result = window->property(useDwayland).toBool();
    qCDebug(dwli) << "Checking dwayland enabled:" << result << "for window:" << window;
    return result;
}

void DWaylandInterfaceHook::splitWindowOnScreen(WId wid, quint32 type)
{
    qCDebug(dwli) << "Splitting window on screen, window ID:" << wid << "type:" << type;
    return splitWindowOnScreenByType(wid, 1, type);
}

void DWaylandInterfaceHook::splitWindowOnScreenByType(WId wid, quint32 position, quint32 type)
{
    qCDebug(dwli) << "Splitting window by type, window ID:" << wid << "position:" << position << "type:" << type;
    QWindow *window = fromQtWinId(wid);
    if(!window || !window->handle()) {
        qCWarning(dwli) << "Invalid window or no handle for window ID:" << wid;
        return;
    }
    // position: 15 not preview
    if (position == 15) {
        qCDebug(dwli) << "Position 15 (not preview), toggling maximized state";
        if (window->windowStates().testFlag(Qt::WindowMaximized)) {
            window->showNormal();
        } else {
            window->showMaximized();
        }
    } else {
        qCDebug(dwli) << "Setting split window property, position:" << position << "type:" << type;
        // type 1:two splitting 2:three splitting 4:four splitting
        // position enum class SplitType in kwin-dev, Left=0x1, Right=0x10, Top=0x100, Bottom=0x1000
        QVariantList value{position, type};
        DNoTitlebarWlWindowHelper::setWindowProperty(window, ::splitWindowOnScreen, value);
    }
}

bool DWaylandInterfaceHook::supportForSplittingWindow(WId wid)
{
    qCDebug(dwli) << "Checking support for splitting window, window ID:" << wid;
    return supportForSplittingWindowByType(wid, 1);
}

// screenSplittingType: 0: can't splitting, 1:two splitting, 2: four splitting(includ three splitting)
bool DWaylandInterfaceHook::supportForSplittingWindowByType(quint32 wid, quint32 screenSplittingType)
{
    qCDebug(dwli) << "Checking support for splitting window by type, window ID:" << wid << "type:" << screenSplittingType;
    QWindow *window = fromQtWinId(wid);
    if(!window || !window->handle()) {
        qCWarning(dwli) << "Invalid window or no handle for window ID:" << wid;
        return false;
    }
    DNoTitlebarWlWindowHelper::setWindowProperty(window, ::supportForSplittingWindow, false);
    const auto &result = window->property(::supportForSplittingWindow).toInt() >= screenSplittingType;
    qCDebug(dwli) << "Support for splitting window result:" << result;
    return result;
}

DPP_END_NAMESPACE
