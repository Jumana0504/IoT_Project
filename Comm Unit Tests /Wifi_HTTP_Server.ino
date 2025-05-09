#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

const char* ssid = "Name";        // Change this
const char* password = "Password"; // Change this

const uint16_t kIrLed = 4;  // GPIO pin for IR LED (change if needed)
IRsend irsend(kIrLed);

AsyncWebServer server(80);

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("🔌 Connecting to WiFi");

  int maxRetries = 20;
  while (WiFi.status() != WL_CONNECTED && maxRetries-- > 0) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected to WiFi!");
    Serial.print("📡 ESP IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Failed to connect to WiFi.");
  }
}

void setup() {
  Serial.begin(115200);
  connectToWiFi();
  irsend.begin();

  // Serve a simple HTML page with a button
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", R"rawliteral(
      <h2>IR Control</h2>
      <button onclick="fetch('/send')">Send IR Signal</button>
    )rawliteral");
  });

  // Send the IR signal when user clicks the button
  server.on("/send", HTTP_GET, [](AsyncWebServerRequest *request){
    irsend.sendNEC(0x20DF10EF, 32);  // Replace with your actual IR code
    Serial.println("📤 IR signal sent!");
    request->send(200, "text/plain", "IR signal sent.");
  });

  server.begin();
}

void loop() {}
