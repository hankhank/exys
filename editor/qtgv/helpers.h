#pragma once

#include <QPointF>
#include <QPolygonF>
#include <QPainterPath>
#include <QColor>

#include <gvc.h>
#include <cgraph.h>

const qreal DotDefaultDPI = 72.0;

inline QPointF ToPoint(pointf p, qreal gheight)
{
    return QPointF(p.x, gheight - p.y);
}

inline qreal GraphHeight(Agraph_t *graph)
{
    return GD_bb(graph).UR.y;
}

inline QColor ToColor(const QString &color)
{
    return QColor(color);
}

