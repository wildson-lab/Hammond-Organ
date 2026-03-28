#include <Arduino.h>
#include <driver/i2s.h>
#include "tonewheel_phase_inc_table.h"
#include "wavetable_sin_12bits.h"

#define NOTE_OFFSET 12  //Offset entre a  primeira tecla do teclado (C2) e a primeira roda de tom (C1)

uint32_t phases[91] = {0};  // fase inicial de cada roda de tom
byte keyboard[61] = {0};  // estado de cada tecla (0 ou 1)
uint8_t drawbars[9] = {8,0,8,0,0,0,0,0,0};  // posição de cada drawbar (0 a 8)
int drawbar_map[9] = {-12, 7, 0, 12, 19, 24, 28, 31, 36};  // mapeamento de cada drawbar para as rodas de tom correspondentes

// GPIOs
static constexpr int PIN_I2S_BCLK = 4;  // Pino BCK no módulo PCM5102, também conhecido como SCK ou CLK
static constexpr int PIN_I2S_WS   = 5;  // Pino LCK no módulo PCM5102, também conhecido como FS ou LRCK
static constexpr int PIN_I2S_DOUT = 6;  // Pino DIN, no módulo PCM5102, também conhecido como SDOUT ou SD

// áudio
static constexpr uint32_t SAMPLE_RATE = 48000;    // taxa de amostragem em Hz. O PCM5102 suporta até 384kHz, mas 48kHz é uma escolha comum para áudio de alta qualidade e é mais fácil de processar em tempo real.
static constexpr size_t FRAMES_PER_BUFFER = 256;  // número de frames (amostras por canal) por buffer de áudio. O tamanho do buffer em bytes será FRAMES_PER_BUFFER * 2 (estéreo) * 2 (16 bits por amostra)

int16_t audioBuffer[FRAMES_PER_BUFFER * 2];  // estéreo intercalado

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Iniciando I2S...");

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = PIN_I2S_BCLK,
    .ws_io_num = PIN_I2S_WS,
    .data_out_num = PIN_I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  esp_err_t err;

  err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Erro em i2s_driver_install: %d\n", err);
    while (true) delay(1000);
  }

  err = i2s_set_pin(I2S_NUM_0, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Erro em i2s_set_pin: %d\n", err);
    while (true) delay(1000);
  }

  err = i2s_set_clk(I2S_NUM_0, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
  if (err != ESP_OK) {
    Serial.printf("Erro em i2s_set_clk: %d\n", err);
    while (true) delay(1000);
  }

  Serial.println("I2S iniciado");

  keyboard[33] = 1;  // Pressiona a tecla A4 (lá de 440Hz)
}


void loop() {
  byte keyboard_cache[61];
  memcpy(keyboard_cache, keyboard, sizeof(keyboard_cache));

  for (size_t n = 0; n < FRAMES_PER_BUFFER; n++) {

    for (int i = 0; i < 91; i++) {
      phases[i] += tonewheel_phase_inc[i];
    }

    int64_t sample = 0;

    for (int key = 0; key < 61; key++) {

      if (keyboard_cache[key]) {
        for(int db = 0; db < 9; db++) {
          int tw_index = key + NOTE_OFFSET + drawbar_map[db];

          if (tw_index >= 0 && tw_index < 91) {            
            int16_t wave = wavetable_sin_4096[(phases[tw_index] >> 20) & 0x0FFF];
            sample += (int32_t)wave * drawbars[db];
          }
        }
      }
    }

    sample >>= 5;

    // Saturação para int16_t
    if (sample > 32767) sample = 32767;
    else if (sample < -32768) sample = -32768;

    int16_t out = (int16_t)sample;

    audioBuffer[2 * n + 0] = out; // L
    audioBuffer[2 * n + 1] = out; // R
  }

  size_t bytes_written = 0;
  i2s_write(I2S_NUM_0, audioBuffer, sizeof(audioBuffer), &bytes_written, portMAX_DELAY);

}
