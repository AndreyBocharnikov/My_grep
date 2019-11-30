#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDir>
#include <QMessageBox>
#include <iostream>
#include <QTextStream> 
#include <vector>
#include <numeric>
#include <QChar>
#include <QString>
#include <QDirIterator>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->progressBar->setValue(0);
    ui->lineEdit_2->setReadOnly(true);
    QPalette *palette = new QPalette();
    palette->setColor(QPalette::Base,Qt::black);
    palette->setColor(QPalette::Text,Qt::white);
    ui->lineEdit_2->setPalette(*palette);

    ui->listWidget_2->addItem("Errors and warnings:");
    thread_for_grep::hesh::precalc(2 * thread_for_grep::hesh::BUFFER_SIZE);
    connect(ui->pushButton, &QPushButton::clicked, this, [this]
    {
        ui->progressBar->setValue(0);
        thread_grep.set_grep(ui->lineEdit->text(), ui->lineEdit_3->text(), ui->checkBox->isChecked());
    });

    connect(&thread_grep, &thread_for_grep::result_changed, this, [this]
    {
        // std::cout << thread_grep.string_to_grep.toUtf8().data() << " " << thread_grep.result_id << " " << thread_grep.get_result_size() << std::endl;
        if (thread_grep.current_result.size() == 0) {
            ui->listWidget->clear();
        } else {
            QString res = thread_grep.get_result(thread_grep.current_result.id);
            thread_grep.current_result.id++;
            ui->listWidget->addItem(res);
        }
    });

    connect(&thread_grep, &thread_for_grep::warning_or_error, this, [this]
    {
        auto current_warnings = thread_grep.get_warnings();
        for (auto const& w : current_warnings)
            ui->listWidget_2->addItem(w);
    });

    connect(&thread_grep, &thread_for_grep::status_bar_changed, this, [this]
    {
        ui->progressBar->setValue(thread_grep.get_progress());
    });
    delete palette;
}

MainWindow::~MainWindow()
{
    delete ui;
}
