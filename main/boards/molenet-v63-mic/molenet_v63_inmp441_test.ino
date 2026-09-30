#include <driver/i2s.h>

#define MIC_SCK 40
#define MIC_WS  41
#define MIC_SD  42
#define I2S_PORT I2S_NUM_0

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("MoleNet V6.3 + INMP441 test");

  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };

  i2s_pin_config_t pins = {
    .bck_io_num = MIC_SCK,
    .ws_io_num = MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = MIC_SD
  };

  esp_err_t r1 = i2s_driver_install(
    I2S_PORT,
    &config,
    0,
    NULL
  );

  esp_err_t r2 = i2s_set_pin(
    I2S_PORT,
    &pins
  );

  Serial.print("Driver install: ");
  Serial.println(r1);

  Serial.print("Pin setup: ");
  Serial.println(r2);

  Serial.println("Speak or clap near microphone");
}

void loop() {
  int32_t samples[64];
  size_t bytesRead = 0;

  esp_err_t result = i2s_read(
    I2S_PORT,
    samples,
    sizeof(samples),
    &bytesRead,
    portMAX_DELAY
  );

  if (result != ESP_OK) {
    Serial.println("I2S read ERROR");
    delay(500);
    return;
  }

  int count = bytesRead / sizeof(int32_t);
  int32_t peak = 0;

  for (int i = 0; i < count; i++) {
    int32_t sample = samples[i] >> 11;
    int32_t level = abs(sample);

    if (level > peak)
      peak = level;
  }

  Serial.print("Mic level: ");
  Serial.println(peak);

  delay(100);
}