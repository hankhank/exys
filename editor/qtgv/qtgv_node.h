#pragma once

#include <cgraph.h>

#include <QGraphicsItem>
#include <QPen>

class QtGvEdge;
class QtGvScene;

class QtGvNode : public QGraphicsItem
{
public:
    QtGvNode(Agnode_t* node, QGraphicsItem* parent);
    virtual ~QtGvNode();

    QString label() const;
    void setLabel(const QString &label);

    QRectF boundingRect() const;
    void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0);
    void setAttribute(const QString &label, const QString &value);
    QString getAttribute(const QString &name) const;

    enum { Type = UserType + 2 };
    int type() const
    {
        return Type;
    }

    void updateLayout(qreal gheight);

private:

    QPainterPath mPath;
    QPen mPen;
    QBrush mBrush;

    Agnode_t* mNode;
};

