// SPDX-FileCopyrightText: 2020 - 2022 Uniontech Software Technology Co.,Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dinputselectionhandle.h"
#include "ddesktopinputselectioncontrol.h"

#include <QImageReader>
#include <QGuiApplication>
#include <QPainter>
#include <QPalette>
#include <QMouseEvent>
#include <QLoggingCategory>

#ifndef QT_DEBUG
Q_LOGGING_CATEGORY(dplatform, "dtk.qpa.platform", QtInfoMsg);
#else
Q_LOGGING_CATEGORY(dplatform, "dtk.qpa.platform");
#endif

DPP_BEGIN_NAMESPACE

DInputSelectionHandle::DInputSelectionHandle(HandlePosition position, DDesktopInputSelectionControl *pControl)
    : QRasterWindow()
    , m_position(position)
    , m_pInputSelectionControl(pControl)
{
    qCDebug(dplatform) << "DInputSelectionHandle constructor called, position:" << position;
    setFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);

    QSurfaceFormat format;
    format.setAlphaBufferSize(8);
    setFormat(format);
    updateImage(position);
}

DInputSelectionHandle::HandlePosition DInputSelectionHandle::handlePosition() const
{
    qCDebug(dplatform) << "handlePosition called, returning:" << m_position;
    return m_position;
}

void DInputSelectionHandle::setHandlePosition(HandlePosition position)
{
    qCDebug(dplatform) << "setHandlePosition called, position:" << position;
    if (m_position == position) {
        qCDebug(dplatform) << "Position unchanged, skipping update";
        return;
    }

    m_position = position;
    updateImage(position);
    update();
}

QSize DInputSelectionHandle::handleImageSize() const
{
    qCDebug(dplatform) << "handleImageSize called, size:" << m_image.size() / devicePixelRatio();
    return m_image.size() / devicePixelRatio();
}

void DInputSelectionHandle::updateImage(HandlePosition position)
{
    qCDebug(dplatform) << "updateImage called, position:" << position;
    QImage handle;
    QImageReader reader(position == Up ? ":/up_handle.svg" : ":/down_handle.svg");
    QSize image_size(reader.size());

    reader.setScaledSize(image_size * devicePixelRatio());
    reader.read(&handle);

    m_image = handle;
    m_image.setDevicePixelRatio(devicePixelRatio());
}

void DInputSelectionHandle::paintEvent(QPaintEvent *pe)
{
    qCDebug(dplatform) << "paintEvent called";
    Q_UNUSED(pe);
    QPainter painter(this);
    QImage image(m_image);
    const QSize szDelta = size() - image.size();

    QPainter pa(&image);
    pa.setCompositionMode(QPainter::CompositionMode_SourceIn);
    pa.fillRect(image.rect(), qApp->palette().highlight());

    // center image onto window
    painter.drawImage(QPointF(szDelta.width(), szDelta.height()) / 2, image);
}

void DInputSelectionHandle::mousePressEvent(QMouseEvent *event)
{
    qCDebug(dplatform) << "mousePressEvent called, pos:" << event->pos();
    if (QWindow *focusWindow = QGuiApplication::focusWindow()) {
        QCoreApplication::sendEvent(focusWindow, event);
    }
}

void DInputSelectionHandle::mouseReleaseEvent(QMouseEvent *event)
{
    qCDebug(dplatform) << "mouseReleaseEvent called, pos:" << event->pos();
    if (QWindow *focusWindow = QGuiApplication::focusWindow()) {
        QCoreApplication::sendEvent(focusWindow, event);
    }
}

void DInputSelectionHandle::mouseMoveEvent(QMouseEvent *event)
{
    qCDebug(dplatform) << "mouseMoveEvent called, pos:" << event->pos();
    if (QWindow *focusWindow = QGuiApplication::focusWindow()) {
        QCoreApplication::sendEvent(focusWindow, event);
    }
}

DPP_END_NAMESPACE
