#include "./qoutlinelabel.h"

#include <QPainter>
#include <QPainterPath>

QOutlineLabel::QOutlineLabel(QWidget *parent) : QLabel(parent) {}

void QOutlineLabel::setOutline(QColor cf, QColor co, int ow, bool anti) {
    cFill            = cf;
    cOutline         = co;
    outlineWidth     = ow;
    antiAntialiasing = anti;
}

void QOutlineLabel::paintEvent(QPaintEvent *) {
    QPainter painter(this);

    if (antiAntialiasing) {
        painter.setRenderHint(QPainter::Antialiasing);
    }

    QFont font = this->font();
    QString text = this->text();
    
    if (text.isEmpty()) {
        return;
    }

    QPainterPath path;
    QRect rect = this->contentsRect();
    
    // 获取文字的边界矩形
    QFontMetrics fm(font);
    QRect textRect = fm.boundingRect(text);
    
    // 根据对齐方式计算文字位置
    QPoint textPos;
    Qt::Alignment align = this->alignment();
    
    // 水平对齐
    if (align & Qt::AlignHCenter) {
        textPos.setX(rect.center().x() - textRect.width() / 2);
    } else if (align & Qt::AlignRight) {
        textPos.setX(rect.right() - textRect.width());
    } else {
        textPos.setX(rect.left());
    }
    
    // 垂直对齐
    if (align & Qt::AlignVCenter) {
        textPos.setY(rect.center().y() + textRect.height() / 2);
    } else if (align & Qt::AlignBottom) {
        textPos.setY(rect.bottom());
    } else {
        textPos.setY(rect.top() + textRect.height());
    }
    
    path.addText(textPos, font, text);

    painter.setPen(QPen(cOutline, outlineWidth));
    painter.drawPath(path);

    painter.fillPath(path, cFill);

    painter.end();
}
