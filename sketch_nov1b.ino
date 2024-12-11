#include <WiFi.h>
#include <HTTPClient.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

// WiFi credentials and Firebase URL
const char* ssid = "";
const char* password = "";
const char* firebaseURL = "";

// Pin definitions
const int solenoidPin = 5;
const int vibrationPin = 14;  // SW-420 vibration sensor digital output pin
const int batteryPin = 34;    // Analog pin to read battery voltage

// Device ID
const String deviceID = "127"; // Set device ID here

// Battery monitoring constants
const float VOLTAGE_DIVIDER_RATIO = 2.0;
const float ADC_REF_VOLTAGE = 3.3;
const int ADC_MAX = 4095;
const float MIN_BATTERY_VOLTAGE = 3.0;
const float MAX_BATTERY_VOLTAGE = 4.2;

// GPS and timing variables
TinyGPSPlus gps;
HardwareSerial SerialGPS(1);
String lastTimestamp = "";
unsigned long lastVibrationCheck = 0;
unsigned long lastHeartbeatSent = 0;
unsigned long lastOpenStatusCheck = 0;
bool isVibrationDetected = false;
String lockStatus = "Locked";  // Initialize lock status
unsigned long heartbeatCounter = 0;

// Added vibration counter
unsigned long vibrationCounter = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(solenoidPin, OUTPUT);
  pinMode(vibrationPin, INPUT);
  digitalWrite(solenoidPin, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  SerialGPS.begin(9600, SERIAL_8N1, 12, 13);
  captureInitialTimestamp();
  getInitialVibrationCount(); // Get the existing vibration count from Firebase
}

void loop() {
  if (millis() - lastHeartbeatSent >= 60000) {  // Heartbeat interval
    lastHeartbeatSent = millis();
    sendHeartbeat();
    if (gps.location.isValid()) {
            publishLocationToFirebase();
          }
  }

  // Check open status every 5 seconds
  if (millis() - lastOpenStatusCheck >= 5000) {
    lastOpenStatusCheck = millis();
    checkOpenStatus();
  }

  checkPaymentData();

  if (millis() - lastVibrationCheck >= 1000) {  // Vibration check interval
    lastVibrationCheck = millis();
    checkVibrationAndUpdate();
  }

  while (SerialGPS.available() > 0) {
    gps.encode(SerialGPS.read());
  }

  delay(100);
}

void checkOpenStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(String(firebaseURL) + "/devices/" + deviceID + "/open.json");
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String payload = http.getString();
      int valueStart = payload.indexOf("\"value\":") + 8;
      int valueEnd = payload.indexOf("}", valueStart);

      if (valueStart > 7 && valueEnd > valueStart) {
        String openValueStr = payload.substring(valueStart, valueEnd);
        int openValue = openValueStr.toInt();

        if (openValue == 1) {
          // Activate solenoid
          digitalWrite(solenoidPin, HIGH);
          lockStatus = "Open";

          
        } else {
          // Deactivate solenoid
          digitalWrite(solenoidPin, LOW);
          lockStatus = "Locked";
        }
      } else {
        // Handle case where "open" value is deleted or missing
        digitalWrite(solenoidPin, LOW);
        lockStatus = "Locked";
        Serial.println("Open value missing, locking solenoid.");
      }
    } else {
      // Error handling for HTTP request failure
      Serial.print("Error in HTTP GET for open status: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}


void sendHeartbeat() {
  float batteryPercentage = getBatteryPercentage();
  heartbeatCounter++;

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(String(firebaseURL) + "/devices/" + deviceID + "/heartbeat.json");
    http.addHeader("Content-Type", "application/json");
    String payload = "{\"batteryPercentage\": " + String(batteryPercentage) + 
                     ", \"status\": \"" + lockStatus + "\"" +
                     ", \"heartbeatCount\": " + String(heartbeatCounter) + "}";
    int httpResponseCode = http.PUT(payload);

    if (httpResponseCode > 0) {
      Serial.println("Heartbeat #" + String(heartbeatCounter) + " sent to Firebase with lock status: " + lockStatus);
    } else {
      Serial.print("Error sending heartbeat: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}

// New function to get initial vibration count from Firebase
void getInitialVibrationCount() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(String(firebaseURL) + "/devices/" + deviceID + "/vibration.json");
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String payload = http.getString();
      int valueStart = payload.indexOf("\"value\":") + 8;
      int valueEnd = payload.indexOf("}", valueStart);
      if (valueStart > 7 && valueEnd > valueStart) {
        String countStr = payload.substring(valueStart, valueEnd);
        vibrationCounter = countStr.toInt();
        Serial.println("Initial vibration count set to: " + String(vibrationCounter));
      }
    }
    http.end();
  }
}

float getBatteryPercentage() {
  int adcValue = analogRead(batteryPin);
  float measuredVoltage = (adcValue / float(ADC_MAX)) * ADC_REF_VOLTAGE * VOLTAGE_DIVIDER_RATIO;
  float batteryPercentage = ((measuredVoltage - MIN_BATTERY_VOLTAGE) / 
                             (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE)) * 100.0;

  return constrain(batteryPercentage, 0, 100);
}

void checkVibrationAndUpdate() {
  // Only check vibration if the device is not in open status
  if (lockStatus != "Open") {
    int vibrationState = digitalRead(vibrationPin);

    if (vibrationState == HIGH && !isVibrationDetected) {
      isVibrationDetected = true;
      Serial.println("Vibration detected!");
      sendVibrationAlert();
      if (gps.location.isValid()) {
        publishLocationToFirebase();
      }
    } else {
      isVibrationDetected = false;
    }
  }
}

void sendVibrationAlert() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(String(firebaseURL) + "/devices/" + deviceID + "/vibration.json");
    http.addHeader("Content-Type", "application/json");

    vibrationCounter++; // Increment the vibration counter
    String payload = "{\"value\": " + String(vibrationCounter) + "}";
    int httpResponseCode = http.PUT(payload);

    if (httpResponseCode > 0) {
      Serial.println("Vibration alert #" + String(vibrationCounter) + " sent to Firebase");
    } else {
      Serial.print("Error sending vibration alert: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}

void checkPaymentData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(String(firebaseURL) + "/devices/" + deviceID + "/payment.json");

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String payload = http.getString();
      int timestampStart = payload.indexOf("\"timestamp\":") + 12;
      int timestampEnd = payload.indexOf(",", timestampStart);
      String timestamp = payload.substring(timestampStart, timestampEnd).trim(); // Ensure no spaces

      int valueStart = payload.indexOf("\"value\":") + 8;
      int valueEnd = payload.indexOf("}", valueStart);
      String amount = payload.substring(valueStart, valueEnd);

      if (!timestamp.isEmpty() && timestamp != lastTimestamp) {
        // Update lastTimestamp immediately
        lastTimestamp = timestamp;

        Serial.print("Payment Amount Received: Rs. ");
        Serial.println(amount);

        lockStatus = "Unlocked";  // Update lock status
        digitalWrite(solenoidPin, HIGH);

        // Keep solenoid active for 20 seconds
        delay(20000);

        digitalWrite(solenoidPin, LOW);
        lockStatus = "Payment received and locked";  // Reset lock status
      }
    } else {
      Serial.print("Error in HTTP GET for payment data: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}


void publishLocationToFirebase() {
  if (WiFi.status() == WL_CONNECTED && gps.location.isValid()) {
    String locationData = String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);

    HTTPClient http;
    http.begin(String(firebaseURL) + "/devices/" + deviceID + "/location.json");
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"value\":\"" + locationData + "\"}";
    int httpResponseCode = http.PUT(payload);

    if (httpResponseCode > 0) {
      Serial.println("Location data sent: " + locationData);
    }
    http.end();
  }
}

void captureInitialTimestamp() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(String(firebaseURL) + "/devices/" + deviceID + "/payment.json");
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String payload = http.getString();
      int timestampStart = payload.indexOf("\"timestamp\":") + 12;
      int timestampEnd = payload.indexOf(",", timestampStart);
      lastTimestamp = payload.substring(timestampStart, timestampEnd);
      Serial.println("Initial Timestamp set to: " + lastTimestamp);
    }
    http.end();
  }
}
