#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <thread_for_grep.h>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void recursive_search(QString const& path, uint32_t, uint32_t target_length, bool first=true);

private:
    Ui::MainWindow *ui;
    thread_for_grep thread_grep;
};

#endif // MAINWINDOW_H
