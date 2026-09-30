#include "wifi_board.h"
#include "audio_codec.h"
#include "config.h"
#include "display/display.h"
#include "button.h"
#include "application.h"
#include "led/gpio_led.h"

#include <driver/i2s_std.h>
#include <esp_log.h>
#include <vector>

#define TAG "MoleNetV63Mic"

class MicOnlyCodec : public AudioCodec {
public:
    MicOnlyCodec(
        int input_sample_rate,
        int output_sample_rate,
        gpio_num_t mic_sck,
        gpio_num_t mic_ws,
        gpio_num_t mic_din
    ) {
        duplex_ = false;
        input_sample_rate_ = input_sample_rate;
        output_sample_rate_ = output_sample_rate;
        input_channels_ = 1;
        output_channels_ = 1;

        i2s_chan_config_t chan_cfg =
            I2S_CHANNEL_DEFAULT_CONFIG(
                XIAOZHI_I2S_PORT(1),
                I2S_ROLE_MASTER
            );

        chan_cfg.dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM;
        chan_cfg.dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM;

        ESP_ERROR_CHECK(
            i2s_new_channel(
                &chan_cfg,
                nullptr,
                &rx_handle_
            )
        );

        i2s_std_config_t rx_cfg = {
            .clk_cfg = {
                .sample_rate_hz = (uint32_t)input_sample_rate_,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
#ifdef I2S_HW_VERSION_2
                .ext_clk_freq_hz = 0,
#endif
            },

            .slot_cfg = {
                .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                .slot_mode = I2S_SLOT_MODE_MONO,
                .slot_mask = I2S_STD_SLOT_LEFT,
                .ws_width = I2S_DATA_BIT_WIDTH_32BIT,
                .ws_pol = false,
                .bit_shift = true,
#ifdef I2S_HW_VERSION_2
                .left_align = true,
                .big_endian = false,
                .bit_order_lsb = false
#endif
            },

            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = mic_sck,
                .ws = mic_ws,
                .dout = I2S_GPIO_UNUSED,
                .din = mic_din,
                .invert_flags = {
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv = false
                }
            }
        };

        ESP_ERROR_CHECK(
            i2s_channel_init_std_mode(
                rx_handle_,
                &rx_cfg
            )
        );

        ESP_LOGI(
            TAG,
            "INMP441 initialized SCK=%d WS=%d SD=%d",
            mic_sck,
            mic_ws,
            mic_din
        );
    }

    virtual ~MicOnlyCodec() {
        if (rx_handle_ != nullptr) {
            i2s_channel_disable(rx_handle_);
            i2s_del_channel(rx_handle_);
        }
    }

protected:
    int Read(int16_t* dest, int samples) override {
        std::vector<int32_t> raw(samples);
        size_t bytes_read = 0;

        esp_err_t ret =
            i2s_channel_read(
                rx_handle_,
                raw.data(),
                samples * sizeof(int32_t),
                &bytes_read,
                pdMS_TO_TICKS(200)
            );

        if (ret != ESP_OK) {
            return 0;
        }

        int count = bytes_read / sizeof(int32_t);
        for (int i = 0; i < count; i++) {
            int32_t value = raw[i] >> 16;

            if (value > INT16_MAX)
                value = INT16_MAX;
            if (value < INT16_MIN)
                value = INT16_MIN;

            dest[i] = static_cast<int16_t>(value);
        }

        return count;
    }

    int Write(const int16_t* data, int samples) override {
        // No speaker
        return samples;
    }

    void EnableInput(bool enable) override {
        if (enable == input_enabled_) {
            return;
        }

        if (enable) {
            ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
        } else {
            ESP_ERROR_CHECK(i2s_channel_disable(rx_handle_));
        }

        AudioCodec::EnableInput(enable);
    }

    void EnableOutput(bool enable) override {
        // No physical speaker
        AudioCodec::EnableOutput(enable);
    }
};

class MoleNetV63MicBoard : public WifiBoard {
private:
    Button boot_button_;

    void InitializeButton() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            app.ToggleChatState();
        });
    }

public:
    MoleNetV63MicBoard()
        : WifiBoard(),
          boot_button_(BOOT_BUTTON_GPIO) {

        InitializeButton();

        ESP_LOGI(TAG, "MoleNet V6.3 board initialized");
    }

    AudioCodec* GetAudioCodec() override {
        static MicOnlyCodec codec(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_MIC_GPIO_SCK,
            AUDIO_I2S_MIC_GPIO_WS,
            AUDIO_I2S_MIC_GPIO_DIN
        );

        return &codec;
    }

    Display* GetDisplay() override {
        static NoDisplay display;
        return &display;
    }

    Led* GetLed() override {
        static GpioLed led(GPIO_NUM_38);
        return &led;
    }
};

DECLARE_BOARD(MoleNetV63MicBoard);
