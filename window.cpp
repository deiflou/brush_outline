#include "window.h"

#include <QPainter>
#include <QPainterPath>
#include <QDoubleSpinBox>
#include <QDebug>

#include <cmath>

// Comment this to use a circle instead of a mask
#define USE_BRUSH_MASK
// Comment this to use a simple black outline instead of the fancy black & white
#define USE_BW_OUTLINE

constexpr int canvasWidth {512};
constexpr int canvasHeight {512};

#ifdef USE_BRUSH_MASK
constexpr QString maskFileName {"://brush_mask_3.png"};
#endif

struct Circle
{
    QPointF center;
    qreal radius;
};

qreal clamp(qreal v, qreal min, qreal max)
{
    return std::max(std::min(v, max), min);
}

qreal squaredDistanceToPoint(const QPointF &p1, const QPointF &p2)
{
    const QPointF delta = p2 - p1;
    return delta.x() * delta.x() + delta.y() * delta.y();
}

qreal distanceToPoint(const QPointF &p1, const QPointF &p2)
{
    return std::sqrt(squaredDistanceToPoint(p1, p2));
}

qreal distanceToCircle(const QPointF &p, const Circle &c)
{
    const qreal distanceToCenter = distanceToPoint(p, c.center);
    return distanceToCenter - c.radius;
}

bool isInsideCircle(const QPointF &p, const Circle &c)
{
    const qreal distanceToCenter = distanceToPoint(p, c.center);
    return distanceToCenter <= c.radius;
}

qreal sampleCircleFunction(const QPointF &p, const Circle &c)
{
    return isInsideCircle(p, c);
}

window::window()
    : m_canvas(canvasWidth, canvasHeight, QImage::Format_Grayscale8)
{
    resize(512, 512);

    // Initialize the canvas with a gradient
    {
        QPainter p(&m_canvas);
        QLinearGradient g(0.0, 0.0, 0.0, 512.0);
        g.setColorAt(0.0, QColor(32, 32, 32));
        g.setColorAt(1.0, QColor(224, 224, 224));
        p.fillRect(m_canvas.rect(), g);
    }

#ifdef USE_BRUSH_MASK
    const QImage brushMask = QImage(maskFileName).convertToFormat(QImage::Format_Grayscale8);
    const quint32 brushStride = brushMask.bytesPerLine();
#else
    const Circle c {{canvasWidth / 2, canvasHeight / 2}, 100};
#endif

    for (int y = 2; y < canvasHeight - 2; ++y) {
        quint8 *dst = m_canvas.scanLine(y) + 2;

#ifdef USE_BRUSH_MASK
        const quint8 *src = brushMask.scanLine(y) + 2;
        for (int x = 2; x < canvasWidth - 2; ++x, ++src, ++dst) {
            // Sample the mask to obtain the neighbor pixel values. It is sampled
            // in a binary fashion: pixels = 0 will return 0, and pixels > 0 will
            // return 1
            const qreal tlv   = *(src - brushStride - 1) > 0;
            const qreal tv    = *(src - brushStride    ) > 0;
            const qreal trv   = *(src - brushStride + 1) > 0;
            const qreal lv    = *(src          - 1) > 0;
            const qreal cv    = *(src             ) > 0;
            const qreal rv    = *(src          + 1) > 0;
            const qreal blv   = *(src + brushStride - 1) > 0;
            const qreal bv    = *(src + brushStride    ) > 0;
            const qreal brv   = *(src + brushStride + 1) > 0;
#else
        for (int x = 2; x < canvasWidth - 2; ++x, ++dst) {
            // Sample the circle function at the center of the neighbor pixels. 0 = outside the circle, 1 = inside
            const qreal tlv   = sampleCircleFunction(QPointF(x - 1 + 0.5, y - 1 + 0.5), c);
            const qreal tv    = sampleCircleFunction(QPointF(x     + 0.5, y - 1 + 0.5), c);
            const qreal trv   = sampleCircleFunction(QPointF(x + 1 + 0.5, y - 1 + 0.5), c);
            const qreal lv    = sampleCircleFunction(QPointF(x - 1 + 0.5, y     + 0.5), c);
            const qreal cv    = sampleCircleFunction(QPointF(x     + 0.5, y     + 0.5), c);
            const qreal rv    = sampleCircleFunction(QPointF(x + 1 + 0.5, y     + 0.5), c);
            const qreal blv   = sampleCircleFunction(QPointF(x - 1 + 0.5, y + 1 + 0.5), c);
            const qreal bv    = sampleCircleFunction(QPointF(x     + 0.5, y + 1 + 0.5), c);
            const qreal brv   = sampleCircleFunction(QPointF(x + 1 + 0.5, y + 1 + 0.5), c);
#endif

            // Perform some basic 3x3 gaussian blur. This gives an estimation of
            // how far we are from the contour of the shape
            const qreal v = tlv * 0.0625 + tv * 0.1250 + trv * 0.0625 +
                            lv  * 0.1250 + cv * 0.2500 + rv  * 0.1250 +
                            blv * 0.0625 + bv * 0.1250 + brv * 0.0625;

            // Since we sampled the shape function in a binary fashion and blurred
            // with a 3x3 kernel, values = 0 or 1 can be skipped because we only want
            // to paint the ones at the border (values such that 0 < x < 1)
            if (v <= 0.0 || v >= 1.0) {
                continue;
            }

#ifdef USE_BW_OUTLINE
            // Black & white outline
            {
                // Map the blurred region to the alpha of the outline. This function is:

                // 1|        ----------------
                //  |       /                \
                //  |      /                  \
                //  |     /                    \
                //  |    /                      \
                //  |   /                        \
                //  |  /                          \
                //  | /                            \
                //  |/                              \
                //  |--------------------------------
                //  0                                1

                // So the center of the contour region will be opaque and the borders
                // wil be semi-transparent to account for a bit of anti-aliasing
                const qreal alpha = std::min((1.0 - std::abs(2.0 * v - 1.0)) * 2.0, 1.0);
                if (alpha > 0.0) {
                    // a contrast function is used on the contour to keep the
                    // black and white colors, but less blurry
                    const qreal srcColor = clamp(v * 1.5 - 0.25, 0.0, 1.0);
                    const qreal dstColor = *dst / 255.0;
                    // Linear interpolation
                    const qreal blend = dstColor + (srcColor - dstColor) * alpha;
                    *dst = static_cast<quint8>(blend * 255.0);
                }
            }
#else
            // Normal outline
            {
                // Map the blurred region to the alpha of the outline. This function is:

                // 1|        -
                //  |       / \
                //  |      /   \
                //  |     /     \
                //  |    /       \
                //  |   /         \
                //  |  /           \
                //  | /             \
                //  |/               \
                //  |--------------------------------
                //  0                0.5             1
                // So we only keep the dark part of the contour, the bg part.
                const qreal alpha = std::max(1.0 - std::abs(4.0 * v - 1.0), 0.0);
                if (alpha > 0.0) {
                    // Here the color is just black
                    const qreal srcColor = 0.0;
                    const qreal dstColor = *dst / 255.0;
                    // Linear interpolation
                    const qreal blend = dstColor + (srcColor - dstColor) * alpha;
                    *dst = static_cast<quint8>(blend * 255.0);
                }
            }
#endif
        }
    }
}

window::~window()
{}

void window::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.drawImage(0, 0, m_canvas);
}
