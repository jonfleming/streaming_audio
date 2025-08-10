#include <WiFi.h>
#include <WebSocketsClient.h>
#include "ESP_I2S.h"

#define SAMPLERATE 16000
#define BUFFER_SIZE 500

const char* ssid = "FLEMING";
const char* password = "aMi4mTsg";
const char* wsServer = "omen"; // WebSocket server hostname
const int wsPort = 3000;      // WebSocket server port
const char* wsPath = "/";   // WebSocket endpoint


I2SClass i2s;
WebSocketsClient webSocket;
int16_t buffer[BUFFER_SIZE];
bool recording = false;
int debugCounter = 0;

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket disconnected");
      recording = false; // Stop recording on disconnect
      break;
    case WStype_CONNECTED:
      Serial.println("WebSocket connected");
      break;
    case WStype_TEXT:
      // Handle text messages from server (e.g., stop command)
      if (strcmp((char*)payload, "stop") == 0) {
        Serial.println("Received stop command");
        recording = false;
      } else {
        Serial.printf("Received message: %s\n", payload);
      }
      break;
    case WStype_BIN:
      // Not expecting binary data from server
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Setup.");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");

  // Initialize WebSocket client
  webSocket.begin(wsServer, wsPort, wsPath);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000); // Try to reconnect every 5 seconds

  // Set I2S pins for PDM microphone
  i2s.setPinsPdmRx(42, 41); // CLK=42, DATA=41 - adjust for your board

  // Initialize I2S for PDM RX
  i2s.begin(I2S_MODE_PDM_RX, SAMPLERATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);

  Serial.println("Commands:");
  Serial.println("- Send 'r' via serial to start recording");
  Serial.println("- Send 's' via serial to stop recording");

  recording = true;
}


void checkSerial() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    if (cmd == "r") {
      Serial.println("\nRecord Command. Start Recording");
      recording = true;
    } else if (cmd == "s") {
      Serial.println("\nStop Command. Stop Recording");
      recording = false;
      webSocket.sendTXT("stop"); // Notify server to finalize recording
    } else {
      Serial.printf("\nUnknown command: %s\n", cmd.c_str());
    }
  }
}

bool getSample(int i) {
  if (i2s.available()) {
    int32_t sample = i2s.read();
    int16_t sample16 = (int16_t)(sample & 0xFFFF) * 2; // Convert to 16-bit
    if (debugCounter++ % 10000 == 0) {
      // Serial.printf("Sample: %d\t%d\n", sample, sample16);
      Serial.print("o");
    }
    buffer[i] = sample16;
    return true; // Data available
  } else {
    Serial.println("\n No data available");
    return false; // No data available
  }
}

void loop() {
  if (debugCounter++ % 100000 == 0) {
    Serial.print("-");
  }

  webSocket.loop(); // Handle WebSocket events
  checkSerial();

  if (recording && webSocket.isConnected()) {
    int validSamples = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
      if (getSample(i)) {
        validSamples++;
      } else {
        Serial.println("\n No data available");
        break;
      }
    }

    if (validSamples > 0) {
      // Send audio buffer over WebSocket as binary data
      webSocket.sendBIN((uint8_t*)buffer, validSamples * sizeof(int16_t));
      // Serial.printf("\nSent %d samples\n", validSamples);
    }
  }
}