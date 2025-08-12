// SPDX-FileCopyrightText: 2017 - 2022 Uniontech Software Technology Co.,Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dplatformopenglcontexthelper.h"
#include "vtablehook.h"
#include "dplatformwindowhelper.h"
#include "dframewindow.h"
#include "dwmsupport.h"

#include <qpa/qplatformsurface.h>
#include <qpa/qplatformopenglcontext.h>
#include <qpa/qplatformbackingstore.h>

#include <QOpenGLPaintDevice>
#include <QPainter>
#include <QOpenGLFunctions>
#include <QPainterPathStroker>
#include <QDebug>
#include <QLoggingCategory>

#ifndef QT_DEBUG
Q_LOGGING_CATEGORY(dxcb, "dtk.qpa.xcb", QtInfoMsg);
#else
Q_LOGGING_CATEGORY(dxcb, "dtk.qpa.xcb");
#endif

DPP_BEGIN_NAMESPACE

DPlatformOpenGLContextHelper::DPlatformOpenGLContextHelper()
{
    qCDebug(dxcb) << "Creating DPlatformOpenGLContextHelper";
}

bool DPlatformOpenGLContextHelper::addOpenGLContext(QOpenGLContext *object, QPlatformOpenGLContext *context)
{
    qCDebug(dxcb) << "Adding OpenGL context, object:" << object << "context:" << context;
    Q_UNUSED(object)

    const auto &result = VtableHook::overrideVfptrFun(context, &QPlatformOpenGLContext::swapBuffers, this, &DPlatformOpenGLContextHelper::swapBuffers);
    qCDebug(dxcb) << "OpenGL context override result:" << result;
    return result;
}

static void drawCornerImage(const QImage &source, const QPoint &source_offset, QPainter *dest, const QPainterPath &dest_path, QOpenGLFunctions *glf)
{
    qCDebug(dxcb) << "Drawing corner image, source size:" << source.size() << "offset:" << source_offset;
    if (source.isNull()) {
        qCDebug(dxcb) << "Source image is null, skipping";
        return;
    }

    const QRectF &br = dest_path.boundingRect();

    if (br.isEmpty()) {
        qCDebug(dxcb) << "Bounding rect is empty, skipping";
        return;
    }

    int height = dest->device()->height();
    QBrush brush(source);
    QImage tmp_image(br.size().toSize(), QImage::Format_RGBA8888);

    glf->glReadPixels(br.x(), height - br.y() - tmp_image.height(), tmp_image.width(), tmp_image.height(),
                      GL_RGBA, GL_UNSIGNED_BYTE, tmp_image.bits());

    tmp_image = tmp_image.mirrored();
    brush.setTransform(QTransform(1, 0, 0, 1, -source_offset.x() - br.x(), -source_offset.y() - br.y()));
    QPainter pa(&tmp_image);

    pa.setRenderHint(QPainter::Antialiasing);
    pa.setCompositionMode(QPainter::CompositionMode_Source);
    pa.fillPath(dest_path.translated(-br.topLeft()), brush);
    pa.end();
    dest->drawImage(br.topLeft(), tmp_image);
    qCDebug(dxcb) << "Corner image drawn successfully";
}

void DPlatformOpenGLContextHelper::swapBuffers(QPlatformSurface *surface)
{
    qCDebug(dxcb) << "Swapping buffers for surface:" << surface;
    if (!DWMSupport::instance()->hasWindowAlpha()) {
        qCDebug(dxcb) << "Window does not have alpha, skipping custom swap";
        goto end;
    }

    if (surface->surface()->surfaceClass() == QSurface::Window) {
        QWindow *window = static_cast<QWindow*>(surface->surface());
        DPlatformWindowHelper *window_helper = DPlatformWindowHelper::mapped.value(window->handle());

        if (!window_helper) {
            qCDebug(dxcb) << "No window helper found, skipping";
            goto end;
        }

        if (!window_helper->m_isUserSetClipPath && window_helper->getWindowRadius() <= 0) {
            qCDebug(dxcb) << "No user clip path and no window radius, skipping";
            goto end;
        }

        qreal device_pixel_ratio = window_helper->m_nativeWindow->window()->devicePixelRatio();
        QPainterPath path;
        const QPainterPath &real_clip_path = window_helper->m_clipPath * device_pixel_ratio;
        const QSize &window_size = window->handle()->geometry().size();

        path.addRect(QRect(QPoint(0, 0), window_size));
        path -= real_clip_path;

        if (path.isEmpty()) {
            qCDebug(dxcb) << "Path is empty after clipping, skipping";
            goto end;
        }

        QOpenGLPaintDevice device(window_size);
        QPainter pa_device(&device);

        pa_device.setCompositionMode(QPainter::CompositionMode_Source);

        if (window_helper->m_isUserSetClipPath) {
            qCDebug(dxcb) << "Using user set clip path";
            const QRect &content_rect = QRect(window_helper->m_frameWindow->contentOffsetHint() * device_pixel_ratio, window_size);
            QBrush border_brush(window_helper->m_frameWindow->platformBackingStore->toImage());
            border_brush.setTransform(QTransform(1, 0, 0, 1, -content_rect.x(), -content_rect.y()));
            pa_device.fillPath(path, border_brush);
        } else {
            qCDebug(dxcb) << "Using window radius for clipping";
            const QImage &frame_image = window_helper->m_frameWindow->platformBackingStore->toImage();
            const QRect background_rect(QPoint(0, 0), window_size);
            const QPoint offset = window_helper->m_frameWindow->contentOffsetHint() * device_pixel_ratio;
            QRect corner_rect(0, 0, window_helper->m_windowRadius * device_pixel_ratio, window_helper->m_windowRadius * device_pixel_ratio);
            QPainterPath corner_path;
            QOpenGLFunctions *gl_funcs = QOpenGLContext::currentContext()->functions();

            // draw top-left
            qCDebug(dxcb) << "Drawing top-left corner";
            corner_path.addRect(corner_rect);
            drawCornerImage(frame_image, offset, &pa_device, corner_path - real_clip_path, gl_funcs);

            // draw top-right
            qCDebug(dxcb) << "Drawing top-right corner";
            corner_rect.moveTopRight(background_rect.topRight());
            corner_path = QPainterPath();
            corner_path.addRect(corner_rect);
            drawCornerImage(frame_image, offset, &pa_device, corner_path - real_clip_path, gl_funcs);

            // draw bottom-left
            qCDebug(dxcb) << "Drawing bottom-left corner";
            corner_rect.moveBottomLeft(background_rect.bottomLeft());
            corner_path = QPainterPath();
            corner_path.addRect(corner_rect);
            drawCornerImage(frame_image, offset, &pa_device, corner_path - real_clip_path, gl_funcs);

            // draw bottom-right
            qCDebug(dxcb) << "Drawing bottom-right corner";
            corner_rect.moveBottomRight(background_rect.bottomRight());
            corner_path = QPainterPath();
            corner_path.addRect(corner_rect);
            drawCornerImage(frame_image, offset, &pa_device, corner_path - real_clip_path, gl_funcs);
        }

        pa_device.end();
        qCDebug(dxcb) << "Custom swap buffers completed";
    }

end:
    qCDebug(dxcb) << "Calling original swapBuffers";
    VtableHook::callOriginalFun(this->context(), &QPlatformOpenGLContext::swapBuffers, surface);
}

DPP_END_NAMESPACE
