
#include <qtgv_node.h>
#include <qtgv_scene.h>

#include <QDebug>
#include <QPainter>

#include "helpers.h"

QPointF CenterToOrigin(const QPointF &p, qreal width, qreal height)
{
    return QPointF(p.x() - width/2, p.y() - height/2);
}

QPolygonF ToPolygon(const polygon_t *poly, qreal width, qreal height)
{
    if (poly->peripheries != 1)
        qWarning("unsupported number of peripheries %d", poly->peripheries);

    const int sides = poly->sides;
    const pointf* vertices = poly->vertices;

    QPolygonF polygon;
    for (int side = 0; side < sides; side++)
        polygon.append(QPointF(vertices[side].x + width/2, vertices[side].y + height/2));
    return polygon;
}

QPainterPath ToPath(const char *type, const polygon_t *poly, qreal width, qreal height)
{
    QPainterPath path;
    if ((strcmp(type, "rectangle") == 0) ||
        (strcmp(type, "box") == 0) ||
        (strcmp(type, "hexagon") == 0) ||
        (strcmp(type, "polygon") == 0) ||
        (strcmp(type, "diamond") == 0))
    {
        QPolygonF polygon = ToPolygon(poly, width, height);
        polygon.append(polygon[0]);
        path.addPolygon(polygon);
    }
    else if ((strcmp(type, "ellipse") == 0) ||
            (strcmp(type, "circle") == 0))
    {
        QPolygonF polygon = ToPolygon(poly, width, height);
        path.addEllipse(QRectF(polygon[0], polygon[1]));
    }
    else
    {
        qWarning("unsupported shape %s", type);
    }
    return path;
}


Qt::BrushStyle ToBrushStyle(const QString &style)
{
    if(style == "filled")
        return Qt::SolidPattern;
    return Qt::NoBrush;
}

QtGvNode::QtGvNode(Agnode_t* node, QGraphicsItem* parent)
: QGraphicsItem(parent)
, mNode(node)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
}

QtGvNode::~QtGvNode()
{
}

QString QtGvNode::label() const
{
    return getAttribute("label");
}

void QtGvNode::setLabel(const QString &label)
{
    setAttribute("label", label);
}

QRectF QtGvNode::boundingRect() const
{
    return mPath.boundingRect();
}

void QtGvNode::paint(QPainter * painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->save();

    painter->setPen(mPen);

    if(isSelected())
    {
        QBrush tbrush(mBrush);
        tbrush.setColor(tbrush.color().darker(120));
        painter->setBrush(tbrush);
    }
    else
    {
        painter->setBrush(mBrush);
    }

    painter->drawPath(mPath);

    //painter->setPen(QtGvCore::toColor(getAttribute("labelfontcolor")));

    const QRectF rect = boundingRect().adjusted(2,2,-2,-2); //Margin
    painter->drawText(rect, Qt::AlignCenter , QtGvNode::label());
    painter->restore();
}

void QtGvNode::setAttribute(const QString &name, const QString &value)
{
    char empty[] = "";
    agsafeset
    (
        mNode,
        name.toLocal8Bit().data(),
        value.toLocal8Bit().data(),
        empty
    );
}

QString QtGvNode::getAttribute(const QString &name) const
{
    char* value = agget(mNode, name.toLocal8Bit().data());
    if(value) return value;
    return QString();
}

void QtGvNode::updateLayout(qreal gheight)
{
    prepareGeometryChange();
    qreal width = ND_width(mNode) * DotDefaultDPI;
    qreal height = ND_height(mNode) * DotDefaultDPI;

    //Node Position (center)
    setPos
    (
        CenterToOrigin
        (
            ToPoint(ND_coord(mNode), gheight),
            width,
            height
        )
    );

    //Node on top
    setZValue(1);

    //Node path
    mPath = 
    ToPath
    (
        ND_shape(mNode)->name, 
        (polygon_t*)ND_shape_info(mNode), 
        width, 
        height
    );

    mPen.setWidth(1);

    mBrush.setStyle(ToBrushStyle(getAttribute("style")));
    mBrush.setColor(ToColor(getAttribute("fillcolor")));
    mPen.setColor(ToColor(getAttribute("color")));

    setToolTip(getAttribute("tooltip"));
}
