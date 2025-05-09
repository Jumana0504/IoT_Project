#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "Name";           // 🔁 Replace with your Wi-Fi network name
const char* password = "Password";   // 🔁 Replace with your Wi-Fi password
const char* apiKey = "Key";     // 🔁 Replace with your OpenWeatherMap API key

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi connected!");
  Serial.print("📡 IP address: ");
  Serial.println(WiFi.localIP());

  // 🛰️ Build API request for Haifa
  String url = "http://api.openweathermap.org/data/2.5/weather?q=Haifa&units=metric&appid=" + String(apiKey);

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("🔁 Raw response:");
    Serial.println(payload);

    // 📦 Parse JSON
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      float temp = doc["main"]["temp"];
      int humidity = doc["main"]["humidity"];
      const char* description = doc["weather"][0]["description"];

      Serial.println("🌤 Weather in Haifa:");
      Serial.print("Temperature: ");
      Serial.print(temp);
      Serial.println(" °C");

      Serial.print("Humidity: ");
      Serial.print(humidity);
      Serial.println(" %");

      Serial.print("Condition: ");
      Serial.println(description);
    } else {
      Serial.print("❌ JSON parsing failed: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.print("❌ HTTP request failed. Code: ");
    Serial.println(httpCode);
  }

  http.end();
}

void loop() {
  // Nothing here
}
