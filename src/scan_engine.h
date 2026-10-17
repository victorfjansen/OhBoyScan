#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace ohboyscan {

// Runs scan tasks on a single background worker thread and reports progress
// through a callback. RAII-managed: the thread is started on construction and
// joined on destruction/stop().
//
// Note: the progress callback runs on the worker thread. UI callers must
// marshal updates to the UI thread (e.g. slint::invoke_from_event_loop).
class ScanEngine {
public:
    using Task = std::function<void()>;
    using ProgressCallback = std::function<void(const std::string&)>;

    explicit ScanEngine(ProgressCallback progress = {});
    ~ScanEngine();

    ScanEngine(const ScanEngine&) = delete;
    ScanEngine& operator=(const ScanEngine&) = delete;

    void submit(Task task);
    void report(const std::string& message);
    void stop();

private:
    void worker_loop();

    ProgressCallback m_progress;
    std::thread m_worker;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<Task> m_queue;
    bool m_stop = false;
};

} // namespace ohboyscan
