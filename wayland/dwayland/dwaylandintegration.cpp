// SPDX-FileCopyrightText: 2017 - 2022 Uniontech Software Technology Co.,Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dwaylandintegration.h"
#include "dwaylandinterfacehook.h"
#include "vtablehook.h"
#include "dxsettings.h"

#include <wayland-cursor.h>

#define private public
#include <QtWaylandClient/private/qwaylanddisplay_p.h>
#include <QtWaylandClient/private/qwaylandscreen_p.h>
#include <QtWaylandClient/private/qwaylandcursor_p.h>
#include <QtWaylandClient/private/qwaylandinputdevice_p.h>
#undef private
#include <QtWaylandClientVersion>

#include <QDebug>
#include <QTimer>
#include <QLoggingCategory>

#include <qpa/qplatformnativeinterface.h>
#include <private/qguiapplication_p.h>

#ifndef QT_DEBUG
Q_LOGGING_CATEGORY(dwayland, "dtk.wayland.integration", QtInfoMsg);
#else
Q_LOGGING_CATEGORY(dwayland, "dtk.wayland.integration");
#endif

DPP_BEGIN_NAMESPACE

#define XSETTINGS_DOUBLE_CLICK_TIME QByteArrayLiteral("Net/DoubleClickTime")
#define XSETTINGS_CURSOR_THEME_NAME QByteArrayLiteral("Gtk/CursorThemeName")
#define XSETTINGS_PRIMARY_MONITOR_RECT QByteArrayLiteral("DDE/PrimaryMonitorRect")

enum XSettingType:quint64 {
    Gtk_CursorThemeName,
    Dde_PrimaryMonitorRect
};

static void overrideChangeCursor(QPlatformCursor *cursorHandle, QCursor * cursor, QWindow * widget)
{
    qCDebug(dwayland) << "Overriding change cursor for widget:" << widget;
    if (!(widget && widget->handle())) {
        qCDebug(dwayland) << "Widget or handle not available";
        return;
    }

    if (widget->property(disableOverrideCursor).toBool()) {
        qCDebug(dwayland) << "Cursor override disabled for widget";
        return;
    }

    // qtwayland里面判断了，如果没有设置环境变量，就用默认大小32, 这里通过设在环境变量将大小设置我们需要的24
    static bool xcursorSizeIsSet = qEnvironmentVariableIsSet("XCURSOR_SIZE");
    if (!xcursorSizeIsSet) {
        const auto &size = 24 * qApp->devicePixelRatio();
        qCDebug(dwayland) << "Setting XCURSOR_SIZE environment variable to:" << size;
        qputenv("XCURSOR_SIZE", QByteArray::number(size));
    }

    HookCall(cursorHandle, &QPlatformCursor::changeCursor, cursor, widget);

    // 调用changeCursor后，控制中心窗口的光标没有及时更新，这里强制刷新一下
    qCDebug(dwayland) << "Updating cursor for all input devices";
    for (auto *inputDevice : DWaylandIntegration::instance()->display()->inputDevices()) {
        if (inputDevice->pointer()) {
            qCDebug(dwayland) << "Updating cursor for input device pointer";
            inputDevice->pointer()->updateCursor();
        }
    }
}

static void onXSettingsChanged(xcb_connection_t *connection, const QByteArray &name, const QVariant &property, void *handle)
{
    Q_UNUSED(connection)
    Q_UNUSED(property)
    Q_UNUSED(name)

    quint64 type = reinterpret_cast<quint64>(handle);
    qCDebug(dwayland) << "XSettings changed, type:" << type << "name:" << name;

    switch (type) {
    case Gtk_CursorThemeName:
        qCDebug(dwayland) << "Handling cursor theme name change";
        const QByteArray &cursor_name = dXSettings->globalSettings()->setting(name).toByteArray();
        qCDebug(dwayland) << "New cursor theme name:" << cursor_name;

#if QTWAYLANDCLIENT_VERSION < QT_VERSION_CHECK(5, 13, 0)
        const auto &cursor_map = DWaylandIntegration::instance()->display()->mCursorThemesBySize;
#elif QTWAYLANDCLIENT_VERSION < QT_VERSION_CHECK(5, 16, 0)
        const auto &cursor_map = DWaylandIntegration::instance()->display()->mCursorThemes;
#endif
        // 处理光标主题变化
        qCDebug(dwayland) << "Processing cursor theme change for" << cursor_map.size() << "cursor themes";
        for (auto cursor = cursor_map.constBegin(); cursor != cursor_map.constEnd(); ++cursor) {
            QtWaylandClient::QWaylandCursorTheme *ct = cursor.value();
            // 根据大小记载新的主题

#if QTWAYLANDCLIENT_VERSION < QT_VERSION_CHECK(5, 13, 0)
            auto theme = wl_cursor_theme_load(cursor_name.constData(), cursor.key(), DWaylandIntegration::instance()->display()->shm()->object());
#elif QTWAYLANDCLIENT_VERSION < QT_VERSION_CHECK(5, 16, 0)
            auto theme = wl_cursor_theme_load(cursor_name.constData(), cursor.key().second, DWaylandIntegration::instance()->display()->shm()->object());
#endif
            // 如果新主题创建失败则不更新数据
            if (!theme) {
                qCWarning(dwayland) << "Failed to load cursor theme:" << cursor_name;
                continue;
            }

            qCDebug(dwayland) << "Cursor theme loaded successfully";
            // 先尝试销毁旧主题
            if (ct->m_theme) {
                qCDebug(dwayland) << "Destroying old cursor theme";
                wl_cursor_theme_destroy(ct->m_theme);
            }

            // 清理缓存数据
            ct->m_cursors.clear();
            ct->m_theme = theme;
        }

        // 更新窗口光标
        qCDebug(dwayland) << "Updating cursor for all windows";
        for (auto s : DWaylandIntegration::instance()->display()->screens()) {
            for (QWindow *w : s->windows()) {
                QCursor cursor = w->cursor();
                // 为窗口重新设置光标
                s->cursor()->changeCursor(&cursor, w);
            }
        }

        break;

    }
}

static void onPrimaryRectChanged(xcb_connection_t *connection, const QByteArray &name, const QVariant &property, void *handle)
{
    Q_UNUSED(connection)
    Q_UNUSED(property)

    quint64 type = reinterpret_cast<quint64>(handle);
    qCDebug(dwayland) << "Primary rect changed, type:" << type << "name:" << name;
    switch (type) {
    case Dde_PrimaryMonitorRect:
    {
        qCDebug(dwayland) << "Handling primary monitor rect change";
        auto screens = DWaylandIntegration::instance()->display()->screens();
        const QString &primaryScreenRect = dXSettings->globalSettings()->setting(name).toString();
        qCDebug(dwayland) << "Primary screen rect from xsettings:" << primaryScreenRect;
        auto list = primaryScreenRect.split('-');
        if (list.size() != 4) {
            qCWarning(dwayland) << "Invalid primary screen rect format:" << primaryScreenRect;
            return;
        }

        QRect xsettingsRect(list.at(0).toInt(), list.at(1).toInt(), list.at(2).toInt(), list.at(3).toInt());
        qCDebug(dwayland) << "Parsed xsettings rect:" << xsettingsRect;

        qDebug() << "primary rect from xsettings:" << primaryScreenRect << ", key:" << name;
        for (auto s : screens) {
            qDebug() << "screen info:"
                     << s->screen()->name() << s->screen()->model() << s->screen()->geometry() << s->geometry()
                     << s->name() << s->model() << s->screen()->geometry() << s->geometry();
        }

        // 设置新的主屏
        QtWaylandClient::QWaylandScreen *primaryScreen = nullptr;

        // 坐标一致，认定为主屏
        for (auto screen : screens) {
            if (screen->geometry().topLeft() == xsettingsRect.topLeft() && screen->screen() != qApp->primaryScreen()) {
                primaryScreen = screen;
                qCDebug(dwayland) << "Found primary screen:" << primaryScreen->name() << primaryScreen->geometry();
                break;
            }
        }

        if (primaryScreen) {
            qCDebug(dwayland) << "Setting new primary screen";
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
            QWindowSystemInterface::handlePrimaryScreenChanged(primaryScreen);
#else
            DWaylandIntegration::instance()->setPrimaryScreen(primaryScreen);
#endif
        } else {
            qCDebug(dwayland) << "No suitable primary screen found";
        }

        break;
    }
    default:
        qCDebug(dwayland) << "Unknown primary rect change type:" << type;
        break;
    }
    qDebug() << "primary screen info:" << QGuiApplication::primaryScreen()->name() << QGuiApplication::primaryScreen()->model() << QGuiApplication::primaryScreen()->geometry();
}

DWaylandIntegration::DWaylandIntegration()
{
    qCDebug(dwayland) << "Creating DWaylandIntegration";
    dXSettings->initXcbConnection();
}

void DWaylandIntegration::initialize()
{
    qCDebug(dwayland) << "Initializing DWaylandIntegration";
    // 由于Qt代码中可能写死了判断是不是wayland平台，所以只能伪装成是wayland
    if (qgetenv("DXCB_FAKE_PLATFORM_NAME_XCB") != "0") {
        qCDebug(dwayland) << "Setting platform name to wayland";
        *QGuiApplicationPrivate::platform_name = "wayland";
    }
    qApp->setProperty("_d_isDwayland", true);

    QWaylandIntegration::initialize();
    qCDebug(dwayland) << "QWaylandIntegration initialized";
    
    // 覆盖wayland的平台函数接口，用于向上层返回一些必要的与平台相关的函数供其使用
    qCDebug(dwayland) << "Hooking platform function interface";
    HookOverride(nativeInterface(), &QPlatformNativeInterface::platformFunction, &DWaylandInterfaceHook::platformFunction);
    
    // hook qtwayland的改变光标的函数，用于设置环境变量并及时更新
    qCDebug(dwayland) << "Hooking cursor change for all screens";
    for (QScreen *screen : qApp->screens()) {
        //在没有屏幕的时候，qtwayland会创建一个虚拟屏幕，此时不能调用screen->handle()->cursor()
        if (screen && screen->handle() && screen->handle()->cursor()) {
            qCDebug(dwayland) << "Hooking cursor for screen:" << screen->name();
             HookOverride(screen->handle()->cursor(), &QPlatformCursor::changeCursor, &overrideChangeCursor);
        }
    }
    
    // 监听xsettings的信号，用于更新程序状态（如更新光标主题）
    qCDebug(dwayland) << "Registering XSettings callbacks";
    dXSettings->globalSettings()->registerCallbackForProperty(XSETTINGS_CURSOR_THEME_NAME, onXSettingsChanged, reinterpret_cast<void*>(XSettingType::Gtk_CursorThemeName));

    // 增加rect的属性，保存主屏的具体坐标，不依靠其name判断(根据name查找对应的屏幕时概率性出错，根据主屏的rect确定哪一个QScreen才是主屏)
    dXSettings->globalSettings()->registerCallbackForProperty(XSETTINGS_PRIMARY_MONITOR_RECT, onPrimaryRectChanged, reinterpret_cast<void*>(XSettingType::Dde_PrimaryMonitorRect));

    //初始化时应该设一次主屏，防止应用启动时主屏闪变
    qCDebug(dwayland) << "Initializing primary screen";
    onPrimaryRectChanged(nullptr, XSETTINGS_PRIMARY_MONITOR_RECT, QVariant(), reinterpret_cast<void*>(XSettingType::Dde_PrimaryMonitorRect));

    QTimer *m_delayTimer = new QTimer;
    m_delayTimer->setInterval(10);
    m_delayTimer->setSingleShot(true);
    QObject::connect(qApp, &QGuiApplication::aboutToQuit, m_delayTimer, &QObject::deleteLater);

    QObject::connect(m_delayTimer, &QTimer::timeout, []{
        qCDebug(dwayland) << "Delay timer timeout, updating primary screen";
        onPrimaryRectChanged(nullptr, XSETTINGS_PRIMARY_MONITOR_RECT, QVariant(), reinterpret_cast<void*>(XSettingType::Dde_PrimaryMonitorRect));
    });

    // 显示器信息发生变化时，刷新主屏信息
    auto handleScreenInfoChanged = [ = ](QScreen *s) {
        qCDebug(dwayland) << "Screen info changed for screen:" << s->name();
#define HANDLE_SCREEN_SIGNAL(signal) \
    QObject::connect(s, signal, m_delayTimer, static_cast<void (QTimer::*)()>(&QTimer::start));

        HANDLE_SCREEN_SIGNAL(&QScreen::geometryChanged);
        HANDLE_SCREEN_SIGNAL(&QScreen::availableGeometryChanged);
        HANDLE_SCREEN_SIGNAL(&QScreen::physicalSizeChanged);
        HANDLE_SCREEN_SIGNAL(&QScreen::physicalDotsPerInchChanged);
        HANDLE_SCREEN_SIGNAL(&QScreen::logicalDotsPerInchChanged);
        HANDLE_SCREEN_SIGNAL(&QScreen::virtualGeometryChanged);
        HANDLE_SCREEN_SIGNAL(&QScreen::primaryOrientationChanged);
        HANDLE_SCREEN_SIGNAL(&QScreen::orientationChanged);
        HANDLE_SCREEN_SIGNAL(&QScreen::refreshRateChanged);

        m_delayTimer->start();
    };

    qCDebug(dwayland) << "Setting up screen change handlers for" << qApp->screens().size() << "screens";
    for (auto s : qApp->screens()) {
        handleScreenInfoChanged(s);
    }

    QObject::connect(qApp, &QGuiApplication::screenAdded, handleScreenInfoChanged);
    QObject::connect(qApp, &QGuiApplication::screenAdded, m_delayTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    qCDebug(dwayland) << "DWaylandIntegration initialization completed";
}

QStringList DWaylandIntegration::themeNames() const
{
    qCDebug(dwayland) << "Getting theme names";
    auto list = QWaylandIntegration::themeNames();
    const QByteArray desktop_session = qgetenv("DESKTOP_SESSION");
    qCDebug(dwayland) << "Desktop session:" << desktop_session;

    // 在lightdm环境中，无此环境变量。默认使用deepin平台主题
    if (desktop_session.isEmpty() || desktop_session == "deepin") {
        qCDebug(dwayland) << "Prepending deepin theme to list";
        list.prepend("deepin");
    }

    qCDebug(dwayland) << "Theme names:" << list;
    return list;
}

#define GET_VALID_XSETTINGS(key) { \
    auto value = dXSettings->globalSettings()->setting(key); \
    if (value.isValid()) return value; \
}

QVariant DWaylandIntegration::styleHint(QPlatformIntegration::StyleHint hint) const
{
    qCDebug(dwayland) << "Getting style hint:" << hint;
#ifdef Q_OS_LINUX
    switch ((int)hint) {
    case MouseDoubleClickInterval:
        qCDebug(dwayland) << "Getting mouse double click interval from XSettings";
        GET_VALID_XSETTINGS(XSETTINGS_DOUBLE_CLICK_TIME);
        break;
    case ShowShortcutsInContextMenus:
        qCDebug(dwayland) << "Returning false for ShowShortcutsInContextMenus";
        return false;
    default:
        qCDebug(dwayland) << "Using default style hint handling";
        break;
    }
#endif

    const auto &result = QtWaylandClient::QWaylandIntegration::styleHint(hint);
    qCDebug(dwayland) << "Style hint result:" << result;
    return result;
}

#if QT_VERSION < QT_VERSION_CHECK(5, 12, 0)
void DWaylandIntegration::setPrimaryScreen(QPlatformScreen *newPrimary)
{
    qCDebug(dwayland) << "Setting primary screen:" << newPrimary;
    return QPlatformIntegration::setPrimaryScreen(newPrimary);
}
#endif

DPP_END_NAMESPACE
