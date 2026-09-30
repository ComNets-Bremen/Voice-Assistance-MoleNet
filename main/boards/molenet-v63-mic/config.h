#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// Audio sample rates
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// INMP441 Microphone Connection (J9 Header)
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_40
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_41
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_42

// Boot Button (SW2)
#define BOOT_BUTTON_GPIO GPIO_NUM_0

#endif // _BOARD_CONFIG_H_