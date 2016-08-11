
#include <qtgv_edge.h>
#include <qtgv_scene.h>
#include <QDebug>
#include <QPainter>

#include "helpers.h"

QPainterPath ToPath(const splines *spl, qreal gheight)
{
    QPainterPath path;
    if((spl->list != 0) && (spl->list->size%3 == 1))
    {
        bezier bez = spl->list[0];
        //If there is a starting point, draw a line from it to the first curve point
        if(bez.sflag)
        {
            path.moveTo(ToPoint(bez.sp, gheight));
            path.lineTo(ToPoint(bez.list[0], gheight));
        }
        else
            path.moveTo(ToPoint(bez.list[0], gheight));

        //Loop over the curve points
        for(int i=1; i<bez.size; i+=3)
            path.cubicTo(ToPoint(bez.list[i], gheight), ToPoint(bez.list[i+1], gheight), ToPoint(bez.list[i+2], gheight));

        //If there is an ending point, draw a line to it
        if(bez.eflag)
            path.lineTo(ToPoint(bez.ep, gheight));
    }
    return path;
}


Qt::PenStyle ToPenStyle(const QString &style)
{
    if(style =="dashed")
        return Qt::DashLine;
    else if(style == "dotted")
        return Qt::DotLine;
    return Qt::SolidLine;
}

QtGvEdge::QtGvEdge(Agedge_t* edge, QGraphicsItem* parent) 
: QGraphicsItem(parent)
, mEdge(edge)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
}

QtGvEdge::~QtGvEdge()
{
}

QString QtGvEdge::label() const
{
    return getAttribute("xlabel");
}

QRectF QtGvEdge::boundingRect() const
{
    return mPath.boundingRect() | mHeadArrow.boundingRect() | mTailArrow.boundingRect() | mLabelRect;
}

QPainterPath QtGvEdge::shape() const
{
    QPainterPathStroker ps;
    ps.setCapStyle(mPen.capStyle());
    ps.setWidth(mPen.widthF() + 10);
    ps.setJoinStyle(mPen.joinStyle());
    ps.setMiterLimit(mPen.miterLimit());
    return ps.createStroke(mPath);
}

void QtGvEdge::setLabel(const QString &label)
{
    setAttribute("xlabel", label);
}

void QtGvEdge::paint(QPainter * painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->save();

    if(isSelected())
    {
        QPen tpen(mPen);
        tpen.setColor(mPen.color().darker(120));
        tpen.setStyle(Qt::DotLine);
        painter->setPen(tpen);
    }
    else
        painter->setPen(mPen);


    painter->drawPath(mPath);

    /*
    QRectF pp = mPath.controlPointRect();
    if(pp.width() < pp.height())
    {
        painter->save();
        painter->translate(mLabelRect.topLeft());
        painter->rotate(90);
        painter->drawText(QRectF(QPointF(0, -mLabelRect.width()), mLabelRect.size()), Qt::AlignCenter, mLabel);
        painter->restore();
    }
    else
    */
    painter->drawText(mLabelRect, Qt::AlignCenter, mLabel);

    painter->setBrush(QBrush(mPen.color(), Qt::SolidPattern));
    painter->drawPolygon(mHeadArrow);
    painter->drawPolygon(mTailArrow);
    painter->restore();
}

void QtGvEdge::setAttribute(const QString &name, const QString &value)
{
    char empty[] = "";
    agsafeset(mEdge, name.toLocal8Bit().data(), value.toLocal8Bit().data(), empty);
}

QString QtGvEdge::getAttribute(const QString &name) const
{
    char* value = agget(mEdge, name.toLocal8Bit().data());
    if(value) return value;
    return QString();
}

void QtGvEdge::updateLayout(qreal gheight)
{
    prepareGeometryChange();

    const splines* spl = ED_spl(mEdge);
    mPath = ToPath(spl, gheight);

    //Edge arrows
    if((spl->list != 0) && (spl->list->size%3 == 1))
    {
        if(spl->list->sflag)
        {
            mTailArrow = ToArrow(QLineF(ToPoint(spl->list->list[0], gheight), ToPoint(spl->list->sp, gheight)));
        }

        if(spl->list->eflag)
        {
            mHeadArrow = ToArrow(QLineF(ToPoint(spl->list->list[spl->list->size-1], gheight), ToPoint(spl->list->ep, gheight)));

        }
    }

    mPen.setWidth(1);
    mPen.setColor(ToColor(getAttribute("color")));
    mPen.setStyle(ToPenStyle(getAttribute("style")));

    //Edge label
    textlabel_t *xlabel = ED_xlabel(mEdge);
    if(xlabel)
    {
        mLabel = xlabel->text;
        mLabelRect.setSize(QSize(xlabel->dimen.x, xlabel->dimen.y));
        mLabelRect.moveCenter(ToPoint(xlabel->pos, gheight));
    }

    setToolTip(getAttribute("tooltip"));
}

QPolygonF QtGvEdge::ToArrow(const QLineF &line) const
{
    QLineF n = line.normalVector();
    QPointF o(n.dx() / 3.0, n.dy() / 3.0);

    //Only support normal arrow type
    QPolygonF polygon;
    polygon.append(line.p1() + o);
    polygon.append(line.p2());
    polygon.append(line.p1() - o);

    return polygon;
}
