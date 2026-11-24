#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <memory>

namespace qrcodegen { class QrCode; }

// QML item that renders a QR code for `text` (uses Nayuki's qrcodegen).
// Usage in QML:  import TechCard 1.0  ...  QrCode { text: "https://..." }
class QrCodeItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QColor background READ background WRITE setBackground NOTIFY backgroundChanged)
    Q_PROPERTY(bool valid READ valid NOTIFY textChanged)

public:
    explicit QrCodeItem(QQuickItem *parent = nullptr);
    ~QrCodeItem() override;

    QString text() const { return m_text; }
    void setText(const QString &text);
    QColor color() const { return m_color; }
    void setColor(const QColor &color);
    QColor background() const { return m_background; }
    void setBackground(const QColor &background);
    bool valid() const { return m_qr != nullptr; }

    void paint(QPainter *painter) override;

signals:
    void textChanged();
    void colorChanged();
    void backgroundChanged();

private:
    QString m_text;
    QColor m_color = Qt::black;
    QColor m_background = Qt::white;
    std::unique_ptr<qrcodegen::QrCode> m_qr;
};
