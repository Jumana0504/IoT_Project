#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ArduinoJson.h>
#include <addons/RTDBHelper.h>
#include <ir_Electra.h>  // For IRElectraAc protocol
#include <Preferences.h>
#include <WebServer.h>
#include "secrets.h"
#include "parameters.h"



// ----------------------- Firebase Objects -----------------------
FirebaseAuth auth;
FirebaseConfig config;
FirebaseData fbdo;       // for streaming
FirebaseData writeFbdo;  // for writing
Preferences prefs;
WebServer server(80);

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;
bool shouldClearCommand = false;
String deviceMacPath;

// Function prototypes
void onDataChange(FirebaseStream data);
void onStreamTimeout(bool timeout);

// ----------------------- IR Config -----------------------
// IR send + Electra AC protocol instance
IRsend irsend(kIrLedPin);
IRElectraAc ac(kIrLedPin);

// Track current temperature for up/down
int currentTemp = DEFAULT_TEMP;  // Starting temp

bool isProvisioned() {
  prefs.begin("setup", true); // read-only
  bool ready = prefs.getBool("provisioned", false);
  prefs.end();
  return ready;
}

void startAPMode() {
  String apName = "SmartAC-Setup-" + WiFi.macAddress();
  apName.replace(":", "");

  WiFi.softAP(apName.c_str(), "12345678");
  Serial.println("📡 AP Mode Started: " + apName);
  Serial.println("IP address: " + WiFi.softAPIP().toString());

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/plain", "Smart AC Setup: send POST to /setup with ssid, password, and userId");
  });

  server.on("/setup", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
      server.send(400, "text/plain", "Missing body");
      return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
      server.send(400, "text/plain", "Invalid JSON");
      return;
    }

    String ssid = doc["ssid"];
    String pass = doc["password"];
    String userId = doc["userId"];

    if (ssid == "" || pass == "" || userId == "") {
      server.send(400, "text/plain", "Missing ssid/password/userId");
      return;
    }

    prefs.begin("setup", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.putString("userId", userId);
    prefs.putBool("provisioned", true);
    prefs.end();

    server.send(200, "text/plain", "✅ Credentials received, rebooting...");
    delay(1000);
    ESP.restart();
  });

  server.begin();
}

// ----------------------- Setup -----------------------
void setup() {
  Serial.begin(115200);

  if (!isProvisioned()) {
    startAPMode();
    return;
  }
  // Already provisioned — connect using saved creds
  prefs.begin("setup", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  String adminUID = prefs.getString("userId", "");
  prefs.end();

  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nConnected!");

  // Configure Firebase
  config.api_key = ApiKey;
  config.database_url = DbUrl;

  auth.user.email = AuthEmail;
  auth.user.password = AuthPass;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  // Wait for authentication
  while (auth.token.uid == "") {
    Serial.print(".");
    delay(200);
  }
  Serial.println("\nFirebase Authenticated!");

  // Use MAC address as device ID
  String mac = WiFi.macAddress();               // e.g., "84:F3:EB:12:34:56"
  mac.replace(":", "");                         // remove colons if desired
  deviceMacPath = "/devices/" + mac + "/command";

  // Set admin who provisioned this ESP
  String adminPath = "/devices/" + mac + "/authorizedUsers/" + adminUID;
  if (Firebase.RTDB.setString(&writeFbdo, adminPath.c_str(), "admin")) {
    Serial.println("✅ First-time admin registered");
  } else {
    Serial.println("❌ Failed to set admin: " + writeFbdo.errorReason());
  }
  delay(1000);
  // Report device is online
  String statusPath = "/devices/" + mac + "/status";
  if (!Firebase.RTDB.setString(&writeFbdo, statusPath.c_str(), "online")) {
    Serial.println("❌ Failed to update device status: " + writeFbdo.errorReason());
  } else {
    Serial.println("✅ Device status set to ONLINE");
  }

  Serial.println("Listening to path: " + deviceMacPath);

  // Start IR
  irsend.begin();
  ac.begin();

  // Start stream
  if (!Firebase.RTDB.beginStream(&fbdo, deviceMacPath.c_str())) {
    Serial.println("Stream begin failed: " + fbdo.errorReason());
  }

  Firebase.RTDB.setStreamCallback(&fbdo, onDataChange, onStreamTimeout);
}

// ----------------------- Stream Callback -----------------------
void onDataChange(FirebaseStream data) {
  Serial.println("Received raw: " + data.to<String>());
  Serial.println("Path: " + data.dataPath());
  Serial.println("Type: " + data.dataType());

  if (data.dataType() == "json") {
    FirebaseJson &json = data.to<FirebaseJson>();
    FirebaseJsonData result;

    // Extract "action"
    if (json.get(result, "action")) {
      String action = result.to<String>();
      Serial.println("🔹 Action: " + action);
      
      // Extract "user"
      if (json.get(result, "user")) {
        String user = result.to<String>();
        Serial.println("👤 User: " + user);
      } else {
        Serial.println("⚠️ User not found in JSON");
      }

      // Extract "timestamp"
      if (json.get(result, "timestamp")) {
        long timestamp = result.to<int64_t>();  // Use int64_t to support long millis
        Serial.print("🕒 Timestamp: ");
        Serial.println(timestamp);
      } else {
        Serial.println("⚠️ Timestamp not found in JSON");
      }

      // Proceed with the action
      // Validate user role before sending command
      if (json.get(result, "user")) {
        String user = result.to<String>();
        Serial.println("👤 User: " + user);

        String role = getUserRole(user);
        if (role == "") {
          Serial.println("🚫 Unknown or unauthorized user");
          return;
        }

        Serial.println("✅ User role: " + role);

        if (isCommandAllowed(role, action)) {
          sendElectraCommand(action);
        } else {
          Serial.println("🚫 Command rejected: " + action + " not allowed for role: " + role);
        }
      } else {
        Serial.println("⚠️ User not found in JSON");
      }
    } else {
      Serial.println("❌ 'action' not found in JSON");
    }
  } else {
    Serial.println("Invalid or missing command");
  }
}

void onStreamTimeout(bool timeout) {
  if (timeout) {
    Serial.println("[Firebase] Stream timed out, reconnecting...");
    if (!fbdo.httpConnected())
      Serial.println("[Firebase] HTTP not connected");
  }
}

void sendElectraCommand(String cmd) {
  Serial.println("[Electra] Handling command: " + cmd);

  ac.setMode(kElectraAcCool);
  ac.setFan(kElectraAcFanAuto);
  ac.setPower(true);

  if (cmd == "power_off") {
    ac.setPower(false);
    Serial.println("[Electra] Sending POWER OFF");
  } else if (cmd == "power_on") {
    ac.setPower(true);
    Serial.println("[Electra] Sending POWER ON");
  } else if (cmd == "temp_up") {
    currentTemp = min(currentTemp + 1, MAX_TEMP);
    Serial.printf("[Electra] Increasing temp to %d\n", currentTemp);
  } else if (cmd == "temp_down") {
    currentTemp = max(currentTemp - 1, MIN_TEMP);
    Serial.printf("[Electra] Decreasing temp to %d\n", currentTemp);
  } else {
    Serial.println("[Electra] Unknown command");
    return;
  }

  ac.setTemp(currentTemp);
  ac.send();
  Serial.println("[Electra] IR command sent.");
  delay(1000);
  shouldClearCommand = true;
}

String getUserRole(const String& username) {
  String authPath = deviceMacPath;
  authPath.replace("/command", "/authorizedUsers/" + username);

  if (Firebase.RTDB.getString(&writeFbdo, authPath.c_str())) {
    return writeFbdo.stringData();  // e.g. "admin" or "user"
  }

  return "";  // not found or failed
}

bool isCommandAllowed(const String& role, const String& action) {
  if (role == "admin") return true;

  // Role is "user"
  if (action == "power_on" || action == "temp_up" || action == "temp_down")
    return true;

  return false;  // All else blocked for normal users
}

void loop() {
  if (WiFi.getMode() == WIFI_AP) {
    server.handleClient();
    return;
  }
  // Reconnect Wi-Fi if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ Wi-Fi disconnected, trying to reconnect...");
    WiFi.reconnect();
    delay(1000);
    return; // Don't continue loop while offline
  }

  // Reconnect Firebase stream if lost
  if (!fbdo.httpConnected()) {
    Serial.println("⚠️ Firebase stream lost. Reinitializing...");
    
    if (!Firebase.RTDB.beginStream(&fbdo, deviceMacPath.c_str())) {
      Serial.println("❌ Failed to restart stream: " + fbdo.errorReason());
    } else {
      Serial.println("✅ Firebase stream restarted");
      Firebase.RTDB.setStreamCallback(&fbdo, onDataChange, onStreamTimeout);
    }

    delay(1000);
    return;
  }

  // Clear command if needed
  if (shouldClearCommand) {
    shouldClearCommand = false;

    Serial.println("Clearing command from loop: " + deviceMacPath);
    delay(1000);
    if (!Firebase.RTDB.setString(&writeFbdo, deviceMacPath.c_str(), "waiting")) {
      Serial.println("❌ Failed to clear command: " + writeFbdo.errorReason());

      // Recheck HTTP connection status
      if (!writeFbdo.httpConnected()) {
        Serial.println("⚠️ HTTP not connected. Attempting recovery...");

        // Reinitialize Firebase stream in case of dropped connection
        if (!Firebase.RTDB.beginStream(&fbdo, deviceMacPath.c_str())) {
          Serial.println("❌ Failed to restart stream: " + fbdo.errorReason());
        } else {
          Serial.println("✅ Firebase stream reinitialized.");
          Firebase.RTDB.setStreamCallback(&fbdo, onDataChange, onStreamTimeout);
        }
      }
    } else {
      Serial.println("✅ Command cleared.");
    }
  }

  delay(10);
}