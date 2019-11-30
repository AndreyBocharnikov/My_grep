#pragma once

#include <QObject>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

struct thread_for_grep : QObject
{
private:
    Q_OBJECT
public:
    struct result
    {
        result();
        uint32_t size() const;
        std::vector<QString> targes;
        bool incomplete;
        uint32_t id;
    };

    struct hesh
    {
        static uint32_t multy(uint32_t a, uint32_t b);

        static uint32_t add(uint32_t a, uint32_t b);

        static uint32_t substract(uint32_t a, uint32_t b);

        static void precalc(int target_length);

        static std::vector<uint32_t> compute_hash(QString const& input);

        static uint32_t compute_hash_word(QString const& input);

        static bool same_hesh(int r, int l, std::vector<uint32_t> const& hashes, uint32_t target_hash);

        static const uint32_t BUFFER_SIZE = 4096, pow = 269, mod = 1000000007;
        static const char min_char = std::numeric_limits<char>::min();
        static std::vector <uint32_t> pows;
    };

    thread_for_grep();

    ~thread_for_grep();

    void set_grep(QString const& str, bool disable_warnings);
    QString get_result(uint32_t) const;
    std::vector<QString> get_warnings() const;
signals:
    void result_changed();
    void warning_or_error();
    void status_bar_changed();

private:
    void grep();
    void recursive_search(QString const& path, uint32_t target, uint32_t target_length);
    void queue_callback();
    void warning_or_error_callback();
    void status_bar_callback();
    Q_INVOKABLE void queue_emit();
    Q_INVOKABLE void warning_or_error_emit();
    Q_INVOKABLE void status_bar_emit();
public:
    result current_result;
    int get_progress() const;
private:
    mutable std::mutex m;
    QString string_to_grep;
    QString start_path;
    bool quit;
    bool callback_queued;
    std::atomic<bool> cancel;
    std::condition_variable has_work_cv;
    std::vector<QString> warning_or_errors;
    bool disable_warnings;
    double directory_size, current_done, previous_done;
    std::thread thread;
};
