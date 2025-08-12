// SPDX-FileCopyrightText: 2022 Uniontech Software Technology Co.,Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dxsettings.h"
#include <QCoreApplication>
#include <QLoggingCategory>

#ifndef QT_DEBUG
Q_LOGGING_CATEGORY(dxsettings, "dtk.wayland.xsettings", QtInfoMsg);
#else
Q_LOGGING_CATEGORY(dxsettings, "dtk.wayland.xsettings");
#endif

DPP_BEGIN_NAMESPACE

class DXcbEventFilter : public QThread
{
public:
    DXcbEventFilter(xcb_connection_t *connection)
        : m_connection(connection)
        , m_threadProxy(new RunInThreadProxy(qApp))
    {
        qCDebug(dxsettings) << "Creating DXcbEventFilter with connection:" << connection;
        QThread::start();
    }

    void run() override {
        qCDebug(dxsettings) << "DXcbEventFilter thread started";
        xcb_generic_event_t *event;
        while (m_connection && (event = xcb_wait_for_event(m_connection))) {
            uint response_type = event->response_type & ~0x80;
            qCDebug(dxsettings) << "Received XCB event, response type:" << response_type;
            switch (response_type) {
                case XCB_PROPERTY_NOTIFY: {
                    qCDebug(dxsettings) << "Handling XCB_PROPERTY_NOTIFY event";
                    m_threadProxy->proxyCall([event]() {
                        xcb_property_notify_event_t *pn = (xcb_property_notify_event_t *)event;
                        DXcbXSettings::handlePropertyNotifyEvent(pn);
                    });
                    break;
                }

                case XCB_CLIENT_MESSAGE: {
                    qCDebug(dxsettings) << "Handling XCB_CLIENT_MESSAGE event";
                    m_threadProxy->proxyCall([event]() {
                        xcb_client_message_event_t *ev = reinterpret_cast<xcb_client_message_event_t*>(event);
                        DXcbXSettings::handleClientMessageEvent(ev);
                    });
                    break;
                }
                default: {
                    qCDebug(dxsettings) << "Unhandled XCB event type:" << response_type;
                    break;
                }
            }
        }
        qCDebug(dxsettings) << "DXcbEventFilter thread ended";
    }

private:
    xcb_connection_t *m_connection;
    RunInThreadProxy *m_threadProxy = nullptr;
};

xcb_connection_t *DXSettings::xcb_connection = nullptr;
DXcbXSettings *DXSettings::m_xsettings = nullptr;

void DXSettings::initXcbConnection()
{
    qCDebug(dxsettings) << "Initializing XCB connection";
    static bool isInit = false;

    if (isInit && xcb_connection) {
        qCDebug(dxsettings) << "XCB connection already initialized";
        return;
    }

    isInit = true;
    int primary_screen_number = 0;
    const auto &display = qgetenv("DISPLAY");
    qCDebug(dxsettings) << "Connecting to XCB display:" << display;
    xcb_connection = xcb_connect(display, &primary_screen_number);

    if (xcb_connection) {
        qCDebug(dxsettings) << "XCB connection established successfully, screen number:" << primary_screen_number;
        new DXcbEventFilter(xcb_connection);
    } else {
        qCWarning(dxsettings) << "Failed to establish XCB connection";
    }
}

bool DXSettings::buildNativeSettings(QObject *object, quint32 settingWindow)
{
    qCDebug(dxsettings) << "Building native settings for object:" << object << "window:" << settingWindow;
    QByteArray settings_property = DNativeSettings::getSettingsProperty(object);
    qCDebug(dxsettings) << "Settings property:" << settings_property;
    DXcbXSettings *settings = nullptr;
    bool global_settings = false;
    if (settingWindow || !settings_property.isEmpty()) {
        qCDebug(dxsettings) << "Creating specific XCB settings";
        settings = new DXcbXSettings(xcb_connection, settingWindow, settings_property);
    } else {
        qCDebug(dxsettings) << "Using global settings";
        global_settings = true;
        settings = globalSettings();
    }

    // 跟随object销毁
    auto native_settings = new DNativeSettings(object, settings, global_settings);

    if (!native_settings->isValid()) {
        qCWarning(dxsettings) << "Native settings is not valid, cleaning up";
        delete native_settings;
        return false;
    }

    qCDebug(dxsettings) << "Native settings built successfully";
    return true;
}

void DXSettings::clearNativeSettings(quint32 settingWindow)
{
    qCDebug(dxsettings) << "Clearing native settings for window:" << settingWindow;
#ifdef Q_OS_LINUX
    DXcbXSettings::clearSettings(settingWindow);
    qCDebug(dxsettings) << "Settings cleared for Linux";
#else
    qCDebug(dxsettings) << "Settings clearing not supported on this platform";
#endif
}

DXcbXSettings *DXSettings::globalSettings()
{
    qCDebug(dxsettings) << "Getting global settings";
    if (Q_LIKELY(m_xsettings)) {
        qCDebug(dxsettings) << "Returning existing global settings";
        return m_xsettings;
    }

    if (!xcb_connection) {
        qCDebug(dxsettings) << "XCB connection not initialized, initializing now";
        initXcbConnection();
    }
    qCDebug(dxsettings) << "Creating new global XCB settings";
    m_xsettings = new DXcbXSettings(xcb_connection);

    return m_xsettings;
}

xcb_window_t DXSettings::getOwner(xcb_connection_t *conn, int screenNumber) {
    qCDebug(dxsettings) << "Getting owner for connection:" << conn << "screen:" << screenNumber;
    const auto &result = DXcbXSettings::getOwner(conn, screenNumber);
    qCDebug(dxsettings) << "Owner window ID:" << result;
    return result;
}

DPP_END_NAMESPACE
