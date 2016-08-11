#pragma once

#include <cgraph.h>

#include <QGraphicsItem>
#include <QPen>

class QtGvNode;
class QtGvScene;

class QtGvEdge : public QGraphicsItem
{
public:
    QtGvEdge(Agedge_t* edge, QGraphicsItem* parent);
    virtual ~QtGvEdge();

    QString label() const;
    QRectF boundingRect() const;
    QPainterPath shape() const;

    void setLabel(const QString &label);

    void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0);

    void setAttribute(const QString &name, const QString &value);
    QString getAttribute(const QString &name) const;

    void updateLayout(qreal gheight);

    enum { Type = UserType + 3 };
    int type() const
    {
        return Type;
    }

private:
    QPolygonF ToArrow(const QLineF &normal) const;

    Agedge_t* mEdge;

    QPainterPath mPath;
    QPen mPen;
    QPolygonF mHeadArrow;
    QPolygonF mTailArrow;

    QString mLabel;
    QRectF mLabelRect;
};
