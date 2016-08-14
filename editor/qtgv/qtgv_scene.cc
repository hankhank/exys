
#include <QDebug>
#include <QGraphicsSceneContextMenuEvent>

#include "helpers.h"
#include "qtgv_scene.h"
#include "qtgv_node.h"
#include "qtgv_edge.h"

typedef struct 
{
    const char *data;
    int len;
    int cur;
} rdr_t;

int memiofread(void *chan, char *buf, int bufsize)
{
    const char *ptr;
    char *optr;
    char c;
    int l;
    rdr_t *s;

    if (bufsize == 0) return 0;
    s = (rdr_t *) chan;
    if (s->cur >= s->len)
            return 0;
    l = 0;
    ptr = s->data + s->cur;
    optr = buf;
    do {
        *optr++ = c = *ptr++;
        l++;
    } while (c && (c != '\n') && (l < bufsize));
    s->cur += l;
    return l;
}

Agraph_t *agmemread2(const char *cp)
{
    Agraph_t* g;
    rdr_t rdr;
    Agdisc_t disc;
    Agiodisc_t memIoDisc;

    memIoDisc.afread = memiofread;
    memIoDisc.putstr = AgIoDisc.putstr;
    memIoDisc.flush = AgIoDisc.flush;
    rdr.data = cp;
    rdr.len = strlen(cp);
    rdr.cur = 0;

    disc.mem = &AgMemDisc;
    disc.id = &AgIdDisc;
    disc.io = &memIoDisc;
    g = agread (&rdr, &disc);
    return g;
}

QtGvScene::QtGvScene(const QString &name, QObject *parent) 
: QGraphicsScene(parent)
{
    mContext = gvContext();
    mGraph = agopen(name.toLocal8Bit().data(), Agdirected, NULL);
}

QtGvScene::~QtGvScene()
{
}

void QtGvScene::LoadLayout(const QString &text)
{
    Clear();
    mGraph = agmemread2(text.toLocal8Bit().constData());
    if(!mGraph) return;

    if(gvLayout(mContext, mGraph, "dot") != 0)
    {
        qCritical()<<"Layout render error"<<agerrors()<<QString::fromLocal8Bit(aglasterr());
        return;
    }

    //Debug output
		//gvRenderFilename(mContext, mGraph.get(), "png", "debug.png");

    //Read nodes and edges
    for
    (
        auto* node = agfstnode(mGraph);
        node != NULL;
        node = agnxtnode(mGraph, node)
    )
    {
        auto *inode = new QtGvNode(node, nullptr);
        inode->updateLayout(GraphHeight(mGraph));
        addItem(inode);
        for
        (
            auto* edge = agfstout(mGraph, node);
            edge != NULL;
            edge = agnxtout(mGraph, edge)
        )
        {
            auto *iedge = new QtGvEdge(edge, nullptr);
            iedge->updateLayout(GraphHeight(mGraph));
            addItem(iedge);
        }
    }
    update();
}

void QtGvScene::Clear()
{
    mNodes.clear();
    mEdges.clear();
    QGraphicsScene::clear();
}

void QtGvScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *contextMenuEvent)
{
    //QGraphicsItem *item = itemAt(contextMenuEvent->scenePos(), QTransform());
    //if(item)
    //{
    //    item->setSelected(true);
    //    if(item->type() == QtGvNode::Type)
    //        emit nodeContextMenu(qgraphicsitem_cast<QtGvNode*>(item));
    //    else if(item->type() == QtGvEdge::Type)
    //        emit edgeContextMenu(qgraphicsitem_cast<QtGvEdge*>(item));
    //    else if(item->type() == QtGvSubGraph::Type)
    //        emit subGraphContextMenu(qgraphicsitem_cast<QtGvSubGraph*>(item));
    //    else
    //        emit graphContextMenuEvent();
    //}
    QGraphicsScene::contextMenuEvent(contextMenuEvent);
}

void QtGvScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    //QGraphicsItem *item = itemAt(mouseEvent->scenePos(), QTransform());
    //if(item)
    //{
    //    if(item->type() == QtGvNode::Type)
    //        emit nodeDoubleClick(qgraphicsitem_cast<QtGvNode*>(item));
    //    else if(item->type() == QtGvEdge::Type)
    //        emit edgeDoubleClick(qgraphicsitem_cast<QtGvEdge*>(item));
    //    else if(item->type() == QtGvSubGraph::Type)
    //        emit subGraphDoubleClick(qgraphicsitem_cast<QtGvSubGraph*>(item));
    //}
    QGraphicsScene::mouseDoubleClickEvent(mouseEvent);
}

