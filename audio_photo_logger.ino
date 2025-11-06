/*
  XIAO ESP32S3 Sense - Automatic Photo Capture and Sound-Triggered Audio Recording
  ---------------------------------------------------------
  - Takes a photo every 10 seconds
  - Listens continuously for sound via onboard microphone
  - Records a 10-second WAV file when loud sound is detected
  - Saves all files to SD card
*/

#include <Arduino.h>
#include <esp_camera.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <I2S.h>

#define CAMERA_MODEL_XIAO_ESP32S3
#include "camera_pins.h"

// ---------- SETTINGS ----------
#define RECORD_TIME 10               // seconds for each audio recording
#define SAMPLE_RATE 16000U
#define SAMPLE_BITS 16
#define WAV_HEADER_SIZE 44
#define VOLUME_GAIN 3
#define PHOTO_INTERVAL 10000         // 10 seconds between photos
#define SOUND_THRESHOLD 5000         // mic amplitude threshold for sound detection
// ------------------------------

bool camera_status = false;
bool sd_status = false;
int fileCount = 1;
unsigned long lastPhotoTime = 0;

// ---------- FUNCTION DECLARATIONS ----------
void photo_save(const char *fileName);
void writeFile(fs::FS &fs, const char *path, uint8_t *data, size_t len);
void record_wav(String audiofile);
void generate_wav_header(uint8_t *wav_header, uint32_t wav_size, uint32_t sample_rate);
void CameraParameters();

// ---------- CAMERA SETUP ----------
void CameraParameters() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (psramFound()) {
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

// ---------- PHOTO SAVE ----------
void photo_save(const char *fileName) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Failed to get camera frame buffer");
    return;
  }

  writeFile(SD, fileName, fb->buf, fb->len);
  esp_camera_fb_return(fb);
  Serial.printf("Photo saved: %s\n", fileName);
}

// ---------- FILE WRITE ----------
void writeFile(fs::FS &fs, const char *path, uint8_t *data, size_t len) {
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  file.write(data, len);
  file.close();
}

// ---------- AUDIO RECORD ----------
void record_wav(String audiofile) {
  uint32_t sample_size = 0;
  uint32_t record_size = (SAMPLE_RATE * SAMPLE_BITS / 8) * RECORD_TIME;

  File file = SD.open(audiofile, FILE_WRITE);
  uint8_t wav_header[WAV_HEADER_SIZE];
  generate_wav_header(wav_header, record_size, SAMPLE_RATE);
  file.write(wav_header, WAV_HEADER_SIZE);

  uint8_t *rec_buffer = (uint8_t *)ps_malloc(record_size);
  if (rec_buffer == NULL) {
    Serial.println("Malloc failed!");
    return;
  }

  Serial.println("Recording sound...");
  esp_i2s::i2s_read(esp_i2s::I2S_NUM_0, rec_buffer, record_size, &sample_size, portMAX_DELAY);

  // Apply gain
  for (uint32_t i = 0; i < sample_size; i += 2)
    (*(uint16_t *)(rec_buffer + i)) <<= VOLUME_GAIN;

  file.write(rec_buffer, sample_size);
  file.close();
  free(rec_buffer);
  Serial.println("Recording complete!");
}

// ---------- WAV HEADER ----------
void generate_wav_header(uint8_t *wav_header, uint32_t wav_size, uint32_t sample_rate) {
  uint32_t file_size = wav_size + WAV_HEADER_SIZE - 8;
  uint32_t byte_rate = SAMPLE_RATE * SAMPLE_BITS / 8;
  const uint8_t header[] = {
    'R', 'I', 'F', 'F',
    file_size, file_size >> 8, file_size >> 16, file_size >> 24,
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00,
    0x01, 0x00,
    0x01, 0x00,
    sample_rate, sample_rate >> 8, sample_rate >> 16, sample_rate >> 24,
    byte_rate, byte_rate >> 8, byte_rate >> 16, byte_rate >> 24,
    0x02, 0x00,
    0x10, 0x00,
    'd', 'a', 't', 'a',
    wav_size, wav_size >> 8, wav_size >> 16, wav_size >> 24
  };
  memcpy(wav_header, header, sizeof(header));
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Initialize microphone (I2S)
  I2S.setAllPins(-1, 42, 41, -1, -1);
  if (!I2S.begin(PDM_MONO_MODE, SAMPLE_RATE, SAMPLE_BITS)) {
    Serial.println("Failed to initialize I2S!");
    while (1);
  }

  // Initialize camera
  CameraParameters();
  camera_status = true;
  Serial.println("Camera OK!");

  // Initialize SD card
  if (!SD.begin(21)) {
    Serial.println("Failed to mount SD Card!");
    while (1);
  }

  if (SD.cardType() == CARD_NONE) {
    Serial.println("No SD card detected!");
    while (1);
  }
  sd_status = true;
  Serial.println("SD Card OK!");
}

// ---------- LOOP ----------
void loop() {
  if (!(camera_status && sd_status)) return;

  unsigned long now = millis();

  // Take a photo every 10 seconds
  if (now - lastPhotoTime >= PHOTO_INTERVAL) {
    char imgFile[32];
    sprintf(imgFile, "/image%d.jpg", fileCount);
    photo_save(imgFile);
    Serial.printf("Saved photo: %s\n", imgFile);
    lastPhotoTime = now;
    fileCount++;
  }

  // Check microphone for loud sound
  int16_t sample = 0;
  size_t bytes_read = 0;
  esp_i2s::i2s_read(esp_i2s::I2S_NUM_0, &sample, sizeof(sample), &bytes_read, 0);

  if (bytes_read > 0 && abs(sample) > SOUND_THRESHOLD) {
    Serial.println("Sound detected! Starting recording...");
    char audioFile[32];
    sprintf(audioFile, "/audio%d.wav", fileCount);
    record_wav(audioFile);
    fileCount++;
    delay(1000); // small delay to avoid rapid retriggers
  }
}
