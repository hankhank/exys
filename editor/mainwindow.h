
#pragma once

#include "highlighter.h"
#include "linetextedit.h"
#include "qtgv_scene.h"

#include <QMainWindow>

class QTableWidget;

namespace Exys 
{
struct GraphState;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);

public slots:
    void about();
    void newFile();
    void openFile(const QString &path = QString());

    void textChanged();
    void fileChanged(const QString&);

private:
    void setupEditor();
    void setupFileMenu();
    void setupHelpMenu();
    void setupTable();
    void setTable(const Exys::GraphState &states);

    CodeEditor *editor;
    Highlighter *highlighter;
    QtGvScene    *scene;
    QGraphicsView *view;
    QTableWidget *table;
};
