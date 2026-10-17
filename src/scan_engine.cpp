#include "scan_engine.h"

#include <utility>

namespace ohboyscan {

ScanEngine::ScanEngine(ProgressCallback progress)
    : m_progress(std::move(progress)) {
    m_worker = std::thread(&ScanEngine::worker_loop, this);
}

ScanEngine::~ScanEngine() { stop(); }

void ScanEngine::submit(Task task) {
    if (!task) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stop) {
            return;
        }
        m_queue.push_back(std::move(task));
    }
    m_cv.notify_one();
}

void ScanEngine::report(const std::string& message) {
    ProgressCallback callback;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callback = m_progress;
    }
    if (callback) {
        callback(message);
    }
}

void ScanEngine::stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stop) {
            return;
        }
        m_stop = true;
    }
    m_cv.notify_all();
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void ScanEngine::worker_loop() {
    for (;;) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return m_stop || !m_queue.empty(); });
            if (m_stop && m_queue.empty()) {
                return;
            }
            task = std::move(m_queue.front());
            m_queue.pop_front();
        }
        task();
    }
}

} // namespace ohboyscan
