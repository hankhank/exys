#include <QtWidgets>
#include <memory>

#include "mainwindow.h"
#include "exys.h"
#include "executioner.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupFileMenu();
    setupHelpMenu();
    setupEditor();
    setupTable();

    scene = new QtGvScene("DEMO", this);
    view = new QGraphicsView(scene);

    auto* graphWidget = new QWidget(this);
    auto* hsplit = new QHBoxLayout(this);

    hsplit->addWidget(view);
    hsplit->addWidget(table);
    graphWidget->setLayout(hsplit);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(editor);
    splitter->addWidget(graphWidget);

    setCentralWidget(splitter);
    setWindowTitle(tr("Exys Editor"));

    connect(editor, SIGNAL(textChanged()), SLOT(textChanged()));

    auto args = QCoreApplication::arguments();
    if(args.size() > 1)
    {
        auto watcher = new QFileSystemWatcher(this);
        watcher->addPath(args[1]);
        connect(watcher, SIGNAL(fileChanged(const QString&)), this, SLOT(fileChanged(const QString&)));
        fileChanged(args[1]);
    }
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About Exys Editor"),
                tr("<p><b>Exys Editor</b> allows you to express your valuations"));
}

void MainWindow::newFile()
{
    editor->clear();
}

void MainWindow::openFile(const QString &path)
{
    QString fileName = path;

    if (fileName.isNull())
        fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", "Exys Files (*.exys)");

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QFile::ReadOnly | QFile::Text))
            editor->setPlainText(file.readAll());
    }
}

void MainWindow::setupEditor()
{
    QFont font;
    font.setFamily("Courier");
    font.setFixedPitch(true);
    font.setPointSize(10);

    editor = new CodeEditor;
    editor->setFont(font);

    highlighter = new Highlighter(editor->document());

    QFile file("mainwindow.h");
    if (file.open(QFile::ReadOnly | QFile::Text))
        editor->setPlainText(file.readAll());
}

void MainWindow::setupFileMenu()
{
    QMenu *fileMenu = new QMenu(tr("&File"), this);
    menuBar()->addMenu(fileMenu);

    fileMenu->addAction(tr("&New"), this, SLOT(newFile()), QKeySequence::New);
    fileMenu->addAction(tr("&Open..."), this, SLOT(openFile()), QKeySequence::Open);
    fileMenu->addAction(tr("E&xit"), qApp, SLOT(quit()), QKeySequence::Quit);
}

void MainWindow::setupHelpMenu()
{
    QMenu *helpMenu = new QMenu(tr("&Help"), this);
    menuBar()->addMenu(helpMenu);

    helpMenu->addAction(tr("&About"), this, SLOT(about()));
}

void MainWindow::setupTable()
{
    table = new QTableWidget(this);
}

// would be better if states werent in unordered map as we 
// cant actually on them being in the same order for each level
void MainWindow::setTable(const Exys::GraphState &state)
{
    table->clear();
    table->setRowCount(0);
    table->setColumnCount(0);
    if(state.inputs.size() && state.observers.size())
    {
        const auto& inputs = state.inputs;
        const auto& observers = state.observers;

        table->setRowCount(inputs.size());
        table->setColumnCount(inputs.size() + observers.size());

        int col = 0;
        QStringList headers;
        for(const auto& input : inputs)
        {
            headers << input.first;
            int row = 0;
            for(auto val : input.second)
            {
                table->insertRow(row);
                table->setItem(row, col, 
                    new QTableWidgetItem(QString::number(val)));
                ++row;
            }
            ++col;
        }

        for(const auto& observer : observers)
        {
            headers << observer.first;
            int row = 0;
            for(auto val : observer.second)
            {
                table->setItem(row, col, 
                    new QTableWidgetItem(QString::number(val)));
                ++row;
            }
            ++col;
        }
    }
}

void MainWindow::textChanged()
{
    const auto &text = editor->toPlainText().toStdString();
    try
    {
        std::unique_ptr<Exys::Exys> graph = Exys::Exys::Build(text);
        scene->LoadLayout(graph->GetDOTGraph().c_str());
        auto results = Exys::Execute(*graph, buffer.str());
        setTable(std::get<3>(results));
    }
    catch (const Exys::ParseException& e)
    {
        qWarning() << e.GetErrorMessage(text).c_str();
    }
    catch (const Exys::GraphBuildException& e)
    {
        qWarning() << e.GetErrorMessage(text).c_str();
    }
}

void MainWindow::fileChanged(const QString& path)
{
    QFile file(path);
    file.open(QIODevice::ReadOnly);
    QTextStream txt(&file);
    editor->setText(txt.readAll());
}

