#include "tinyml_keyword_spotter.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <vector>

#include <esp_log.h>
#include <dl_model_base.hpp>
#include <dl_tensor_base.hpp>
#include <kiss_fftr.h>

namespace {
constexpr char TAG[] = "TinyMlKws";
constexpr int kAudioSamples = 16000;
constexpr int kFftSize = 1024;
constexpr int kWindowLength = 800;
constexpr int kHopLength = 160;
constexpr int kMelBands = 128;
constexpr int kMfccCount = 40;
constexpr int kFrames = 101;
constexpr int kFrequencyBins = kFftSize / 2 + 1;
constexpr int kWindowOffset = (kFftSize - kWindowLength) / 2;
constexpr float kMinRms = 0.030f;
constexpr float kClipLevel = 0.98f;
constexpr float kMaxClippedFraction = 0.01f;
constexpr float kConfidenceThreshold = 0.70f;
constexpr const char* kLabels[] = {"YES", "NO", "UP"};

extern const uint8_t kws_model[] asm("_binary_kws_3class_espdl_start");
extern const uint8_t hann_window_bin[] asm("_binary_hann_window_bin_start");
extern const uint8_t mel_filterbank_transposed_bin[]
    asm("_binary_mel_filterbank_transposed_bin_start");
extern const uint8_t dct_matrix_bin[] asm("_binary_dct_matrix_bin_start");

float ReflectedSample(const int16_t* audio, int index, float mean) {
    if (index < 0) index = -index;
    if (index >= kAudioSamples) index = 2 * kAudioSamples - 2 - index;
    return static_cast<float>(audio[index]) / 32768.0f - mean;
}

bool ComputeMfcc(const int16_t* audio, float mean, float* output,
                 kiss_fft_scalar* fft_input, kiss_fft_cpx* fft_output,
                 float* mel_energies) {
    const auto* hann = reinterpret_cast<const float*>(hann_window_bin);
    const auto* filterbank = reinterpret_cast<const float*>(mel_filterbank_transposed_bin);
    const auto* dct = reinterpret_cast<const float*>(dct_matrix_bin);
    kiss_fftr_cfg fft = kiss_fftr_alloc(kFftSize, 0, nullptr, nullptr);
    if (fft == nullptr) return false;

    float max_db = -1.0e30f;

    // First pass finds the global dB reference used by the training pipeline.
    for (int frame = 0; frame < kFrames; ++frame) {
        const int frame_start = frame * kHopLength - kFftSize / 2;
        for (int n = 0; n < kFftSize; ++n) {
            if (n < kWindowOffset || n >= kWindowOffset + kWindowLength) {
                fft_input[n] = 0;
            } else {
                fft_input[n] = ReflectedSample(audio, frame_start + n, mean) * hann[n - kWindowOffset];
            }
        }
        kiss_fftr(fft, fft_input, fft_output);
        for (int mel = 0; mel < kMelBands; ++mel) {
            float energy = 0.0f;
            const float* filter = filterbank + mel * kFrequencyBins;
            for (int k = 0; k < kFrequencyBins; ++k) {
                const float re = fft_output[k].r;
                const float im = fft_output[k].i;
                energy += (re * re + im * im) * filter[k];
            }
            const float db = 10.0f * log10f(std::max(energy, 1.0e-10f));
            max_db = std::max(max_db, db);
        }
        if ((frame & 7) == 7) vTaskDelay(1);
    }

    const float minimum_db = max_db - 80.0f;
    // Second pass recomputes each frame and immediately applies the DCT. This
    // avoids a 50 KB mel spectrogram allocation on boards without PSRAM.
    for (int frame = 0; frame < kFrames; ++frame) {
        const int frame_start = frame * kHopLength - kFftSize / 2;
        for (int n = 0; n < kFftSize; ++n) {
            if (n < kWindowOffset || n >= kWindowOffset + kWindowLength) {
                fft_input[n] = 0;
            } else {
                fft_input[n] = ReflectedSample(audio, frame_start + n, mean) * hann[n - kWindowOffset];
            }
        }
        kiss_fftr(fft, fft_input, fft_output);
        for (int mel = 0; mel < kMelBands; ++mel) {
            float energy = 0.0f;
            const float* filter = filterbank + mel * kFrequencyBins;
            for (int k = 0; k < kFrequencyBins; ++k) {
                const float re = fft_output[k].r;
                const float im = fft_output[k].i;
                energy += (re * re + im * im) * filter[k];
            }
            mel_energies[mel] = std::max(
                10.0f * log10f(std::max(energy, 1.0e-10f)), minimum_db);
        }
        for (int coeff = 0; coeff < kMfccCount; ++coeff) {
            float sum = 0.0f;
            for (int mel = 0; mel < kMelBands; ++mel) {
                sum += mel_energies[mel] * dct[mel * kMfccCount + coeff];
            }
            output[coeff * kFrames + frame] = sum;
        }
        if ((frame & 7) == 7) vTaskDelay(1);
    }
    free(fft);
    return true;
}

bool ValidateAudio(const int16_t* capture, float& mean) {
    double mean_accumulator = 0.0;
    for (int i = 0; i < kAudioSamples; ++i) {
        mean_accumulator += static_cast<float>(capture[i]) / 32768.0f;
    }
    mean_accumulator /= static_cast<double>(kAudioSamples);

    double sum_squared = 0.0;
    int clipped = 0;
    for (int i = 0; i < kAudioSamples; ++i) {
        const float sample = static_cast<float>(capture[i]) / 32768.0f -
                             static_cast<float>(mean_accumulator);
        sum_squared += sample * sample;
        if (fabsf(sample) >= kClipLevel) ++clipped;
    }
    const float rms = sqrtf(static_cast<float>(sum_squared / kAudioSamples));
    const float clipped_fraction = static_cast<float>(clipped) / kAudioSamples;
    if (rms < kMinRms || clipped_fraction > kMaxClippedFraction) {
        ESP_LOGD(TAG, "Ignoring quiet/clipped audio (RMS %.3f, clipped %.3f)", rms,
                 clipped_fraction);
        return false;
    }
    mean = static_cast<float>(mean_accumulator);
    return true;
}

bool Predict(dl::Model* model, const float* mfcc, int& prediction, float& confidence) {
    auto* input = model->get_input();
    auto* output = model->get_output();
    if (input == nullptr || output == nullptr ||
        !input->assign(input->get_shape(), mfcc, 0, dl::DATA_TYPE_FLOAT)) {
        return false;
    }
    model->run();
    dl::TensorBase float_output(output->get_shape(), nullptr, 0, dl::DATA_TYPE_FLOAT);
    if (!float_output.assign(output)) return false;
    const float* logits = float_output.get_element_ptr<float>();
    if (logits == nullptr) return false;

    float max_logit = std::max({logits[0], logits[1], logits[2]});
    float probabilities[3];
    float total = 0.0f;
    for (int i = 0; i < 3; ++i) {
        probabilities[i] = expf(logits[i] - max_logit);
        total += probabilities[i];
    }
    prediction = 0;
    for (int i = 0; i < 3; ++i) {
        probabilities[i] /= total;
        if (probabilities[i] > probabilities[prediction]) prediction = i;
    }
    confidence = probabilities[prediction];
    return true;
}
}  // namespace

TinyMlKeywordSpotter::TinyMlKeywordSpotter() = default;

TinyMlKeywordSpotter::~TinyMlKeywordSpotter() {
    enabled_.store(false);
    if (worker_task_ != nullptr) {
        xTaskNotifyGive(worker_task_);
        vTaskDelete(worker_task_);
    }
}

bool TinyMlKeywordSpotter::Initialize(ResultCallback callback) {
    callback_ = std::move(callback);
    if (xTaskCreate(WorkerTask, "tinyml_kws", 8192, this, 4, &worker_task_) != pdPASS) {
        return false;
    }
    initialized_ = true;
    ESP_LOGI(TAG, "TinyML keyword spotter ready; model loads when offline mode begins");
    return true;
}

void TinyMlKeywordSpotter::SetEnabled(bool enabled) {
    if (!initialized_) return;
    const bool was_enabled = enabled_.exchange(enabled);
    if (!enabled) return;
    if (!was_enabled) initialization_failed_.store(false);
    std::lock_guard<std::mutex> lock(mutex_);
    if (!window_ready_) {
        capture_.clear();
        collecting_ = false;
    }
    xTaskNotifyGive(worker_task_);
}

void TinyMlKeywordSpotter::Feed(const int16_t* samples, size_t count) {
    if (!enabled_.load() || samples == nullptr || count == 0) return;
    bool ready = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_.load() || !collecting_) return;
        const size_t needed = kAudioSamples - capture_.size();
        const size_t copy_count = std::min(needed, count);
        capture_.insert(capture_.end(), samples, samples + copy_count);
        if (capture_.size() == kAudioSamples) {
            collecting_ = false;
            window_ready_ = true;
            ready = true;
        }
    }
    if (ready && worker_task_ != nullptr) xTaskNotifyGive(worker_task_);
}

void TinyMlKeywordSpotter::WorkerTask(void* arg) {
    auto* self = static_cast<TinyMlKeywordSpotter*>(arg);
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (self->enabled_.load() && !self->initialization_failed_.load() &&
            self->model_ == nullptr) {
            try {
                self->model_.reset(new (std::nothrow) dl::Model(
                    reinterpret_cast<const char*>(kws_model), fbs::MODEL_LOCATION_IN_FLASH_RODATA));
            } catch (const std::bad_alloc&) {
                ESP_LOGE(TAG, "Not enough RAM to load the ESP-DL model");
            }
            if (self->model_ == nullptr || self->model_->get_input() == nullptr ||
                self->model_->get_output() == nullptr) {
                self->model_.reset();
                self->initialization_failed_.store(true);
                ESP_LOGE(TAG, "ESP-DL model failed to initialize; local command detection stopped");
                continue;
            }
            ESP_LOGI(TAG, "YES/NO/UP ESP-DL model loaded in offline mode");
        }
        if (self->enabled_.load() && !self->initialization_failed_.load() &&
            self->model_ != nullptr && !self->work_buffers_ready_) {
            try {
                self->capture_.reserve(kAudioSamples);
                self->mfcc_.resize(kMfccCount * kFrames);
                self->fft_input_.resize(kFftSize);
                self->fft_output_.resize(kFrequencyBins);
                self->mel_energies_.resize(kMelBands);
                self->work_buffers_ready_ = true;
            } catch (const std::bad_alloc&) {
                self->initialization_failed_.store(true);
                ESP_LOGE(TAG, "Not enough RAM to allocate TinyML audio/MFCC buffers");
            }
        }
        bool process_window;
        {
            std::lock_guard<std::mutex> lock(self->mutex_);
            process_window = self->window_ready_ && self->enabled_.load() &&
                             !self->initialization_failed_.load() && self->model_ != nullptr;
        }
        if (process_window) {
            try {
                self->ProcessWindow();
            } catch (const std::bad_alloc&) {
                ESP_LOGE(TAG, "Not enough RAM while processing a keyword window");
            }
        }
        {
            std::lock_guard<std::mutex> lock(self->mutex_);
            if (self->window_ready_) {
                if (self->enabled_.load() && self->model_ != nullptr) {
                    // Keep half a second so commands near a window boundary
                    // are still present in the next one-second inference.
                    self->capture_.erase(self->capture_.begin(),
                                         self->capture_.begin() + kAudioSamples / 2);
                } else {
                    self->capture_.clear();
                }
                self->window_ready_ = false;
            }
            self->collecting_ = self->enabled_.load() && !self->initialization_failed_.load() &&
                                self->model_ != nullptr &&
                                self->work_buffers_ready_;
        }
    }
}

void TinyMlKeywordSpotter::ProcessWindow() {
    float mean = 0.0f;
    if (!ValidateAudio(capture_.data(), mean) ||
        !ComputeMfcc(capture_.data(), mean, mfcc_.data(), fft_input_.data(),
                     fft_output_.data(), mel_energies_.data())) {
        return;
    }
    int prediction = -1;
    float confidence = 0.0f;
    if (!Predict(model_.get(), mfcc_.data(), prediction, confidence)) {
        ESP_LOGE(TAG, "Model inference failed");
        return;
    }
    ESP_LOGI(TAG, "KWS prediction=%s confidence=%.1f%%",
             confidence >= kConfidenceThreshold ? kLabels[prediction] : "UNKNOWN",
             confidence * 100.0f);
    if (enabled_.load() && confidence >= kConfidenceThreshold && callback_) {
        callback_(kLabels[prediction]);
    }
}
