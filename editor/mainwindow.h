
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "highlighter.h"
#include "linetextedit.h"
#include "qtgv_scene.h"

#include <QMainWindow>


//! [0]
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

    CodeEditor *editor;
    Highlighter *highlighter;
    QtGvScene    *scene;
    QGraphicsView *view;
};
//! [0]

#endif // MAINWINDOW_H
