// SPDX-FileCopyrightText: 2017 - 2022 Uniontech Software Technology Co.,Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dnotitlebarwindowhelper_wl.h"
#include "vtablehook.h"

#define protected public
#include <QWindow>
#undef protected
#include <QMouseEvent>
#include <QGuiApplication>
#include <QStyleHints>
#include <QTimer>
#include <QMetaProperty>
#include <QScreen>
#include <qpa/qplatformwindow.h>
#include <QtWaylandClientVersion>

#define private public
#include "QtWaylandClient/private/qwaylandintegration_p.h"
#include "QtWaylandClient/private/qwaylandshellsurface_p.h"
#include "QtWaylandClient/private/qwaylandwindow_p.h"
#include "QtWaylandClient/private/qwaylandcursor_p.h"
#undef private

#include <private/qguiapplication_p.h>
#include <QLoggingCategory>

#ifndef QT_DEBUG
Q_LOGGING_CATEGORY(dnotitlebar, "dtk.wayland.notitlebar", QtInfoMsg);
#else
Q_LOGGING_CATEGORY(dnotitlebar, "dtk.wayland.notitlebar");
#endif

DPP_BEGIN_NAMESPACE

QHash<const QWindow*, DNoTitlebarWlWindowHelper*> DNoTitlebarWlWindowHelper::mapped;

DNoTitlebarWlWindowHelper::DNoTitlebarWlWindowHelper(QWindow *window)
    : QObject(window)
    , m_window(window)
{
    qCDebug(dnotitlebar) << "Creating DNoTitlebarWlWindowHelper for window:" << window;
    // 不允许设置窗口为无边框的
    if (window->flags().testFlag(Qt::FramelessWindowHint)) {
        qCDebug(dnotitlebar) << "Removing FramelessWindowHint flag";
        window->setFlag(Qt::FramelessWindowHint, false);
    }

    mapped[window] = this;
    qCDebug(dnotitlebar) << "Window helper mapped, total helpers:" << mapped.size();

    updateEnableSystemMoveFromProperty();
}

DNoTitlebarWlWindowHelper::~DNoTitlebarWlWindowHelper()
{
    qCDebug(dnotitlebar) << "Destroying DNoTitlebarWlWindowHelper for window:" << m_window;
    if (VtableHook::hasVtable(m_window)) {
        qCDebug(dnotitlebar) << "Resetting vtable for window";
        VtableHook::resetVtable(m_window);
    }

    mapped.remove(static_cast<QWindow*>(parent()));
    qCDebug(dnotitlebar) << "Window helper removed from mapping, remaining helpers:" << mapped.size();

    // TODO
    // if (m_window->handle()) { // 当本地窗口还存在时，移除设置过的窗口属性
    //     //! Utility::clearWindowProperty(m_windowID, Utility::internAtom(_DEEPIN_SCISSOR_WINDOW));
    //     DPlatformIntegration::clearNativeSettings(m_windowID);
    // }
}

void DNoTitlebarWlWindowHelper::setWindowProperty(QWindow *window, const char *name, const QVariant &value)
{
    qCDebug(dnotitlebar) << "Setting window property:" << name << "=" << value << "for window:" << window;
    const QVariant &old_value = window->property(name);

    if (old_value.isValid() && old_value == value) {
        qCDebug(dnotitlebar) << "Property value unchanged, skipping";
        return;
    }

    if (value.typeName() == QByteArray("QPainterPath")) {
        const QPainterPath &old_path = qvariant_cast<QPainterPath>(old_value);
        const QPainterPath &new_path = qvariant_cast<QPainterPath>(value);

        if (old_path == new_path) {
            qCDebug(dnotitlebar) << "PainterPath unchanged, skipping";
            return;
        }
    }

    if(!window) {
        qCWarning(dnotitlebar) << "Window is null, cannot set property";
        return;
    }

    window->setProperty(name, value);
    qCDebug(dnotitlebar) << "Property set successfully";

    if(window->handle()) {
        qCDebug(dnotitlebar) << "Window has handle, sending property to wayland";
        QtWaylandClient::QWaylandWindow *wl_window = static_cast<QtWaylandClient::QWaylandWindow *>(window->handle());

        if (wl_window->shellSurface()) {
            qCDebug(dnotitlebar) << "Sending property to shell surface";
            wl_window->sendProperty(name, value);
        } else {
            qCDebug(dnotitlebar) << "No shell surface available";
        }
    }

    if (DNoTitlebarWlWindowHelper *self = mapped.value(window)) {
        qCDebug(dnotitlebar) << "Found window helper, updating property";

        QByteArray name_array(name);
        if (!name_array.startsWith("_d_")) {
            qCDebug(dnotitlebar) << "Property name does not start with '_d_', skipping slot invocation";
            return;
        }

        // to upper
        name_array[3] = name_array.at(3) & ~0x20;

        const QByteArray slot_name = "update" + name_array.mid(3) + "FromProperty";
        qCDebug(dnotitlebar) << "Generated slot name:" << slot_name;

        if (self->metaObject()->indexOfSlot(slot_name + QByteArray("()")) < 0) {
            qCWarning(dnotitlebar) << "Slot not found:" << slot_name;
            return;
        }

        if (!QMetaObject::invokeMethod(self, slot_name.constData(), Qt::DirectConnection)) {
            qWarning() << "Failed to update property:" << slot_name;
        } else {
            qCDebug(dnotitlebar) << "Property update slot invoked successfully";
        }
    }
}

void DNoTitlebarWlWindowHelper::requestByWindowProperty(QWindow *window, const char *name)
{
    qCDebug(dnotitlebar) << "Requesting window property:" << name << "for window:" << window;
    if (window && window->handle()) {
        qCDebug(dnotitlebar) << "Window has handle, sending property request";
        QtWaylandClient::QWaylandWindow *wl_window = static_cast<QtWaylandClient::QWaylandWindow *>(window->handle());

        if (wl_window && wl_window->shellSurface()) {
            qCDebug(dnotitlebar) << "Sending property request to shell surface";
            wl_window->sendProperty(name, QVariant());
        } else {
            qCDebug(dnotitlebar) << "No shell surface available for property request";
        }
    } else {
        qCDebug(dnotitlebar) << "Window or handle not available for property request";
    }
}

void DNoTitlebarWlWindowHelper::popupSystemWindowMenu(quintptr wid)
{
    qCDebug(dnotitlebar) << "Popping up system window menu for window ID:" << wid;
    QWindow *window = fromQtWinId(wid);
    if(!window || !window->handle()) {
        qCWarning(dnotitlebar) << "Invalid window or no handle for window ID:" << wid;
        return;
    }

    QtWaylandClient::QWaylandWindow *wl_window = static_cast<QtWaylandClient::QWaylandWindow *>(window->handle());
    if (!wl_window->shellSurface()) {
        qCWarning(dnotitlebar) << "No shell surface available for window menu";
        return;
    }

    if (QtWaylandClient::QWaylandShellSurface *s = wl_window->shellSurface()) {
        qCDebug(dnotitlebar) << "Showing window menu";
        auto wl_integration = static_cast<QtWaylandClient::QWaylandIntegration *>(QGuiApplicationPrivate::platformIntegration());
        s->showWindowMenu(wl_integration->display()->defaultInputDevice());
    }
}

void DNoTitlebarWlWindowHelper::updateEnableSystemMoveFromProperty()
{
    qCDebug(dnotitlebar) << "Updating enable system move from property";
    const QVariant &v = m_window->property(enableSystemMove);

    m_enableSystemMove = !v.isValid() || v.toBool();
    qCDebug(dnotitlebar) << "System move enabled:" << m_enableSystemMove;

    if (m_enableSystemMove) {
        qCDebug(dnotitlebar) << "Hooking window event for system move";
        HookOverride(m_window, &QWindow::event, &DNoTitlebarWlWindowHelper::windowEvent);
    } else if (VtableHook::hasVtable(m_window)) {
        qCDebug(dnotitlebar) << "Resetting vtable hook for system move";
        HookReset(m_window, &QWindow::event);
    }
}

bool DNoTitlebarWlWindowHelper::windowEvent(QWindow *w, QEvent *event)
{
    qCDebug(dnotitlebar) << "Window event received, type:" << event->type() << "for window:" << w;
    DNoTitlebarWlWindowHelper *self = mapped.value(w);

    if (!self) {
        qCDebug(dnotitlebar) << "No window helper found, calling original event handler";
        return VtableHook::callOriginalFun(w, &QWindow::event, event);
    }
    // m_window 的 event 被 override 以后，在 windowEvent 里面获取到的 this 就成 m_window 了，
    // 而不是 DNoTitlebarWlWindowHelper，所以此处 windowEvent 改为 static 并传 self 进来
    {
        // get touch begin position
        static bool isTouchDown = false;
        static QPointF touchBeginPosition;
        if (event->type() == QEvent::TouchBegin) {
            qCDebug(dnotitlebar) << "Touch begin event";
            isTouchDown = true;
        }
        if (event->type() == QEvent::TouchEnd || event->type() == QEvent::MouseButtonRelease) {
            qCDebug(dnotitlebar) << "Touch/mouse release event";
            isTouchDown = false;
        }
        if (isTouchDown && event->type() == QEvent::MouseButtonPress) {
            touchBeginPosition = static_cast<QMouseEvent*>(event)->globalPos();
            qCDebug(dnotitlebar) << "Touch begin position recorded:" << touchBeginPosition;
        }
        // add some redundancy to distinguish trigger between system menu and system move
        if (event->type() == QEvent::MouseMove) {
            QPointF currentPos = static_cast<QMouseEvent*>(event)->globalPos();
            QPointF delta = touchBeginPosition  - currentPos;
            const auto &dragDistance = QGuiApplication::styleHints()->startDragDistance();
            qCDebug(dnotitlebar) << "Mouse move, delta:" << delta << "drag distance:" << dragDistance;
            if (delta.manhattanLength() < dragDistance) {
                qCDebug(dnotitlebar) << "Delta too small, calling original event handler";
                return VtableHook::callOriginalFun(w, &QWindow::event, event);
            }
        }
    }

    bool is_mouse_move = event->type() == QEvent::MouseMove && static_cast<QMouseEvent*>(event)->buttons() == Qt::LeftButton;
    qCDebug(dnotitlebar) << "Is mouse move with left button:" << is_mouse_move;

    if (event->type() == QEvent::MouseButtonRelease) {
        qCDebug(dnotitlebar) << "Mouse button release, stopping window moving";
        self->m_windowMoving = false;
    }

    if (!HookCall(w, &QWindow::event, event)) {
        qCDebug(dnotitlebar) << "Hook call failed";
        return false;
    }

    // workaround for kwin: Qt receives no release event when kwin finishes MOVE operation,
    // which makes app hang in windowMoving state. when a press happens, there's no sense of
    // keeping the moving state, we can just reset ti back to normal.
    if (event->type() == QEvent::MouseButtonPress) {
        qCDebug(dnotitlebar) << "Mouse button press, resetting window moving state";
        self->m_windowMoving = false;
    }

    if (is_mouse_move && !event->isAccepted()
            && w->geometry().contains(static_cast<QMouseEvent*>(event)->globalPos())) {
        qCDebug(dnotitlebar) << "Mouse move in window geometry, checking system move";
        if (!self->m_windowMoving && self->isEnableSystemMove()) {
            qCDebug(dnotitlebar) << "Starting window move";
            self->m_windowMoving = true;

            event->accept();
            startMoveWindow(w);
        }
    }

    return true;
}

bool DNoTitlebarWlWindowHelper::isEnableSystemMove(/*quint32 winId*/)
{
    qCDebug(dnotitlebar) << "Checking system move enabled:" << m_enableSystemMove;
    return m_enableSystemMove;
}

void DNoTitlebarWlWindowHelper::startMoveWindow(QWindow *window)
{
    qCDebug(dnotitlebar) << "Starting window move for window:" << window;
    // QWaylandWindow::startSystemMove
    if (window && window->handle()) {
        qCDebug(dnotitlebar) << "Calling startSystemMove on wayland window";
#if QTWAYLANDCLIENT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                static_cast<QtWaylandClient::QWaylandWindow *>(window->handle())->startSystemMove(QCursor::pos());
#else
                static_cast<QtWaylandClient::QWaylandWindow *>(window->handle())->startSystemMove();
#endif
    } else {
        qCWarning(dnotitlebar) << "Window or handle not available for system move";
    }
}

DPP_END_NAMESPACE
