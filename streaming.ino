#include <WiFi.h>
#include <HTTPClient.h>
#include "ESP_I2S.h"

#define SAMPLERATE 16000
#define BUFFER_SIZE 500

const char* ssid = "FLEMING";
const char* password = "aMi4mTsg";
const char* serverUrl = "http://omen:3000/upload";
const byte ledPin = BUILTIN_LED;

I2SClass i2s;
int16_t buffer[BUFFER_SIZE];
bool recording = false;
int debugCounter = 0;

void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");

  // Initialize I2S for PDM RX
  i2s.begin(I2S_MODE_PDM_RX, SAMPLERATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);

  Serial.println("Commands:");
  Serial.println("- Press button to start recording");
  Serial.println("- Send 'r' via serial to start recording");  
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    if (cmd == "r") {
      digitalWrite(ledPin, LOW); // LED off
      Serial.println("Start Recording");
      recording = true;
    } else if (cmd == "s") {
      digitalWrite(ledPin, HIGH); // LED off
      Serial.println("Stop Recording");
      recording = false;
    } else {
      Serial.printf("Unknown command: %s\n", cmd);
    }
  }

  if (recording) {
    int validSamples = 0;  
    for (int i = 0; i < BUFFER_SIZE; i++) {
      if (debugCounter++ % 100 == 0) {
        Serial.print("o");
      }

      if (i2s.available()) {
        int32_t sample = i2s.read();
        int16_t sample16 = (int16_t)(sample & 0xFFFF) * 2; // Convert to 16-bit
        buffer[i] = sample16;
        validSamples++;
      } else {
        break;
      }
    }

    if (validSamples > 0 && WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverUrl);
      http.addHeader("Content-Type", "application/octet-stream");
      int httpResponseCode = http.POST((uint8_t*)buffer, validSamples * sizeof(int16_t));
      http.end();
      // Optional: print response code for debugging
      Serial.printf("\nSamples: %d, Server response: %d\n", validSamples, httpResponseCode);
    }
  }
}