#pragma once

#include <cgraph.h>
#include <gvc.h>

#include <memory>

#include <QGraphicsScene>

class QtGvNode;
class QtGvEdge;


class QtGvScene : public QGraphicsScene
{
    Q_OBJECT
public:

    explicit QtGvScene(const QString &name, QObject *parent = 0);
    virtual ~QtGvScene();

    void LoadLayout(const QString &text);
    void SetError(const QString &text);
    void Clear();

signals:
    void nodeContextMenu(QtGvNode* node);
    void nodeDoubleClick(QtGvNode* node);

    void edgeContextMenu(QtGvEdge* edge);
    void edgeDoubleClick(QtGvEdge* edge);

    void graphContextMenuEvent();
    
public slots:

    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent * contextMenuEvent);
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent * mouseEvent);

private:
    //std::unique_ptr<GVC_t, gvFreeContext> mContext;
    //std::unique_ptr<Agraph_t, agclose> mGraph;

    GVC_t* mContext;
    Agraph_t* mGraph;

    std::list<QtGvNode*> mNodes;
    std::list<QtGvEdge*> mEdges;
};
