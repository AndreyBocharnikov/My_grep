#include "thread_for_grep.h"
#include <QDir>
#include <QString>
#include <QTextStream>
#include <QMessageBox>

#include <iostream>
thread_for_grep::result::result()
    : incomplete(false), id(0)
{}

uint32_t thread_for_grep::result::size() const
{
    return targes.size();
}

thread_for_grep::thread_for_grep()
    : string_to_grep("")
    , start_path("")
    , quit(false)
    , callback_queued(false)
    , cancel(false)
    , directory_size(0)
    , current_done(0)
    , previous_done(0)
    , thread([this]
    {
        for (;;)
        {
            std::unique_lock<std::mutex> lg(m);
            has_work_cv.wait(lg, [this]
            {
                return string_to_grep != "" || quit;
            });
            if (quit)
                break;
            // std::cout << "doen't sleep anymore " << string_to_grep.toUtf8().data() << std::endl;
            current_result.incomplete = true;
            current_result.targes.clear();
            warning_or_errors.clear();
            queue_callback();
            status_bar_callback();

            lg.unlock();
            grep();
            lg.lock();

            current_result.incomplete = cancel.load();
            // queue_callback();
            if (!cancel.load()) {
                string_to_grep = "";
                previous_done = 100;
                status_bar_callback();
            }
            cancel.store(false);
        }
    })
{}

thread_for_grep::~thread_for_grep()
{
    cancel.store(true);
    {
        std::unique_lock<std::mutex> lg(m);
        quit = true;
        has_work_cv.notify_all();
    }
    thread.join();
}

void thread_for_grep::set_grep(QString const& path, QString const& grep_string, bool warnings)
{
    std::unique_lock<std::mutex> lg(m);
    disable_warnings = warnings;
    current_result.id = 0;
    previous_done = 0;
    current_result.targes.clear();
    warning_or_errors.clear();
    if (string_to_grep != "")
        cancel.store(true);

    if (path[0] == '~') {
        start_path = QDir::homePath() + path.mid(1);
    } else {
        start_path = path;
    }
    if (grep_string.length() == 0) {
        warning_or_errors.push_back("String to grep should not be empty");
        warning_or_error_callback();
        return;
    }
    string_to_grep = grep_string;
    if (uint32_t(string_to_grep.length()) > 2 * hesh::BUFFER_SIZE) {
        warning_or_errors.push_back("String to grep is to big, it should be less than 8192");
        warning_or_error_callback();
        // string_to_grep = "";
        return;
    }
    QFileInfo path_info(start_path);
    if (!path_info.exists() || !path_info.isDir()) {
        warning_or_errors.push_back("Error: file " + start_path + " doesn't exist");
        warning_or_error_callback();
        return;
    }

    directory_size = QDir(start_path).count() - 2;
    current_done = 0;
    has_work_cv.notify_all();
}

QString thread_for_grep::get_result(uint32_t id) const
{
    std::lock_guard<std::mutex> lg(m);
    return current_result.targes[id];
}

std::vector<QString> thread_for_grep::get_warnings() const
{
    std::lock_guard<std::mutex> lg(m);
    return warning_or_errors;
}

void thread_for_grep::grep()
{
    uint32_t target = hesh::compute_hash_word(string_to_grep);
    recursive_search(start_path, target, string_to_grep.length());
}

void thread_for_grep::recursive_search(QString const& path, uint32_t target, uint32_t target_length) {
    QDir required_dir(path);
    // std::cout << "in recursive " << target <<  std::endl;

    QStringList files = required_dir.entryList(QDir::Files); // TODO add filters
    QStringList directories = required_dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs);

    foreach(QString current_path, files) { // for in files
        QFileInfo path_info(current_path);
        QFile file(path + '/' + current_path);
        if (cancel.load()) {
            return;
        }
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream in(&file);
            QString rest;
            uint32_t line_number = 1, pos_number = 0;
            while (!in.atEnd()) {
                if (cancel.load())
                    return; // file closes in the ~
                QString text = in.read(hesh::BUFFER_SIZE);
                text = rest + text;
                auto hashes = hesh::compute_hash(text);
                for (int i = target_length - 1; i < text.length(); i++) {
                    uint32_t l = i - target_length + 1;
                    if (text[l] == '\n') {
                        line_number++;
                        pos_number = 0;
                    } else {
                        pos_number++;
                    }
                    if (hesh::same_hesh(i, l, hashes, target)) {
                        {
                            std::unique_lock<std::mutex> lg(m);
                            current_result.targes.push_back(path + '/' + current_path + " at line number " + QString::number(line_number) +
                                                            " at position number " + QString::number(pos_number));
                            queue_callback();
                        }
                    }
                }
                rest = text.mid(text.length() - target_length + 1);
            }
        } else {
            if (!disable_warnings) {
                warning_or_errors.push_back("Warning: file " + path + " is unreadable");
                warning_or_error_callback();
            }
        }
    }

    foreach (QString current_dir, directories) {
        if (cancel.load())
            return;
        recursive_search(path + '/' + current_dir, target, target_length);
        if (path == start_path) {
            current_done++;
            double tmp = 100 * current_done / directory_size;
            if (tmp - previous_done > 1) {
                previous_done = tmp;
                status_bar_callback();
            }
        }
    }
}

void thread_for_grep::queue_callback()
{
    QMetaObject::invokeMethod(this, "queue_emit",
                              Qt::QueuedConnection);
}

void thread_for_grep::queue_emit()
{
    emit result_changed();
}

void thread_for_grep::warning_or_error_callback()
{
    QMetaObject::invokeMethod(this, "warning_or_error_emit",
                              Qt::QueuedConnection);
}

void thread_for_grep::warning_or_error_emit()
{
    emit warning_or_error();
}

void thread_for_grep::status_bar_callback()
{
    QMetaObject::invokeMethod(this, "status_bar_emit",
                              Qt::QueuedConnection);
}

void thread_for_grep::status_bar_emit()
{
    emit status_bar_changed();
}

int thread_for_grep::get_progress() const
{
    return int(previous_done);
}

uint32_t thread_for_grep::hesh::multy(uint32_t a, uint32_t b) {
    return (1LL * a * b) % mod;
}

uint32_t thread_for_grep::hesh::add(uint32_t a, uint32_t b) {
    return (a + b) % mod;
}

uint32_t thread_for_grep::hesh::substract(uint32_t a, uint32_t b) {
    return a > b ? a - b : a - b + mod;
}

void thread_for_grep::hesh::precalc(int target_length) {
    pows.clear();
    pows.resize(BUFFER_SIZE + target_length);
    pows[0] = 1;
    for (uint32_t i = 1; i < BUFFER_SIZE + target_length; i++) {
        pows[i] = multy(pows[i - 1], pow);
    }
}

std::vector<uint32_t> thread_for_grep::hesh::compute_hash(const QString &input) {
    int length = input.length();
    std::vector <uint32_t> result(length);
    result[0] = input.toUtf8().at(0) - min_char + 1;
    for (int i = 1; i < length; i++) {
        result[i] = add(result[i - 1], multy(input.toUtf8().at(i) - min_char + 1, pows[i]));
    }
    return result;
}

uint32_t thread_for_grep::hesh::compute_hash_word(const QString &input) {
    uint32_t result = 0;
    for (int i = 0; i < input.length(); i++) {
        result = add(result, multy(input.toUtf8()[i] - min_char + 1, pows[i]));
    }
    return result;
}

bool thread_for_grep::hesh::same_hesh(int r, int l, const std::vector<uint32_t> &hashes, uint32_t target_hash) {
    uint32_t substaction = 0;
    if (l > 0)
        substaction = hashes[l - 1];
    return substract(hashes[r], substaction) == multy(target_hash, pows[l]);
}

std::vector<uint32_t> thread_for_grep::hesh::pows;
