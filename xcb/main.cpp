// SPDX-FileCopyrightText: 2017 - 2022 Uniontech Software Technology Co.,Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qpa/qplatformintegrationplugin.h>
#include "dplatformintegration.h"

#include <QDebug>
#include <QLoggingCategory>

#ifndef QT_DEBUG
Q_LOGGING_CATEGORY(dxcbmain, "dtk.qpa.xcb.main", QtInfoMsg);
#else
Q_LOGGING_CATEGORY(dxcbmain, "dtk.qpa.xcb.main");
#endif

DPP_USE_NAMESPACE

QT_BEGIN_NAMESPACE

class DPlatformIntegrationPlugin : public QPlatformIntegrationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformIntegrationFactoryInterface_iid FILE "dpp.json")

public:
    QPlatformIntegration *create(const QString&, const QStringList&, int &, char **) Q_DECL_OVERRIDE;
};

QPlatformIntegration* DPlatformIntegrationPlugin::create(const QString& system, const QStringList& parameters, int &argc, char **argv)
{
    qCDebug(dxcbmain) << "Creating platform integration, system:" << system << "parameters:" << parameters;
#ifdef Q_OS_LINUX
    bool loadDXcb = false;

    if (qEnvironmentVariableIsSet("D_DXCB_DISABLE")) {
        qCDebug(dxcbmain) << "DXCB disabled by environment variable";
        loadDXcb = false;
    } else if (system == "dxcb") {
        qCDebug(dxcbmain) << "DXCB explicitly requested via system parameter";
        loadDXcb = true;
    } else if (QString(qgetenv("XDG_CURRENT_DESKTOP")).toLower().startsWith("deepin") ||
               (qgetenv("XDG_CURRENT_DESKTOP") == QByteArrayLiteral("DDE"))) {
        const auto &desktop = qgetenv("XDG_CURRENT_DESKTOP");
        qCDebug(dxcbmain) << "Deepin desktop detected:" << desktop;
        loadDXcb = true;
    } else {
        qCDebug(dxcbmain) << "No DXCB conditions met, using parent integration";
    }

    qCDebug(dxcbmain) << "Loading DXCB:" << loadDXcb;
    const auto &result = loadDXcb ? new DPlatformIntegration(parameters, argc, argv)
                    : new DPlatformIntegrationParent(parameters, argc, argv);
    qCDebug(dxcbmain) << "Platform integration created successfully";
    return result;
#else
    qCDebug(dxcbmain) << "Not on Linux, returning nullptr";
    return nullptr;
#endif
}

QT_END_NAMESPACE

#include "main.moc"
