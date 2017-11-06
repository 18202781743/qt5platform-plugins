/*
 * Copyright (C) 2017 ~ 2017 Deepin Technology Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dplatformbackingstorehelper.h"
#include "vtablehook.h"
#include "dplatformwindowhelper.h"
#include "dframewindow.h"
#include "dwmsupport.h"

#include <qpa/qplatformbackingstore.h>

#include <QPainter>
#include <QOpenGLPaintDevice>

DPP_BEGIN_NAMESPACE

DPlatformBackingStoreHelper::DPlatformBackingStoreHelper()
{

}

bool DPlatformBackingStoreHelper::addBackingStore(QPlatformBackingStore *store)
{
    QObject::connect(store->window(), &QWindow::destroyed, store->window(), [store] {
        VtableHook::clearGhostVtable(store);
    });

    return VtableHook::overrideVfptrFun(store, &QPlatformBackingStore::flush, this, &DPlatformBackingStoreHelper::flush);
}

void DPlatformBackingStoreHelper::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
//    if (Q_LIKELY(DWMSupport::instance()->hasComposite()))
    {
        DPlatformWindowHelper *window_helper = DPlatformWindowHelper::mapped.value(window->handle());

        if (window_helper && (window_helper->m_isUserSetClipPath || window_helper->m_windowRadius > 0)) {
            qreal device_pixel_ratio = window_helper->m_nativeWindow->window()->devicePixelRatio();
            QPainterPath path;

            path.addRegion(region);
            path -= window_helper->m_clipPath * device_pixel_ratio;

            if (path.isEmpty())
                goto end;

            QPainter pa(backingStore()->paintDevice());

            if (!pa.isActive())
                goto end;

            pa.setCompositionMode(QPainter::CompositionMode_Source);
            pa.setRenderHint(QPainter::Antialiasing);
            pa.setRenderHint(QPainter::SmoothPixmapTransform);
            pa.fillPath(path, Qt::transparent);
            pa.setClipPath(path);

            pa.drawImage((window_helper->m_windowVaildGeometry.topLeft()
                          - window_helper->m_frameWindow->contentOffsetHint()) * device_pixel_ratio,
                         window_helper->m_frameWindow->m_shadowImage);

            if (window_helper->m_frameWindow->m_borderWidth > 0) {
                QPen pen;

                pen.setWidthF(window_helper->m_frameWindow->m_borderWidth * 2);
                pen.setColor(window_helper->m_frameWindow->m_borderColor);
                pen.setJoinStyle(Qt::MiterJoin);

                pa.setPen(pen);
                pa.drawPath(window_helper->m_frameWindow->m_clipPathOfContent * device_pixel_ratio);
            }

            pa.end();
        }
    }

end:
    return VtableHook::callOriginalFun(this->backingStore(), &QPlatformBackingStore::flush, window, region, offset);
}

DPP_END_NAMESPACE
