// SPDX-FileCopyrightText: 2020 - 2022 Uniontech Software Technology Co.,Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dapplicationeventmonitor.h"

#include <QGuiApplication>
#include <QInputEvent>
#include <QLoggingCategory>

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#include <QPointingDevice>
typedef QPointingDevice QTouchDevice;
#else
#include <QTouchDevice>
#endif

#ifndef QT_DEBUG
Q_LOGGING_CATEGORY(dplatform, "dtk.qpa.platform", QtInfoMsg);
#else
Q_LOGGING_CATEGORY(dplatform, "dtk.qpa.platform");
#endif

DPP_BEGIN_NAMESPACE

DApplicationEventMonitor::DApplicationEventMonitor(QObject *parent)
    : QObject(parent)
    , m_lastInputDeviceType(None)
{
    qCDebug(dplatform) << "DApplicationEventMonitor constructor called";
    qApp->installEventFilter(this);
}

DApplicationEventMonitor::~DApplicationEventMonitor()
{
    qCDebug(dplatform) << "DApplicationEventMonitor destructor called";
}

DApplicationEventMonitor::InputDeviceType DApplicationEventMonitor::lastInputDeviceType() const
{
    qCDebug(dplatform) << "lastInputDeviceType called, current type:" << m_lastInputDeviceType;
    return m_lastInputDeviceType;
}

DApplicationEventMonitor::InputDeviceType DApplicationEventMonitor::eventType(QEvent *event)
{
    Q_ASSERT(event);

    qCDebug(dplatform) << "eventType called, event type:" << event->type();
    InputDeviceType last_input_device_type = None;

    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
    case QEvent::MouseButtonDblClick: {
        QMouseEvent *pMouseEvent = static_cast<QMouseEvent *>(event);

        if (pMouseEvent->source() == Qt::MouseEventNotSynthesized) { //由真实鼠标事件生成
            qCDebug(dplatform) << "Mouse event detected, source: MouseEventNotSynthesized";
            last_input_device_type = Mouse;
        } else {
            qCDebug(dplatform) << "Mouse event detected, source: synthesized";
        }
        break;
    }
    case QEvent::TabletPress:
    case QEvent::TabletRelease:
    case QEvent::TabletMove:
        qCDebug(dplatform) << "Tablet event detected";
        last_input_device_type = Tablet;
        break;
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
        qCDebug(dplatform) << "Keyboard event detected";
        last_input_device_type = Keyboard;
        break;
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel: {
        QTouchEvent *pTouchEvent = static_cast<QTouchEvent *>(event);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        if (pTouchEvent->device()->type() == QInputDevice::DeviceType::TouchScreen) {
#else
        if (pTouchEvent->device()->type() == QTouchDevice::TouchScreen) {
#endif
            qCDebug(dplatform) << "TouchScreen event detected";
            last_input_device_type = TouchScreen;
        } else {
            qCDebug(dplatform) << "Touch event detected, but not TouchScreen";
        }
        break;
    }
    default:
        qCDebug(dplatform) << "Unknown event type:" << event->type();
        break;
    }

    return last_input_device_type;
}

bool DApplicationEventMonitor::eventFilter(QObject *watched, QEvent *event)
{
    qCDebug(dplatform) << "eventFilter called, watched object:" << watched << "event type:" << event->type();
    auto last_input_device_type = eventType(event);

    if (last_input_device_type != None && last_input_device_type != m_lastInputDeviceType) {
        qCInfo(dplatform) << "Input device type changed from" << m_lastInputDeviceType << "to" << last_input_device_type;
        m_lastInputDeviceType = last_input_device_type;
        Q_EMIT lastInputDeviceTypeChanged();
    }

    return QObject::eventFilter(watched, event);
}

DPP_END_NAMESPACE
