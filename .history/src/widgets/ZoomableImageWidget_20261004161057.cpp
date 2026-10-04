#include "ZoomableImageWidget.h"

#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>

ZoomableImageWidget::ZoomableImageWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(200, 200);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

ZoomableImageWidget::~ZoomableImageWidget() = default;

void ZoomableImageWidget::setImage(const QImage &image, bool resetViewState)
{
    m_image = image;
    m_text.clear();
    if (resetViewState) {
        // Reset view to fit-to-view so the image fills the grid cell
        m_zoomLevel = 1.0;
        m_panOffset = QPointF(0.0, 0.0);
        m_rotationDegrees = 0;
    } else {
        // Keep current zoom/pan, just re-clamp in case the new image has
        // different dimensions
        clampPanOffset();
    }
    update();
}

void ZoomableImageWidget::setImage(const QPixmap &pixmap, bool resetViewState)
{
    setImage(pixmap.toImage(), resetViewState);
}

void ZoomableImageWidget::setText(const QString &text)
{
    m_text = text;
    m_image = QImage();
    update();
}

QPointF ZoomableImageWidget::normalizedPan() const
{
    if (m_image.isNull() || m_image.width() <= 0 || m_image.height() <= 0) {
        return QPointF(0.0, 0.0);
    }
    return QPointF(m_panOffset.x() / m_image.width(),
                   m_panOffset.y() / m_image.height());
}

void ZoomableImageWidget::setZoomLevel(double level, QPointF focalPoint,
                                        bool emitSignal)
{
    if (m_image.isNull()) {
        m_zoomLevel = qBound(kMinZoom, level, kMaxZoom);
        update();
        if (emitSignal) {
            emit zoomChanged(m_zoomLevel, focalPoint);
        }
        return;
    }

    // focalPoint is in normalized image coordinates [0,1]. Anchor the
    // image pixel currently at that normalized position to its current
    // widget-space position across the zoom change. This mirrors the
    // wheelEvent's "image point under cursor stays under cursor" behaviour.
    const QPointF anchorWidget = normalizedImageToWidget(focalPoint);
    applyZoom(level, anchorWidget);

    if (emitSignal) {
        emit zoomChanged(m_zoomLevel, focalPoint);
    }
}

void ZoomableImageWidget::setNormalizedPan(QPointF normalizedOffset, bool emitSignal)
{
    if (m_image.isNull()) {
        if (emitSignal) {
            emit panChanged(QPointF(0.0, 0.0));
        }
        return;
    }
    m_panOffset = QPointF(normalizedOffset.x() * m_image.width(),
                          normalizedOffset.y() * m_image.height());
    clampPanOffset();
    update();
    if (emitSignal) {
        emit panChanged(normalizedPan());
    }
}

void ZoomableImageWidget::resetView(bool emitSignal)
{
    m_zoomLevel = 1.0;
    m_panOffset = QPointF(0.0, 0.0);
    update();
    if (emitSignal) {
        emit viewReset();
    }
}

void ZoomableImageWidget::rotateBy(int degrees, bool emitSignal)
{
    if (m_image.isNull() || degrees == 0) {
        return;
    }
    m_rotationDegrees = (m_rotationDegrees + degrees) % 360;
    if (m_rotationDegrees < 0) {
        m_rotationDegrees += 360;
    }
    clampPanOffset();
    update();
    if (emitSignal) {
        emit rotationChanged(m_rotationDegrees);
    }
}

// ---- Painting ----

double ZoomableImageWidget::fitScale() const
{
    if (m_image.isNull()) return 1.0;

    const QSizeF displaySize = rotatedImageSize();
    double scaleX = static_cast<double>(width()) / displaySize.width();
    double scaleY = static_cast<double>(height()) / displaySize.height();
    return qMin(scaleX, scaleY);
}

double ZoomableImageWidget::effectiveScale() const
{
    return fitScale() * m_zoomLevel;
}

QSizeF ZoomableImageWidget::rotatedImageSize() const
{
    if (m_rotationDegrees % 180 == 0) {
        return QSizeF(m_image.size());
    }
    return QSizeF(m_image.height(), m_image.width());
}

QTransform ZoomableImageWidget::imageToWidgetTransform(bool includePan) const
{
    const double scale = effectiveScale();
    const QSizeF displaySize = rotatedImageSize();
    const double panX = includePan ? m_panOffset.x() : 0.0;
    const double panY = includePan ? m_panOffset.y() : 0.0;
    const double radians = qDegreesToRadians(double(m_rotationDegrees));
    const double rotatedPanX = panX * qCos(radians) - panY * qSin(radians);
    const double rotatedPanY = panX * qSin(radians) + panY * qCos(radians);

    QTransform transform;
    transform.translate(width() / 2.0, height() / 2.0);
    transform.translate(rotatedPanX * scale, rotatedPanY * scale);
    transform.rotate(m_rotationDegrees);
    transform.scale(scale, scale);
    transform.translate(-m_image.width() / 2.0, -m_image.height() / 2.0);
    Q_UNUSED(displaySize);
    return transform;
}

void ZoomableImageWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.fillRect(rect(), QColor(0xF5, 0xF5, 0xF5));

    if (m_image.isNull()) {
        // Draw placeholder text
        if (!m_text.isEmpty()) {
            painter.setPen(QColor(0x9E, 0x9E, 0x9E));
            QFont font = painter.font();
            font.setPixelSize(12);
            painter.setFont(font);
            painter.drawText(rect(), Qt::AlignCenter, m_text);
        }
        return;
    }

    painter.save();
    painter.setClipRect(rect());
    painter.setTransform(imageToWidgetTransform());
    painter.drawImage(QPointF(0.0, 0.0), m_image);
    painter.restore();
}

// ---- Mouse interaction ----

QPointF ZoomableImageWidget::widgetToNormalized(const QPointF &widgetPos) const
{
    if (m_image.isNull()) return QPointF(0.5, 0.5);
    bool invertible = false;
    const QTransform inverse = imageToWidgetTransform().inverted(&invertible);
    if (!invertible) {
        return QPointF(0.5, 0.5);
    }
    const QPointF imagePoint = inverse.map(widgetPos);
    double nx = imagePoint.x() / m_image.width();
    double ny = imagePoint.y() / m_image.height();
    return QPointF(qBound(0.0, nx, 1.0), qBound(0.0, ny, 1.0));
}

void ZoomableImageWidget::clampPanOffset()
{
    if (m_image.isNull()) return;

    // Allow panning up to half the image dimension beyond edges
    double maxPanX = m_image.width() * 0.5;
    double maxPanY = m_image.height() * 0.5;
    m_panOffset.setX(qBound(-maxPanX, m_panOffset.x(), maxPanX));
    m_panOffset.setY(qBound(-maxPanY, m_panOffset.y(), maxPanY));
}

QPointF ZoomableImageWidget::normalizedImageToWidget(const QPointF &normalizedImage) const
{
    if (m_image.isNull()) {
        return QPointF(width() / 2.0, height() / 2.0);
    }
    return imageToWidgetTransform().map(
        QPointF(normalizedImage.x() * m_image.width(),
                normalizedImage.y() * m_image.height()));
}

void ZoomableImageWidget::applyZoom(double newLevel, const QPointF &anchorWidget)
{
    if (m_image.isNull()) {
        m_zoomLevel = qBound(kMinZoom, newLevel, kMaxZoom);
        update();
        return;
    }

    // Image coordinate under the anchor before the zoom change.
    const QPointF normalizedAnchor = widgetToNormalized(anchorWidget);

    // Apply new zoom (clamped).
    m_zoomLevel = qBound(kMinZoom, newLevel, kMaxZoom);

    // Solve for the pan that keeps (imgX, imgY) under anchorWidget.
    // anchorWidget = newOxNoPan + (m_panOffset + (imgX, imgY)) * newScale
    const QPointF imagePoint(normalizedAnchor.x() * m_image.width(),
                             normalizedAnchor.y() * m_image.height());
    const QPointF withoutPan = imageToWidgetTransform(false).map(imagePoint);
    const double scale = effectiveScale();
    const QPointF rotatedPan((anchorWidget.x() - withoutPan.x()) / scale,
                             (anchorWidget.y() - withoutPan.y()) / scale);
    const double radians = qDegreesToRadians(double(m_rotationDegrees));
    m_panOffset = QPointF(rotatedPan.x() * qCos(radians) + rotatedPan.y() * qSin(radians),
                          -rotatedPan.x() * qSin(radians) + rotatedPan.y() * qCos(radians));

    // At fit-to-view, snap pan back to zero so the image stays centred.
    if (qFuzzyCompare(m_zoomLevel, 1.0)) {
        m_panOffset = QPointF(0.0, 0.0);
    }

    clampPanOffset();
    update();
}

void ZoomableImageWidget::wheelEvent(QWheelEvent *event)
{
    if (m_image.isNull()) {
        event->ignore();
        return;
    }

    const QPointF focalWidget = event->position();
    // Capture the focal point in normalized image coords *before* zoom so the
    // emitted signal lets linked cells anchor on the same image-relative point.
    const QPointF normalizedFocal = widgetToNormalized(focalWidget);

    // angleDelta is in eighths of a degree; a standard mouse-wheel detent is
    // 15 degrees == 120 units. Using the continuous value (a double) instead of
    // an integer 120-step count lets high-resolution trackpads and precision
    // wheels — which report deltas far smaller than 120 — zoom proportionally
    // instead of being truncated to zero (no zoom at all on a macOS trackpad).
    // A full detent still yields exactly one kZoomStep, so ordinary mouse-wheel
    // behaviour is unchanged.
    const double steps = event->angleDelta().y() / 120.0;
    const double factor = qPow(kZoomStep, steps);
    const double newLevel = m_zoomLevel * factor;

    applyZoom(newLevel, focalWidget);

    emit zoomChanged(m_zoomLevel, normalizedFocal);
    event->accept();
}

void ZoomableImageWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_zoomLevel > 1.0) {
        m_isPanning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ZoomableImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();

        const double scale = effectiveScale();
        const QPointF rotatedDelta(delta.x() / scale, delta.y() / scale);
        const double radians = qDegreesToRadians(double(m_rotationDegrees));
        m_panOffset += QPointF(rotatedDelta.x() * qCos(radians) + rotatedDelta.y() * qSin(radians),
                       -rotatedDelta.x() * qSin(radians) + rotatedDelta.y() * qCos(radians));
        clampPanOffset();
        update();

        // Emit normalized pan so linked cells with differently-sized images
        // stay in agreement on which image region is centred.
        emit panChanged(normalizedPan());
        event->accept();
        return;
    }

    // Change cursor when hoverable for pan. Only call setCursor when the shape
    // actually changes: this slot runs on every mouse move, and reassigning the
    // cursor each time fights any context-driven cursor and can flicker.
    const Qt::CursorShape desired = (m_zoomLevel > 1.0)
        ? Qt::OpenHandCursor
        : Qt::ArrowCursor;
    if (cursor().shape() != desired) {
        setCursor(desired);
    }

    QWidget::mouseMoveEvent(event);
}

void ZoomableImageWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isPanning) {
        m_isPanning = false;
        if (m_zoomLevel > 1.0) {
            setCursor(Qt::OpenHandCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ZoomableImageWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        resetView(true); // emitSignal = true
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void ZoomableImageWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Clamp pan on resize to keep image in bounds
    clampPanOffset();
    update();
}
