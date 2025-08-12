// SPDX-FileCopyrightText: 2020 - 2022 Uniontech Software Technology Co.,Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dplatforminputcontexthook.h"

#include "global.h"
#include <qpa/qplatforminputcontext.h>
#include <QLoggingCategory>

#ifndef QT_DEBUG
Q_LOGGING_CATEGORY(dxcb, "dtk.qpa.xcb", QtInfoMsg);
#else
Q_LOGGING_CATEGORY(dxcb, "dtk.qpa.xcb");
#endif

DPP_BEGIN_NAMESPACE

void DPlatformInputContextHook::showInputPanel(QPlatformInputContext *inputContext)
{
    qCDebug(dxcb) << "showInputPanel called";
    Q_UNUSED(inputContext)
    instance()->setImActive(true);
}

void DPlatformInputContextHook::hideInputPanel(QPlatformInputContext *inputContext)
{
    qCDebug(dxcb) << "hideInputPanel called";
    Q_UNUSED(inputContext)
    instance()->setImActive(false);
}

bool DPlatformInputContextHook::isInputPanelVisible(QPlatformInputContext *inputContext)
{
    qCDebug(dxcb) << "isInputPanelVisible called";
    Q_UNUSED(inputContext)
    bool result = instance()->imActive();
    qCDebug(dxcb) << "Input panel visible:" << result;
    return result;
}

QRectF DPlatformInputContextHook::keyboardRect(QPlatformInputContext *inputContext)
{
    qCDebug(dxcb) << "keyboardRect called";
    Q_UNUSED(inputContext)
    QRectF result = instance()->geometry();
    qCDebug(dxcb) << "Keyboard geometry:" << result;
    return result;
}

Q_GLOBAL_STATIC_WITH_ARGS(ComDeepinImInterface, __imInterface,
                          (QString("com.deepin.im"), QString("/com/deepin/im"), QDBusConnection::sessionBus()))

ComDeepinImInterface* DPlatformInputContextHook::instance()
{
    qCDebug(dxcb) << "instance called";
    return __imInterface;
}

DPP_END_NAMESPACE
