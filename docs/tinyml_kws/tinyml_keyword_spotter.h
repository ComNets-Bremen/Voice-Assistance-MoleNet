#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "kiss_fftr.h"

namespace dl {
class Model;
}

class TinyMlKeywordSpotter {
public:
    using ResultCallback = std::function<void(const std::string&)>;

    TinyMlKeywordSpotter();
    ~TinyMlKeywordSpotter();

    bool Initialize(ResultCallback callback);
    void SetEnabled(bool enabled);
    void Feed(const int16_t* samples, size_t count);

private:
    static void WorkerTask(void* arg);
    void ProcessWindow();

    std::unique_ptr<dl::Model> model_;
    ResultCallback callback_;
    TaskHandle_t worker_task_ = nullptr;
    std::vector<int16_t> capture_;
    std::vector<float> mfcc_;
    std::vector<kiss_fft_scalar> fft_input_;
    std::vector<kiss_fft_cpx> fft_output_;
    std::vector<float> mel_energies_;
    std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::atomic<bool> initialization_failed_{false};
    bool collecting_ = false;
    bool window_ready_ = false;
    bool work_buffers_ready_ = false;
    bool initialized_ = false;
};
