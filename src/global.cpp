// SPDX-FileCopyrightText: 2017 - 2022 Uniontech Software Technology Co.,Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "global.h"
#include <QWindow>
#include <QGuiApplication>
#include <QLoggingCategory>

#ifndef QT_DEBUG
Q_LOGGING_CATEGORY(dplatform, "dtk.qpa.platform", QtInfoMsg);
#else
Q_LOGGING_CATEGORY(dplatform, "dtk.qpa.platform");
#endif

QWindow * fromQtWinId(WId id) {
    qCDebug(dplatform) << "fromQtWinId called, id:" << id;
    QWindow *window = nullptr;

    for (auto win : qApp->allWindows()) {
        if (win->winId() == id) {
            qCDebug(dplatform) << "Found window for id:" << id;
            window = win;
            break;
        }
    }
    if (!window) {
        qCDebug(dplatform) << "No window found for id:" << id;
    }
    return window;
};

DPP_BEGIN_NAMESPACE

RunInThreadProxy::RunInThreadProxy(QObject *parent)
    : QObject(parent)
{
    qCDebug(dplatform) << "RunInThreadProxy constructor called";
}

void RunInThreadProxy::proxyCall(FunctionType func)
{
    qCDebug(dplatform) << "proxyCall called";
    QObject *receiver = parent();
    if (!receiver) {
        qCDebug(dplatform) << "No parent, using qApp as receiver";
        receiver = qApp;
    }

    QObject scope;
    connect(&scope, &QObject::destroyed, receiver, [func]() {
        qCDebug(dplatform) << "Executing proxy function";
        (func)();
    }, Qt::QueuedConnection);
}

DPP_END_NAMESPACE
