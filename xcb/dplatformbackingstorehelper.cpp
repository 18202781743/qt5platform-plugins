// SPDX-FileCopyrightText: 2017 - 2022 Uniontech Software Technology Co.,Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dplatformbackingstorehelper.h"
#include "vtablehook.h"
#include "dplatformwindowhelper.h"
#include "dframewindow.h"
#include "dwmsupport.h"

#ifdef Q_OS_LINUX
#define private public
#include "qxcbbackingstore.h"
#undef private
#endif

#include <qpa/qplatformbackingstore.h>

#include <QPainter>
#include <QOpenGLPaintDevice>
#include <QThreadStorage>
#include <QThread>
#include <QLoggingCategory>

#include <xcb/xcb_image.h>

#ifndef QT_DEBUG
Q_LOGGING_CATEGORY(dxcb, "dtk.qpa.xcb", QtInfoMsg);
#else
Q_LOGGING_CATEGORY(dxcb, "dtk.qpa.xcb");
#endif

#ifdef Q_OS_LINUX
QT_BEGIN_NAMESPACE
class QXcbShmImage : public QXcbObject
{
public:
    xcb_shm_segment_info_t m_shm_info;
};
QT_END_NAMESPACE
#endif

DPP_BEGIN_NAMESPACE

DPlatformBackingStoreHelper::DPlatformBackingStoreHelper()
{
    qCDebug(dxcb) << "DPlatformBackingStoreHelper constructor called";
}

bool DPlatformBackingStoreHelper::addBackingStore(QPlatformBackingStore *store)
{
    qCDebug(dxcb) << "addBackingStore called, store:" << store;
    VtableHook::overrideVfptrFun(store, &QPlatformBackingStore::beginPaint, this, &DPlatformBackingStoreHelper::beginPaint);
    VtableHook::overrideVfptrFun(store, &QPlatformBackingStore::paintDevice, this, &DPlatformBackingStoreHelper::paintDevice);
    VtableHook::overrideVfptrFun(store, &QPlatformBackingStore::resize, this, &DPlatformBackingStoreHelper::resize);

    return VtableHook::overrideVfptrFun(store, &QPlatformBackingStore::flush, this, &DPlatformBackingStoreHelper::flush);
}

static QThreadStorage<bool> _d_dxcb_overridePaintDevice;

QPaintDevice *DPlatformBackingStoreHelper::paintDevice()
{
    qCDebug(dxcb) << "paintDevice called";
    QPlatformBackingStore *store = backingStore();

    if (_d_dxcb_overridePaintDevice.hasLocalData() && _d_dxcb_overridePaintDevice.localData()) {
        qCDebug(dxcb) << "Using override paint device";
        static thread_local QImage device(1, 1, QImage::Format_Alpha8);

        return &device;
    }

    return VtableHook::callOriginalFun(store, &QPlatformBackingStore::paintDevice);
}

void DPlatformBackingStoreHelper::beginPaint(const QRegion &region)
{
    qCDebug(dxcb) << "beginPaint called, region:" << region;
    QPlatformBackingStore *store = backingStore();
    bool has_alpha = store->window()->property("_d_dxcb_TransparentBackground").toBool();

    if (!has_alpha) {
        qCDebug(dxcb) << "Setting override paint device for non-alpha window";
        _d_dxcb_overridePaintDevice.setLocalData(true);
    }

    VtableHook::callOriginalFun(store, &QPlatformBackingStore::beginPaint, region);

    _d_dxcb_overridePaintDevice.setLocalData(false);
}

void DPlatformBackingStoreHelper::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    qCDebug(dxcb) << "flush called, window:" << window << "region:" << region << "offset:" << offset;
    if (!backingStore()->paintDevice()) {
        qCDebug(dxcb) << "No paint device available";
        return;
    }

    if (Q_LIKELY(DWMSupport::instance()->hasWindowAlpha())) {
        qCDebug(dxcb) << "Window has alpha support";
        DPlatformWindowHelper *window_helper = DPlatformWindowHelper::mapped.value(window->handle());

        if (!window_helper) {
            qCDebug(dxcb) << "No window helper found";
            goto end;
        }

        qreal device_pixel_ratio = window_helper->m_nativeWindow->window()->devicePixelRatio();
        int window_radius = qRound(window_helper->getWindowRadius() * device_pixel_ratio);
        qCDebug(dxcb) << "Window radius:" << window_radius << "device pixel ratio:" << device_pixel_ratio;

        // 停止触发内部窗口更新的定时器
        if (window_helper->m_frameWindow->m_paintShadowOnContentTimerId > 0) {
            qCDebug(dxcb) << "Killing paint shadow timer";
            window_helper->m_frameWindow->killTimer(window_helper->m_frameWindow->m_paintShadowOnContentTimerId);
            window_helper->m_frameWindow->m_paintShadowOnContentTimerId = -1;
        }

        if (window_helper && (window_helper->m_isUserSetClipPath || window_radius > 0)) {
            qCDebug(dxcb) << "Processing clip path or window radius";
            QPainterPath path;
            QPainterPath clip_path = window_helper->m_clipPath * device_pixel_ratio;
            QRegion new_region = region;

//            if (!window_helper->m_isUserSetClipPath) {
//                QRect window_rect(QPoint(0, 0), window_helper->m_nativeWindow->geometry().size());

//                new_region += QRect(0, 0, window_radius, window_radius);
//                new_region += QRect(window_rect.width() - window_radius,
//                                window_rect.height() - window_radius,
//                                window_radius, window_radius);
//                new_region += QRect(0, window_rect.height() - window_radius,
//                                window_radius, window_radius);
//                new_region += QRect(window_rect.width() - window_radius, 0,
//                                window_radius, window_radius);
//            }

            path.addRegion(new_region);
            path -= clip_path;

            if (path.isEmpty()) {
                qCDebug(dxcb) << "Path is empty, skipping paint";
                goto end;
            }

            QPainter pa(backingStore()->paintDevice());

            if (!pa.isActive()) {
                qCDebug(dxcb) << "Painter is not active";
                goto end;
            }

            QBrush border_brush(window_helper->m_frameWindow->m_shadowImage);
            const QPoint &offset = (window_helper->m_frameWindow->m_contentGeometry.topLeft()
                                    - 2 * window_helper->m_frameWindow->contentOffsetHint()) * device_pixel_ratio;

            border_brush.setTransform(QTransform(1, 0, 0, 1, offset.x(), offset.y()));

            pa.setRenderHint(QPainter::Antialiasing);
            pa.setCompositionMode(QPainter::CompositionMode_Source);
            pa.fillPath(path, border_brush);

            if (window_helper->getBorderWidth() > 0
                    && window_helper->m_borderColor != Qt::transparent) {
                qCDebug(dxcb) << "Drawing border";
                pa.setClipPath(path);
                pa.setPen(QPen(window_helper->m_borderColor, window_helper->getBorderWidth(),
                               Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                pa.drawPath(clip_path);
            }

            pa.end();
        }
    }

end:
    return VtableHook::callOriginalFun(this->backingStore(), &QPlatformBackingStore::flush, window, region, offset);
}

#ifdef Q_OS_LINUX
void DPlatformBackingStoreHelper::resize(const QSize &size, const QRegion &staticContents)
{
    qCDebug(dxcb) << "resize called, size:" << size;
    VtableHook::callOriginalFun(this->backingStore(), &QPlatformBackingStore::resize, size, staticContents);

    QXcbBackingStore *bs = static_cast<QXcbBackingStore*>(backingStore());
#if QT_VERSION < QT_VERSION_CHECK(5, 12, 0)
    QXcbShmImage *shm_image = reinterpret_cast<QXcbShmImage*>(bs->m_image);
#else
    struct _QXcbBackingStore { QImage *m_image; }; // Expose m_image
    QXcbShmImage *shm_image = reinterpret_cast<QXcbShmImage*>(reinterpret_cast<_QXcbBackingStore*>( &bs )->m_image);
#endif

    if (shm_image->m_shm_info.shmaddr) {
        qCDebug(dxcb) << "Shared memory image found, updating window properties";
        DPlatformWindowHelper *window_helper = DPlatformWindowHelper::mapped.value(bs->window()->handle());

        if (!window_helper) {
            qCDebug(dxcb) << "No window helper found for shared memory update";
            return;
        }

        xcb_atom_t atom = Utility::internAtom("_DEEPIN_DXCB_SHM_INFO", false);
        QVector<quint32> info;
        const QImage &qimage = bs->toImage();

        info << shm_image->m_shm_info.shmid // 共享内存 id
             << qimage.width() // 图片宽度
             << qimage.height() // 图片高度
             << qimage.bytesPerLine() // 同QImage::bytesPerLine
             << qimage.format() // 图片格式
             << 0 // 图片有效区域的x
             << 0 // 图片有效区域的y
             << qimage.width() // 图片有效区域的宽度
             << qimage.height(); // 图片有效区域的高度

        Utility::setWindowProperty(window_helper->m_frameWindow->winId(), atom, XCB_ATOM_CARDINAL, info.constData(), info.length(), sizeof(quint32) * 8);
    }
}
#endif

DPP_END_NAMESPACE
