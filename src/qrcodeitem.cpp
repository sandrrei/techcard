#include "qrcodeitem.h"

#include <QPainter>
#include <QtMath>

#include "qrcodegen.hpp"

namespace {
const int QUIET_ZONE = 4; // modules of white border required by the QR spec
}

QrCodeItem::QrCodeItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(false); // crisp module edges
}

QrCodeItem::~QrCodeItem() = default;

void QrCodeItem::setText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    m_qr.reset();

    if (!m_text.isEmpty()) {
        try {
            m_qr.reset(new qrcodegen::QrCode(qrcodegen::QrCode::encodeText(
                m_text.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM)));
        } catch (const std::exception &) {
            m_qr.reset(); // text too long for a QR code
        }
    }
    emit textChanged();
    update();
}

void QrCodeItem::setColor(const QColor &color)
{
    if (m_color == color)
        return;
    m_color = color;
    emit colorChanged();
    update();
}

void QrCodeItem::setBackground(const QColor &background)
{
    if (m_background == background)
        return;
    m_background = background;
    emit backgroundChanged();
    update();
}

void QrCodeItem::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), m_background);
    if (!m_qr)
        return;

    const int modules = m_qr->getSize();
    const int total = modules + 2 * QUIET_ZONE;
    const qreal side = qMin(width(), height());

    // Whole-pixel modules avoid blurry seams between squares
    const int cell = qMax(1, qFloor(side / total));
    const qreal drawn = qreal(cell) * modules;
    const qreal ox = (width() - drawn) / 2.0;
    const qreal oy = (height() - drawn) / 2.0;

    painter->setPen(Qt::NoPen);
    painter->setBrush(m_color);
    for (int y = 0; y < modules; ++y) {
        for (int x = 0; x < modules; ++x) {
            if (m_qr->getModule(x, y))
                painter->drawRect(QRectF(ox + x * cell, oy + y * cell, cell, cell));
        }
    }
}
