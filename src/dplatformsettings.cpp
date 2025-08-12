// SPDX-FileCopyrightText: 2020 - 2022 Uniontech Software Technology Co.,Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dplatformsettings.h"
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(dplatform)

DPP_BEGIN_NAMESPACE

void DPlatformSettings::registerCallback(DPlatformSettings::PropertyChangeFunc func, void *handle)
{
    qCDebug(dplatform) << "registerCallback called, handle:" << handle;
    Callback callback = { func, handle };
    callback_links.push_back(callback);
}

void DPlatformSettings::removeCallbackForHandle(void *handle)
{
    qCDebug(dplatform) << "removeCallbackForHandle called, handle:" << handle;
    auto isCallbackForHandle = [handle](const Callback &cb) { return cb.handle == handle; };
    callback_links.erase(std::remove_if(callback_links.begin(), callback_links.end(), isCallbackForHandle));
}

void DPlatformSettings::registerSignalCallback(DPlatformSettings::SignalFunc func, void *handle)
{
    qCDebug(dplatform) << "registerSignalCallback called, handle:" << handle;
    SignalCallback callback = { func, handle };
    signal_callback_links.push_back(callback);
}

void DPlatformSettings::removeSignalCallback(void *handle)
{
    qCDebug(dplatform) << "removeSignalCallback called, handle:" << handle;
    auto isCallbackForHandle = [handle](const SignalCallback &cb) { return cb.handle == handle; };
    signal_callback_links.erase(std::remove_if(signal_callback_links.begin(), signal_callback_links.end(), isCallbackForHandle));
}

void DPlatformSettings::handlePropertyChanged(const QByteArray &property, const QVariant &value)
{
    qCDebug(dplatform) << "handlePropertyChanged called, property:" << property << "value:" << value;
    for (auto callback : callback_links) {
        callback.func(property, value, callback.handle);
    }
}

void DPlatformSettings::handleNotify(const QByteArray &signal, qint32 data1, qint32 data2)
{
    qCDebug(dplatform) << "handleNotify called, signal:" << signal << "data1:" << data1 << "data2:" << data2;
    for (auto callback : signal_callback_links) {
        callback.func(signal, data1, data2, callback.handle);
    }
}

DPP_END_NAMESPACE
