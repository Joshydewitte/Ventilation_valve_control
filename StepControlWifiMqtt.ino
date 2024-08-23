#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <Preferences.h>

// Define stepper motor connections and steps per revolution:
#define dirPin 32
#define stepPin 25
#define enablePin 14
#define resetPin 27

// Define AP credentials and network credentials:
const char *AP_SSID = "ESP32_Config";
const char *AP_PASSWORD = "config1234";

// Initialize WebServer
WebServer server(80);

// Initialize MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

// Initialize Preferences
Preferences preferences;

// Timing variables
unsigned long lastReconnectAttempt = 0;
const long reconnectInterval = 5000; // 5 seconds

// Motor position
int currentSteps = 0; // Current position in steps
int default_total_steps = 200; // Default total steps for the motor

// MQTT settings
String default_mqtt_server;
String mqtt_server;
int default_mqtt_port;
int mqtt_port;
String default_mqtt_user;
String mqtt_user;
String default_mqtt_password;
String mqtt_password;
String default_mqtt_topic = "hello/topic"; // Default topic value
String mqtt_topic;

void handleRoot() {
  String html = "<html><body><h1>Configuration Setup</h1>";
  html += "<form action=\"/setconfig\" method=\"POST\">";
  html += "<h2>WiFi Setup</h2>";
  html += "SSID: <input type=\"text\" name=\"ssid\" value=\"" + preferences.getString("ssid", "") + "\"><br>";
  html += "Password: <input type=\"password\" name=\"password\" value=\"" + preferences.getString("password", "") + "\"><br>";
  html += "<h2>MQTT Setup</h2>";
  html += "Server: <input type=\"text\" name=\"server\" value=\"" + mqtt_server + "\"><br>";
  html += "Port: <input type=\"number\" name=\"port\" value=\"" + String(mqtt_port) + "\"><br>";
  html += "User: <input type=\"text\" name=\"user\" value=\"" + mqtt_user + "\"><br>";
  html += "Password: <input type=\"password\" name=\"pass\" value=\"" + mqtt_password + "\"><br>";
  html += "Topic: <input type=\"text\" name=\"topic\" value=\"" + mqtt_topic + "\"><br>"; // Add topic input
  html += "Total Steps: <input type=\"number\" name=\"steps\" value=\"" + String(default_total_steps) + "\"><br>";
  html += "<input type=\"submit\" value=\"Save\">";
  html += "</form></body></html>";
  server.send(200, "text/html", html);
}

void handleSetConfig() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  mqtt_server = server.arg("server");
  mqtt_port = server.arg("port").toInt();
  mqtt_user = server.arg("user");
  mqtt_password = server.arg("pass");
  mqtt_topic = server.arg("topic"); // Retrieve the topic from the form
  default_total_steps = server.arg("steps").toInt();

  // Save the WiFi credentials
  if (!preferences.begin("wifi", false)) {
    server.send(500, "text/html", "Failed to save configuration");
    return;
  }
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();

  // Save the MQTT configuration
  if (!preferences.begin("mqtt", false)) {
    server.send(500, "text/html", "Failed to save configuration");
    return;
  }
  preferences.putString("mqtt_server", mqtt_server);
  preferences.putInt("mqtt_port", mqtt_port);
  preferences.putString("mqtt_user", mqtt_user);
  preferences.putString("mqtt_password", mqtt_password);
  preferences.putString("mqtt_topic", mqtt_topic); // Save the topic to preferences
  preferences.putInt("total_steps", default_total_steps);
  preferences.end();

  // Send response
  server.send(200, "text/html", "Configuration saved. Rebooting...");
  delay(1000);
  ESP.restart();
}

void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  server.on("/", handleRoot);
  server.on("/setconfig", HTTP_POST, handleSetConfig);
  server.begin();
}

void zeroMotor() {
  Serial.println("Zeroing motor...");
  
  // Simulate zeroing the motor by moving to 0 degrees
  String zeroMessage = "0"; // Simulate 0% which corresponds to 0 degrees
  callback("Zeroing Stepper open", (byte *)zeroMessage.c_str(), zeroMessage.length());
  
  delay(1000); // Wait for the motor to reach 0 position
  
  // Simulate zeroing the motor by moving to 360 degrees
  zeroMessage = "100"; // Simulate 100% which corresponds to 360 degrees
  callback("Zeroing stepper close", (byte *)zeroMessage.c_str(), zeroMessage.length());
}

void setup() {

  // Initialize Preferences and load WiFi credentials
  preferences.begin("wifi", true);
  String ssid = preferences.getString("ssid", "");
  String password = preferences.getString("password", "");

  // Load MQTT settings from preferences
  preferences.end();  // End the WiFi preferences namespace
  preferences.begin("mqtt", true);  // Start the MQTT preferences namespace
  mqtt_server = preferences.getString("mqtt_server", default_mqtt_server);
  mqtt_port = preferences.getInt("mqtt_port", default_mqtt_port);
  mqtt_user = preferences.getString("mqtt_user", default_mqtt_user);
  mqtt_password = preferences.getString("mqtt_password", default_mqtt_password);
  mqtt_topic = preferences.getString("mqtt_topic", default_mqtt_topic); // Load the topic from preferences
  default_total_steps = preferences.getInt("total_steps", default_total_steps);
  preferences.end();  // End the MQTT preferences namespace

  // WiFi verbinding
  if (ssid != "" && password != "") {
    WiFi.begin(ssid.c_str(), password.c_str());
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      

      setup_mqtt();  // Setup MQTT after WiFi is connected
    } else {
      startAccessPoint();
    }
  } else {
    startAccessPoint();
  }

  // Setup pins for stepper motor
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(enablePin, OUTPUT);

  // Setup reset pin
  pinMode(resetPin, INPUT_PULLUP); // Set pin 27 as input with pull-up resistor

  zeroMotor();
}

void reconnect() {
  if (client.connect("ESP32Client", mqtt_user.c_str(), mqtt_password.c_str())) {
    
    client.subscribe(mqtt_topic.c_str()); // Subscribe to the configured topic
    lastReconnectAttempt = 0; // Reset the reconnect attempt timer
  } 
}

void setup_mqtt() {
  client.setServer(mqtt_server.c_str(), mqtt_port);
  client.setCallback(callback);
  lastReconnectAttempt = millis(); // Start the initial reconnect attempt
}

void moveToPosition(int steps) {
  int stepsToMove = steps - currentSteps; // Calculate steps to move from current position
  int stepDelay = 3000; // Adjust delay as needed
  
  digitalWrite(enablePin, HIGH); // Enable motor
  

  if (stepsToMove > 0) {
    digitalWrite(dirPin, HIGH); // Set direction to forward
  } else {
    digitalWrite(dirPin, LOW); // Set direction to reverse
    stepsToMove = -stepsToMove; // Make stepsToMove positive for the loop
  }

  delay(50);
  
  for (int i = 0; i < stepsToMove; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(stepDelay);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(stepDelay);
  }
  
  digitalWrite(enablePin, LOW); // Disable motor
  currentSteps = steps; // Update current position

  delay(50);
}

void callback(char *topic, byte *message, unsigned int length) {
  
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)message[i];
  }
  
  // Extract percentage value
  int percentage = msg.toInt();
  if (percentage >= 0 && percentage <= 100) {
    // Calculate target position based on percentage
    int targetDegrees = map(percentage, 0, 100, 0, 360); // Map percentage to degrees
    int targetSteps = map(targetDegrees, 0, 360, 0, default_total_steps); // Map degrees to steps
    moveToPosition(targetSteps);
  } 
}

void loop() {
  if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > reconnectInterval) {
      lastReconnectAttempt = now;
      reconnect();
    }
  }
  client.loop(); // Process incoming messages
  server.handleClient(); // Handle client requests

  // Check reset pin status
  for (int i = 10; i > 0 && digitalRead(resetPin) == LOW; i--) {
    delay(1000);
    if (digitalRead(resetPin) == HIGH) {
      return;  // Break the loop if the reset pin is released
    }
    if (i <= 1 && digitalRead(resetPin) == LOW) {
      preferences.begin("wifi", false);
      preferences.clear(); // Clear all stored WiFi preferences
      preferences.end();

      preferences.begin("mqtt", false);
      preferences.clear(); // Clear all stored MQTT preferences
      preferences.end();

      delay(1000);
      ESP.restart(); // Reboot to apply changes
    }
  }
}
