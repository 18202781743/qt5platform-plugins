// SPDX-FileCopyrightText: 2017 - 2022 Uniontech Software Technology Co.,Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dxcbwmsupport.h"
#include "dplatformintegration.h"
#include "utility.h"
#include "dframewindow.h"

#include "qxcbconnection.h"
#define private public
#include "qxcbscreen.h"
#undef private
#include "qxcbwindow.h"
#include "3rdparty/clientwin.h"
#include <QLoggingCategory>

#ifndef QT_DEBUG
Q_LOGGING_CATEGORY(dxcbwm, "dtk.qpa.xcb.wm", QtInfoMsg);
#else
Q_LOGGING_CATEGORY(dxcbwm, "dtk.qpa.xcb.wm");
#endif

DPP_BEGIN_NAMESPACE

class _DXcbWMSupport : public DXcbWMSupport {};

Q_GLOBAL_STATIC(_DXcbWMSupport, globalXWMS)

DXcbWMSupport::DXcbWMSupport()
{
    qCDebug(dxcbwm) << "Creating DXcbWMSupport";
    updateWMName(false);

    connect(this, &DXcbWMSupport::windowMotifWMHintsChanged, this, [this] (quint32 winId) {
        qCDebug(dxcbwm) << "Window motif WM hints changed for window ID:" << winId;
        for (const DFrameWindow *frame : DFrameWindow::frameWindowList) {
            if (frame->m_contentWindow && frame->m_contentWindow->handle()
                    && static_cast<QXcbWindow*>(frame->m_contentWindow->handle())->QXcbWindow::winId() == winId) {
                qCDebug(dxcbwm) << "Found matching frame window, emitting signal";
                if (frame->handle())
                    emit windowMotifWMHintsChanged(frame->handle()->winId());
                break;
            }
        }
    });
}

void DXcbWMSupport::updateWMName(bool emitSignal)
{
    qCDebug(dxcbwm) << "Updating WM name, emit signal:" << emitSignal;
    _net_wm_deepin_blur_region_rounded_atom = Utility::internAtom(QT_STRINGIFY(_NET_WM_DEEPIN_BLUR_REGION_ROUNDED), false);
    _net_wm_deepin_blur_region_mask = Utility::internAtom(QT_STRINGIFY(_NET_WM_DEEPIN_BLUR_REGION_MASK), false);
    _kde_net_wm_blur_rehind_region_atom = Utility::internAtom(QT_STRINGIFY(_KDE_NET_WM_BLUR_BEHIND_REGION), false);
    _deepin_wallpaper = Utility::internAtom(QT_STRINGIFY(_DEEPIN_WALLPAPER), false);
    _deepin_wallpaper_shared_key = Utility::internAtom(QT_STRINGIFY(_DEEPIN_WALLPAPER_SHARED_MEMORY), false);
    _deepin_no_titlebar = Utility::internAtom(QT_STRINGIFY(_DEEPIN_NO_TITLEBAR), false);
    _deepin_scissor_window = Utility::internAtom(QT_STRINGIFY(_DEEPIN_SCISSOR_WINDOW), false);

    m_wmName.clear();

    xcb_connection_t *xcb_connection = DPlatformIntegration::xcbConnection()->xcb_connection();
    xcb_window_t root = DPlatformIntegration::xcbConnection()->primaryScreen()->root();
    qCDebug(dxcbwm) << "Getting WM name from root window:" << root;

    xcb_get_property_reply_t *reply =
        xcb_get_property_reply(xcb_connection,
            xcb_get_property_unchecked(xcb_connection, false, root,
                             DPlatformIntegration::xcbConnection()->atom(QXcbAtom::D_QXCBATOM_WRAPPER(_NET_SUPPORTING_WM_CHECK)),
                             XCB_ATOM_WINDOW, 0, 1024), NULL);

    if (reply && reply->format == 32 && reply->type == XCB_ATOM_WINDOW) {
        xcb_window_t windowManager = *((xcb_window_t *)xcb_get_property_value(reply));
        qCDebug(dxcbwm) << "Found window manager window:" << windowManager;

        if (windowManager != XCB_WINDOW_NONE) {
            xcb_get_property_reply_t *windowManagerReply =
                xcb_get_property_reply(xcb_connection,
                    xcb_get_property_unchecked(xcb_connection, false, windowManager,
                                     DPlatformIntegration::xcbConnection()->atom(QXcbAtom::D_QXCBATOM_WRAPPER(_NET_WM_NAME)),
                                     DPlatformIntegration::xcbConnection()->atom(QXcbAtom::D_QXCBATOM_WRAPPER(UTF8_STRING)), 0, 1024), NULL);
            if (windowManagerReply && windowManagerReply->format == 8
                    && windowManagerReply->type == DPlatformIntegration::xcbConnection()->atom(QXcbAtom::D_QXCBATOM_WRAPPER(UTF8_STRING))) {
                m_wmName = QString::fromUtf8((const char *)xcb_get_property_value(windowManagerReply), xcb_get_property_value_length(windowManagerReply));
                qCDebug(dxcbwm) << "WM name retrieved:" << m_wmName;
            } else {
                qCDebug(dxcbwm) << "Failed to get WM name from window manager";
            }

            free(windowManagerReply);
        } else {
            qCDebug(dxcbwm) << "No window manager window found";
        }
    } else {
        qCDebug(dxcbwm) << "Failed to get window manager check property";
    }
    free(reply);

    m_isDeepinWM = (m_wmName == QStringLiteral("Mutter(DeepinGala)"));
    m_isKwin = !m_isDeepinWM && (m_wmName == QStringLiteral("KWin"));
    qCDebug(dxcbwm) << "WM type - DeepinWM:" << m_isDeepinWM << "KWin:" << m_isKwin;

    updateHasComposite();
    updateNetWMAtoms();
    updateRootWindowProperties();

    if (emitSignal) {
        qCDebug(dxcbwm) << "Emitting window manager changed signal";
        emit windowManagerChanged();
    }
}

void DXcbWMSupport::updateNetWMAtoms()
{
    qCDebug(dxcbwm) << "Updating NetWM atoms";
    net_wm_atoms.clear();

    xcb_window_t root = DPlatformIntegration::xcbConnection()->primaryScreen()->root();
    int offset = 0;
    int remaining = 0;
    xcb_connection_t *xcb_connection = DPlatformIntegration::xcbConnection()->xcb_connection();

    do {
        xcb_get_property_cookie_t cookie = xcb_get_property(xcb_connection, false, root,
                                                            DPlatformIntegration::xcbConnection()->atom(QXcbAtom::D_QXCBATOM_WRAPPER(_NET_SUPPORTED)),
                                                            XCB_ATOM_ATOM, offset, 1024);
        xcb_get_property_reply_t *reply = xcb_get_property_reply(xcb_connection, cookie, NULL);
        if (!reply) {
            qCDebug(dxcbwm) << "No reply for NetWM atoms property";
            break;
        }

        remaining = 0;

        if (reply->type == XCB_ATOM_ATOM && reply->format == 32) {
            int len = xcb_get_property_value_length(reply)/sizeof(xcb_atom_t);
            xcb_atom_t *atoms = (xcb_atom_t *)xcb_get_property_value(reply);
            int s = net_wm_atoms.size();
            net_wm_atoms.resize(s + len);
            memcpy(net_wm_atoms.data() + s, atoms, len*sizeof(xcb_atom_t));
            qCDebug(dxcbwm) << "Added" << len << "NetWM atoms, total:" << net_wm_atoms.size();

            remaining = reply->bytes_after;
            offset += len;
        } else {
            qCDebug(dxcbwm) << "Invalid NetWM atoms property format";
        }

        free(reply);
    } while (remaining > 0);

    qCDebug(dxcbwm) << "NetWM atoms update completed, total atoms:" << net_wm_atoms.size();
    updateHasBlurWindow();
    updateHasNoTitlebar();
    updateHasScissorWindow();
    updateWallpaperEffect();
}

void DXcbWMSupport::updateRootWindowProperties()
{
    qCDebug(dxcbwm) << "Updating root window properties";
    root_window_properties.clear();

    xcb_window_t root = DPlatformIntegration::xcbConnection()->primaryScreen()->root();
    xcb_connection_t *xcb_connection = DPlatformIntegration::xcbConnection()->xcb_connection();

    xcb_list_properties_cookie_t cookie = xcb_list_properties(xcb_connection, root);
    xcb_list_properties_reply_t *reply = xcb_list_properties_reply(xcb_connection, cookie, NULL);

    if (!reply) {
        qCDebug(dxcbwm) << "Failed to get root window properties";
        return;
    }

    int len = xcb_list_properties_atoms_length(reply);
    xcb_atom_t *atoms = (xcb_atom_t *)xcb_list_properties_atoms(reply);
    root_window_properties.resize(len);
    memcpy(root_window_properties.data(), atoms, len * sizeof(xcb_atom_t));
    qCDebug(dxcbwm) << "Root window properties updated, count:" << len;

    free(reply);

    updateHasBlurWindow();
}

void DXcbWMSupport::updateHasBlurWindow()
{
    qCDebug(dxcbwm) << "Updating has blur window";
    bool hasBlurWindow((m_isDeepinWM && isSupportedByWM(_net_wm_deepin_blur_region_rounded_atom))
                       || (m_isKwin && isContainsForRootWindow(_kde_net_wm_blur_rehind_region_atom)));
    // 当窗口visual不支持alpha通道时，也等价于不支持窗口背景模糊
    hasBlurWindow = hasBlurWindow && getHasWindowAlpha();
    qCDebug(dxcbwm) << "Has blur window:" << hasBlurWindow << "DeepinWM:" << m_isDeepinWM << "KWin:" << m_isKwin;

    if (m_hasBlurWindow == hasBlurWindow) {
        qCDebug(dxcbwm) << "Blur window state unchanged";
        return;
    }

    m_hasBlurWindow = hasBlurWindow;
    qCDebug(dxcbwm) << "Blur window state changed to:" << hasBlurWindow;

    emit hasBlurWindowChanged(hasBlurWindow);
}

void DXcbWMSupport::updateHasComposite()
{
    qCDebug(dxcbwm) << "Updating has composite";
    bool hasComposite;

    xcb_connection_t *xcb_connection = DPlatformIntegration::xcbConnection()->xcb_connection();

    auto atom = Utility::internAtom("_NET_KDE_COMPOSITE_TOGGLING");
    xcb_window_t root = DPlatformIntegration::xcbConnection()->primaryScreen()->root();

    //stage1: check if _NET_KDE_COMPOSITE_TOGGLING is supported
    xcb_get_property_reply_t *reply = xcb_get_property_reply(xcb_connection,
            xcb_get_property_unchecked(xcb_connection, false, root, atom, atom, 0, 1), NULL);
    if (reply && reply->type != XCB_NONE) {
        int value = 0;
        if (reply->type == atom && reply->format == 8) {
            value = *(int*)xcb_get_property_value(reply);
        }

        hasComposite = value == 1;
        qCDebug(dxcbwm) << "Composite toggling value:" << value << "has composite:" << hasComposite;
        free(reply);

        // 及时更新Qt中记录的值，KWin在关闭窗口合成的一段时间内（2S）并未释放相关的selection owner
        // 因此会导致Qt中的值在某个阶段与真实状态不匹配，用到QX11Info::isCompositingManagerRunning()
        // 的地方会出现问题，如drag窗口
        DPlatformIntegration::xcbConnection()->primaryVirtualDesktop()->m_compositingActive = hasComposite;
    } else {
        qCDebug(dxcbwm) << "Composite toggling not supported, checking selection owner";
        //stage2: fallback to check selection owner
        xcb_get_selection_owner_cookie_t cookit = xcb_get_selection_owner(xcb_connection, DPlatformIntegration::xcbConnection()->atom(QXcbAtom::D_QXCBATOM_WRAPPER(_NET_WM_CM_S0)));
        xcb_get_selection_owner_reply_t *reply = xcb_get_selection_owner_reply(xcb_connection, cookit, NULL);
        if (!reply) {
            qCDebug(dxcbwm) << "Failed to get selection owner";
            return;
        }

        hasComposite = reply->owner != XCB_NONE;
        qCDebug(dxcbwm) << "Selection owner:" << reply->owner << "has composite:" << hasComposite;

        free(reply);
    }

    if (m_hasComposite == hasComposite) {
        qCDebug(dxcbwm) << "Composite state unchanged";
        return;
    }

    m_hasComposite = hasComposite;
    qCDebug(dxcbwm) << "Composite state changed to:" << hasComposite;

    emit hasCompositeChanged(hasComposite);
}

void DXcbWMSupport::updateHasNoTitlebar()
{
    qCDebug(dxcbwm) << "Updating has no titlebar";
    bool hasNoTitlebar(net_wm_atoms.contains(_deepin_no_titlebar));
    qCDebug(dxcbwm) << "Has no titlebar:" << hasNoTitlebar;

    if (m_hasNoTitlebar == hasNoTitlebar) {
        qCDebug(dxcbwm) << "No titlebar state unchanged";
        return;
    }

    m_hasNoTitlebar = hasNoTitlebar;
    qCDebug(dxcbwm) << "No titlebar state changed to:" << hasNoTitlebar;

    emit hasNoTitlebarChanged(m_hasNoTitlebar);
}

void DXcbWMSupport::updateHasScissorWindow()
{
    qCDebug(dxcbwm) << "Updating has scissor window";
    bool hasScissorWindow(net_wm_atoms.contains(_deepin_scissor_window) && hasComposite());
    qCDebug(dxcbwm) << "Has scissor window:" << hasScissorWindow << "has composite:" << hasComposite();

    if (m_hasScissorWindow == hasScissorWindow) {
        qCDebug(dxcbwm) << "Scissor window state unchanged";
        return;
    }

    m_hasScissorWindow = hasScissorWindow;
    qCDebug(dxcbwm) << "Scissor window state changed to:" << hasScissorWindow;

    emit hasScissorWindowChanged(m_hasScissorWindow);
}

void DXcbWMSupport::updateWallpaperEffect()
{
    qCDebug(dxcbwm) << "Updating wallpaper effect";
    bool hasWallpaperEffect(net_wm_atoms.contains(_deepin_wallpaper));
    qCDebug(dxcbwm) << "Has wallpaper effect:" << hasWallpaperEffect;

    if (m_hasWallpaperEffect == hasWallpaperEffect) {
        qCDebug(dxcbwm) << "Wallpaper effect state unchanged";
        return;
    }

    m_hasWallpaperEffect = hasWallpaperEffect;
    qCDebug(dxcbwm) << "Wallpaper effect state changed to:" << hasWallpaperEffect;

    emit hasWallpaperEffectChanged(hasWallpaperEffect);
}

qint8 DXcbWMSupport::getHasWindowAlpha() const
{
    qCDebug(dxcbwm) << "Getting window alpha support, current value:" << m_windowHasAlpha;
    if (m_windowHasAlpha < 0) {
        qCDebug(dxcbwm) << "Testing window visual for alpha channel support";
        // 测试窗口visual是否支持alpha通道
        QWindow test_window;
        QSurfaceFormat sf = test_window.format();
        sf.setDepthBufferSize(32);
        sf.setAlphaBufferSize(8);
        test_window.setFormat(sf);
        test_window.create();
        // 当窗口位深不等于32时即认为它不支持alpha通道
        const auto &depth = static_cast<QXcbWindow*>(test_window.handle())->depth();
        const_cast<DXcbWMSupport*>(this)->m_windowHasAlpha = depth == 32;
        qCDebug(dxcbwm) << "Window depth:" << depth << "has alpha:" << (depth == 32);
    }

    qCDebug(dxcbwm) << "Window alpha support result:" << m_windowHasAlpha;
    return m_windowHasAlpha;
}

DXcbWMSupport *DXcbWMSupport::instance()
{
    qCDebug(dxcbwm) << "Getting DXcbWMSupport instance";
    return globalXWMS;
}

bool DXcbWMSupport::connectWindowManagerChangedSignal(QObject *object, std::function<void ()> slot)
{
    qCDebug(dxcbwm) << "Connecting window manager changed signal, object:" << object;
    if (!object) {
        qCDebug(dxcbwm) << "No object provided, using direct connection";
        return QObject::connect(globalXWMS, &DXcbWMSupport::windowManagerChanged, slot);
    }

    qCDebug(dxcbwm) << "Connecting with object context";
    return QObject::connect(globalXWMS, &DXcbWMSupport::windowManagerChanged, object, slot);
}

bool DXcbWMSupport::connectHasBlurWindowChanged(QObject *object, std::function<void ()> slot)
{
    qCDebug(dxcbwm) << "Connecting has blur window changed signal, object:" << object;
    if (!object) {
        qCDebug(dxcbwm) << "No object provided, using direct connection";
        return QObject::connect(globalXWMS, &DXcbWMSupport::hasBlurWindowChanged, slot);
    }

    qCDebug(dxcbwm) << "Connecting with object context";
    return QObject::connect(globalXWMS, &DXcbWMSupport::hasBlurWindowChanged, object, slot);
}

bool DXcbWMSupport::connectHasCompositeChanged(QObject *object, std::function<void ()> slot)
{
    qCDebug(dxcbwm) << "Connecting has composite changed signal, object:" << object;
    if (!object) {
        qCDebug(dxcbwm) << "No object provided, using direct connection";
        return QObject::connect(globalXWMS, &DXcbWMSupport::hasCompositeChanged, slot);
    }

    qCDebug(dxcbwm) << "Connecting with object context";
    return QObject::connect(globalXWMS, &DXcbWMSupport::hasCompositeChanged, object, slot);
}

bool DXcbWMSupport::connectHasNoTitlebarChanged(QObject *object, std::function<void ()> slot)
{
    qCDebug(dxcbwm) << "Connecting has no titlebar changed signal, object:" << object;
    if (!object) {
        qCDebug(dxcbwm) << "No object provided, using direct connection";
        return QObject::connect(globalXWMS, &DXcbWMSupport::hasNoTitlebarChanged, slot);
    }

    qCDebug(dxcbwm) << "Connecting with object context";
    return QObject::connect(globalXWMS, &DXcbWMSupport::hasNoTitlebarChanged, object, slot);
}

bool DXcbWMSupport::connectHasWallpaperEffectChanged(QObject *object, std::function<void ()> slot)
{
    qCDebug(dxcbwm) << "Connecting has wallpaper effect changed signal, object:" << object;
    if (!object) {
        qCDebug(dxcbwm) << "No object provided, using direct connection";
        return QObject::connect(globalXWMS, &DXcbWMSupport::hasWallpaperEffectChanged, slot);
    }

    qCDebug(dxcbwm) << "Connecting with object context";
    return QObject::connect(globalXWMS, &DXcbWMSupport::hasWallpaperEffectChanged, object, slot);
}

bool DXcbWMSupport::connectWindowListChanged(QObject *object, std::function<void ()> slot)
{
    qCDebug(dxcbwm) << "Connecting window list changed signal, object:" << object;
    if (!object) {
        qCDebug(dxcbwm) << "No object provided, using direct connection";
        return QObject::connect(globalXWMS, &DXcbWMSupport::windowListChanged, slot);
    }

    qCDebug(dxcbwm) << "Connecting with object context";
    return QObject::connect(globalXWMS, &DXcbWMSupport::windowListChanged, object, slot);
}

bool DXcbWMSupport::connectWindowMotifWMHintsChanged(QObject *object, std::function<void (quint32)> slot)
{
    qCDebug(dxcbwm) << "Connecting window motif WM hints changed signal, object:" << object;
    if (!object) {
        qCDebug(dxcbwm) << "No object provided, using direct connection";
        return QObject::connect(globalXWMS, &DXcbWMSupport::windowMotifWMHintsChanged, slot);
    }

    qCDebug(dxcbwm) << "Connecting with object context";
    return QObject::connect(globalXWMS, &DXcbWMSupport::windowMotifWMHintsChanged, object, slot);
}

void DXcbWMSupport::setMWMFunctions(quint32 winId, quint32 func)
{
    qCDebug(dxcbwm) << "Setting MWM functions for window:" << winId << "functions:" << func;
    // FIXME(zccrs): The Openbox window manager does not support the Motif Hints
    if (instance()->windowManagerName() == "Openbox") {
        qCDebug(dxcbwm) << "Openbox window manager does not support Motif Hints, skipping";
        return;
    }

    Utility::QtMotifWmHints hints = Utility::getMotifWmHints(winId);

    hints.flags |= MWM_HINTS_FUNCTIONS;
    hints.functions = func;

    Utility::setMotifWmHints(winId, hints);
    qCDebug(dxcbwm) << "MWM functions set successfully";
}

quint32 DXcbWMSupport::getMWMFunctions(quint32 winId)
{
    qCDebug(dxcbwm) << "Getting MWM functions for window:" << winId;
    Utility::QtMotifWmHints hints = Utility::getMotifWmHints(winId);

    if (hints.flags & MWM_HINTS_FUNCTIONS) {
        qCDebug(dxcbwm) << "MWM functions found:" << hints.functions;
        return hints.functions;
    }

    qCDebug(dxcbwm) << "No MWM functions found, returning MWM_FUNC_ALL";
    return MWM_FUNC_ALL;
}

quint32 DXcbWMSupport::getRealWinId(quint32 winId)
{
    qCDebug(dxcbwm) << "Getting real window ID for:" << winId;
    for (const DFrameWindow *frame : DFrameWindow::frameWindowList) {
        if (frame->handle() && frame->handle()->winId() == winId
                && frame->m_contentWindow && frame->m_contentWindow->handle()) {
            const auto &realWinId = static_cast<QXcbWindow*>(frame->m_contentWindow->handle())->QXcbWindow::winId();
            qCDebug(dxcbwm) << "Found frame window, real window ID:" << realWinId;
            return realWinId;
        }
    }

    qCDebug(dxcbwm) << "No frame window found, returning original window ID:" << winId;
    return winId;
}

void DXcbWMSupport::setMWMDecorations(quint32 winId, quint32 decor)
{
    qCDebug(dxcbwm) << "Setting MWM decorations for window:" << winId << "decorations:" << decor;
    winId = getRealWinId(winId);

    Utility::QtMotifWmHints hints = Utility::getMotifWmHints(winId);

    hints.flags |= MWM_HINTS_DECORATIONS;
    hints.decorations = decor;

    Utility::setMotifWmHints(winId, hints);
    qCDebug(dxcbwm) << "MWM decorations set successfully";
}

quint32 DXcbWMSupport::getMWMDecorations(quint32 winId)
{
    qCDebug(dxcbwm) << "Getting MWM decorations for window:" << winId;
    winId = getRealWinId(winId);

    Utility::QtMotifWmHints hints = Utility::getMotifWmHints(winId);

    if (hints.flags & MWM_HINTS_DECORATIONS) {
        qCDebug(dxcbwm) << "MWM decorations found:" << hints.decorations;
        return hints.decorations;
    }

    qCDebug(dxcbwm) << "No MWM decorations found, returning MWM_DECOR_ALL";
    return MWM_DECOR_ALL;
}

void DXcbWMSupport::popupSystemWindowMenu(quint32 winId)
{
    qCDebug(dxcbwm) << "Popping up system window menu for window:" << winId;
    Utility::showWindowSystemMenu(winId);
}

QString DXcbWMSupport::windowManagerName() const
{
    qCDebug(dxcbwm) << "Getting window manager name:" << m_wmName;
    return m_wmName;
}

QVector<xcb_window_t> DXcbWMSupport::allWindow() const
{
    qCDebug(dxcbwm) << "Getting all windows";
    QVector<xcb_window_t> window_list_stacking;

    xcb_window_t root = DPlatformIntegration::xcbConnection()->primaryScreen()->root();
    int offset = 0;
    int remaining = 0;
    xcb_connection_t *xcb_connection = DPlatformIntegration::xcbConnection()->xcb_connection();

    do {
        xcb_get_property_cookie_t cookie = xcb_get_property(xcb_connection, false, root,
                                                            Utility::internAtom("_NET_CLIENT_LIST_STACKING"),
                                                            XCB_ATOM_WINDOW, offset, 1024);
        xcb_get_property_reply_t *reply = xcb_get_property_reply(xcb_connection, cookie, NULL);
        if (!reply) {
            qCDebug(dxcbwm) << "No reply for client list stacking property";
            break;
        }

        remaining = 0;

        if (reply->type == XCB_ATOM_WINDOW && reply->format == 32) {
            int len = xcb_get_property_value_length(reply)/sizeof(xcb_window_t);
            xcb_window_t *windows = (xcb_window_t *)xcb_get_property_value(reply);
            int s = window_list_stacking.size();
            window_list_stacking.resize(s + len);
            memcpy(window_list_stacking.data() + s, windows, len*sizeof(xcb_window_t));
            qCDebug(dxcbwm) << "Added" << len << "windows, total:" << window_list_stacking.size();

            remaining = reply->bytes_after;
            offset += len;
        } else {
            qCDebug(dxcbwm) << "Invalid client list stacking property format";
        }

        free(reply);
    } while (remaining > 0);

    qCDebug(dxcbwm) << "Total windows found:" << window_list_stacking.size();
    return window_list_stacking;
}

static QXcbScreen *screenFromPoint(const QPoint &p)
{
    qCDebug(dxcbwm) << "Finding screen from point:" << p;
    for (QXcbScreen *screen : DPlatformIntegration::xcbConnection()->screens()) {
        if (screen->geometry().contains(p)) {
            qCDebug(dxcbwm) << "Found screen:" << screen->name() << "geometry:" << screen->geometry();
            return screen;
        }
    }

    qCDebug(dxcbwm) << "No screen found for point, using primary screen";
    return DPlatformIntegration::xcbConnection()->primaryScreen();
}

xcb_window_t DXcbWMSupport::windowFromPoint(const QPoint &p) const
{
    qCDebug(dxcbwm) << "Finding window from point:" << p;
    xcb_window_t wid = XCB_NONE;
    xcb_connection_t *xcb_connection = DPlatformIntegration::xcbConnection()->xcb_connection();
    xcb_window_t root = screenFromPoint(p)->root();

    xcb_window_t parent = root;
    xcb_window_t child = root;
    int16_t x = static_cast<int16_t>(p.x());
    int16_t y = static_cast<int16_t>(p.y());

    auto translate_reply = Q_XCB_REPLY_UNCHECKED(xcb_translate_coordinates, xcb_connection, parent, child, x, y);
    if (!translate_reply) {
        qCDebug(dxcbwm) << "Failed to translate coordinates";
        return wid;
    }

    child = translate_reply->child;
    if (!child || child == root) {
        qCDebug(dxcbwm) << "No child window or child is root";
        return wid;
    }

    child = Find_Client(xcb_connection, root, child);
    wid = child;
    qCDebug(dxcbwm) << "Found window ID:" << wid;
    return wid;
}

bool DXcbWMSupport::isDeepinWM() const
{
    qCDebug(dxcbwm) << "Checking if is DeepinWM:" << m_isDeepinWM;
    return m_isDeepinWM;
}

bool DXcbWMSupport::isKwin() const
{
    qCDebug(dxcbwm) << "Checking if is KWin:" << m_isKwin;
    return m_isKwin;
}

bool DXcbWMSupport::isSupportedByWM(xcb_atom_t atom) const
{
    const auto &result = net_wm_atoms.contains(atom);
    qCDebug(dxcbwm) << "Checking if atom" << atom << "is supported by WM:" << result;
    return result;
}

bool DXcbWMSupport::isContainsForRootWindow(xcb_atom_t atom) const
{
    const auto &result = root_window_properties.contains(atom);
    qCDebug(dxcbwm) << "Checking if atom" << atom << "is in root window properties:" << result;
    return result;
}

bool DXcbWMSupport::hasBlurWindow() const
{
    const auto &result = m_hasBlurWindow && getHasWindowAlpha();
    qCDebug(dxcbwm) << "Checking has blur window:" << result << "m_hasBlurWindow:" << m_hasBlurWindow;
    return result;
}

bool DXcbWMSupport::hasComposite() const
{
    qCDebug(dxcbwm) << "Checking has composite:" << m_hasComposite;
    return m_hasComposite;
}

bool DXcbWMSupport::hasNoTitlebar() const
{
    qCDebug(dxcbwm) << "Checking has no titlebar";
    /*
    *  NOTE(lxz): Some dtk windows may be started before the window manager,
    *  and the rounded corners of the window cannot be set correctly,
    *  and need to use environment variables to force the setting.
    */
    if (qEnvironmentVariableIsSet("D_DXCB_FORCE_NO_TITLEBAR")) {
        const auto &forceValue = qEnvironmentVariableIntValue("D_DXCB_FORCE_NO_TITLEBAR");
        qCDebug(dxcbwm) << "Force no titlebar environment variable set, value:" << forceValue;
        return forceValue != 0;
    }

    static bool disable = qEnvironmentVariableIsSet("D_DXCB_DISABLE_NO_TITLEBAR");
    const auto &result = !disable && m_hasNoTitlebar;
    qCDebug(dxcbwm) << "No titlebar result:" << result << "disable:" << disable << "m_hasNoTitlebar:" << m_hasNoTitlebar;
    return result;
}

bool DXcbWMSupport::hasScissorWindow() const
{
    static bool disable = qEnvironmentVariableIsSet("D_DXCB_DISABLE_SCISSOR_WINDOW");
    const auto &result = !disable && m_hasScissorWindow;
    qCDebug(dxcbwm) << "Scissor window result:" << result << "disable:" << disable << "m_hasScissorWindow:" << m_hasScissorWindow;
    return result;
}

bool DXcbWMSupport::hasWindowAlpha() const
{
    // 窗管不支持混成时也等价于窗口visual不支持alpha通道
    const auto &result = m_hasComposite && getHasWindowAlpha();
    qCDebug(dxcbwm) << "Window alpha result:" << result << "m_hasComposite:" << m_hasComposite;
    return result;
}

bool DXcbWMSupport::hasWallpaperEffect() const
{
    qCDebug(dxcbwm) << "Checking has wallpaper effect:" << m_hasWallpaperEffect;
    return m_hasWallpaperEffect;
}

bool DXcbWMSupport::Global::hasBlurWindow()
{
    qCDebug(dxcbwm) << "Global::hasBlurWindow called";
    const auto &result = DXcbWMSupport::instance()->hasBlurWindow();
    qCDebug(dxcbwm) << "Global::hasBlurWindow result:" << result;
    return result;
}

bool DXcbWMSupport::Global::hasComposite()
{
    qCDebug(dxcbwm) << "Global::hasComposite called";
    // 为了兼容现有的dtk应用中的逻辑，此处默认认为窗管是否支持混成等价于窗口是否支持alpha通道
    static bool composite_with_alpha = qgetenv("D_DXCB_COMPOSITE_WITH_WINDOW_ALPHA") != "0";
    qCDebug(dxcbwm) << "Composite with alpha setting:" << composite_with_alpha;

    const auto &result = composite_with_alpha ? hasWindowAlpha() : DXcbWMSupport::instance()->hasComposite();
    qCDebug(dxcbwm) << "Global::hasComposite result:" << result;
    return result;
}

bool DXcbWMSupport::Global::hasNoTitlebar()
{
    qCDebug(dxcbwm) << "Global::hasNoTitlebar called";
    const auto &result = DXcbWMSupport::instance()->hasNoTitlebar();
    qCDebug(dxcbwm) << "Global::hasNoTitlebar result:" << result;
    return result;
}

bool DXcbWMSupport::Global::hasWindowAlpha()
{
    qCDebug(dxcbwm) << "Global::hasWindowAlpha called";
    const auto &result = DXcbWMSupport::instance()->hasWindowAlpha();
    qCDebug(dxcbwm) << "Global::hasWindowAlpha result:" << result;
    return result;
}

bool DXcbWMSupport::Global::hasWallpaperEffect()
{
    qCDebug(dxcbwm) << "Global::hasWallpaperEffect called";
    const auto &result = DXcbWMSupport::instance()->hasWallpaperEffect();
    qCDebug(dxcbwm) << "Global::hasWallpaperEffect result:" << result;
    return result;
}

QString DXcbWMSupport::Global::windowManagerName()
{
    qCDebug(dxcbwm) << "Global::windowManagerName called";
    const auto &result = DXcbWMSupport::instance()->windowManagerName();
    qCDebug(dxcbwm) << "Global::windowManagerName result:" << result;
    return result;
}

DPP_END_NAMESPACE
