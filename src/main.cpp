#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <Wire.h>

// ----- Pins -----
#define LED_RED_PIN    32  // Power (steady on)
#define LED_GREEN_PIN  33  // Idle/ready (on when idle)
#define LED_WHITE_PIN  14  // Busy/transmitting (blink while beeping)
#define BUZZER_PIN     27  // Buzzer pin

// ----- LCD -----
#define LCD_ADDR 0x27
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

// I2C pins
#define I2C_SDA 26
#define I2C_SCL 25

// ----- Wi-Fi -----
const char* WIFI_SSID     = "HONOR_X7c";
const char* WIFI_PASSWORD = "uuuuuuuub";

// ----- MQTT (HiveMQ Cloud) -----
// NOTE: HiveMQ Cloud requires TLS on 8883, but we will NOT verify the cert.
const char* MQTT_HOST     = "e70fab49237b417185f60ee78a9ba55a.s1.eu.hivemq.cloud";
const uint16_t MQTT_PORT  = 8883;
const char* MQTT_USERNAME = "Panic2";
const char* MQTT_PASSWORD_MQTT = "123@Firstlove";

// Subscribe topic
const char* MQTT_TOPIC_SUB = "Alarm";

String localMac;

// Clients
WiFiClientSecure secureClient;
PubSubClient mqtt(secureClient);

// Display state
float lastTemperature = NAN;
int lastMotion = -1;

// Timing
const uint32_t BEEP_MS        = 500;
const uint32_t WHITE_BLINK_MS = 120;

// Buzzer wiring: set true if your buzzer is active-low (buzzes when pin is LOW).
const bool BUZZER_ACTIVE_LOW = true;

// -------------------- DEBUG HELPERS --------------------

const char* wifiStatusToStr(wl_status_t s) {
  switch (s) {
    case WL_IDLE_STATUS:    return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL:  return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
    case WL_CONNECTED:      return "WL_CONNECTED";
    case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST:return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED:   return "WL_DISCONNECTED";
    default:                return "UNKNOWN";
  }
}

void printMqttState() {
  int8_t s = mqtt.state();
  Serial.print("MQTT state code: ");
  Serial.println(s);
  switch (s) {
    case -4: Serial.println("MQTT_STATE: -4 = CONNECTION_TIMEOUT"); break;
    case -3: Serial.println("MQTT_STATE: -3 = CONNECTION_LOST"); break;
    case -2: Serial.println("MQTT_STATE: -2 = CONNECT_FAILED"); break;
    case -1: Serial.println("MQTT_STATE: -1 = DISCONNECTED"); break;
    case  0: Serial.println("MQTT_STATE: 0 = CONNECTED"); break;
    case  1: Serial.println("MQTT_STATE: 1 = BAD_PROTOCOL"); break;
    case  2: Serial.println("MQTT_STATE: 2 = BAD_CLIENT_ID"); break;
    case  3: Serial.println("MQTT_STATE: 3 = UNAVAILABLE"); break;
    case  4: Serial.println("MQTT_STATE: 4 = BAD_CREDENTIALS"); break;
    case  5: Serial.println("MQTT_STATE: 5 = UNAUTHORIZED"); break;
    default: Serial.println("MQTT_STATE: unknown"); break;
  }
}

// -------------------- BUZZER & LED CONTROL --------------------

void buzzerOn() {
  digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_LOW ? LOW : HIGH);
}
void buzzerOff() {
  digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_LOW ? HIGH : LOW);
}

void setIdleLights() {
  digitalWrite(LED_RED_PIN, HIGH);   // Power steady
  digitalWrite(LED_GREEN_PIN, HIGH); // Idle on
  digitalWrite(LED_WHITE_PIN, LOW);  // Busy off
  buzzerOff();                       // Ensure silent when idle
}

void setBusyStart() {
  digitalWrite(LED_GREEN_PIN, LOW);  // Not idle
}

void blinkWhiteDuring(uint32_t durationMs) {
  uint32_t start = millis();
  while (millis() - start < durationMs) {
    digitalWrite(LED_WHITE_PIN, HIGH);
    delay(WHITE_BLINK_MS);
    digitalWrite(LED_WHITE_PIN, LOW);
    delay(WHITE_BLINK_MS);
  }
}

void beepOnce(uint32_t durationMs) {
  buzzerOn();
  blinkWhiteDuring(durationMs);
  buzzerOff();
}

// -------------------- LCD DISPLAY --------------------

void lcdShow(float temperature, int motion) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  if (isnan(temperature)) {
    lcd.print("--.-");
  } else {
    lcd.print(temperature, 1);
  }
  lcd.print(" C");

  lcd.setCursor(0, 1);
  lcd.print("Motion: ");
  if (motion < 0) {
    lcd.print("-");
  } else {
    lcd.print(motion);
  }
}

// -------------------- MQTT CALLBACK --------------------

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT message received, topic: ");
  Serial.print(topic);
  Serial.print(", length: ");
  Serial.println(length);

  static char buf[512];
  unsigned int len = min(length, (unsigned int)(sizeof(buf) - 1));
  memcpy(buf, payload, len);
  buf[len] = '\0';

  Serial.print("Payload: ");
  Serial.println(buf);

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, buf);
  if (err) {
    Serial.print("JSON deserialization failed: ");
    Serial.println(err.c_str());
    setBusyStart();
    beepOnce(BEEP_MS);
    setIdleLights();
    return;
  }

  float temperature = doc["temperature"] | NAN;
  int motion        = doc["motion"] | -1;
  const char* mac   = doc["mac"] | "";

  Serial.print("Decoded temperature: ");
  Serial.println(temperature);
  Serial.print("Decoded motion: ");
  Serial.println(motion);
  Serial.print("Decoded mac: ");
  Serial.println(mac);

  setBusyStart();
  beepOnce(BEEP_MS);
  lcdShow(temperature, motion);
  setIdleLights();
}

// -------------------- WIFI & MQTT CONNECT HELPERS --------------------

// Ensure Wi-Fi is connected (used in setup and before MQTT)
bool ensureWifiConnected(uint32_t timeoutMs = 20000) {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  Serial.println("ensureWifiConnected(): waiting for Wi-Fi...");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    Serial.print(".");
    Serial.print(" status=");
    Serial.println(wifiStatusToStr(WiFi.status()));
    delay(500);
  }
  Serial.println();
  Serial.print("Final WiFi status: ");
  Serial.println(wifiStatusToStr(WiFi.status()));
  return WiFi.status() == WL_CONNECTED;
}

void ensureMqttConnected() {
  // Don’t try MQTT if Wi-Fi is down
  if (!ensureWifiConnected()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi not ready");
    lcd.setCursor(0, 1);
    lcd.print("MQTT skipped");

    Serial.println("ensureMqttConnected(): WiFi not ready, skipping MQTT");
    delay(1000);
    return;
  }

  while (!mqtt.connected()) {
    String clientId = "alarm" + String((uint32_t)ESP.getEfuseMac(), HEX);

    Serial.print("Attempting MQTT connection with clientId: ");
    Serial.println(clientId);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MQTT: Connecting");
    lcd.setCursor(0, 1);
    lcd.print("ClientID:");
    lcd.print(clientId.substring(0, 6)); // short for LCD

    // Attempt to connect (still using username/password)
    if (mqtt.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD_MQTT)) {
      Serial.println("MQTT connected successfully!");
      Serial.print("Subscribing to topic: ");
      Serial.println(MQTT_TOPIC_SUB);

      mqtt.subscribe(MQTT_TOPIC_SUB);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MQTT: Connected");
      lcd.setCursor(0, 1);
      lcd.print("Sub:");
      lcd.print(MQTT_TOPIC_SUB);
      delay(1000);
    } else {
      Serial.println("MQTT connect FAILED");
      printMqttState();  // print the detailed state

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MQTT: Failed");
      lcd.setCursor(0, 1);
      lcd.print("Retry in 2s");
      delay(2000);
    }
  }
}

// -------------------- SETUP & LOOP --------------------

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("=== Smart Alarm (ESP32) starting ===");

  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_WHITE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  buzzerOff();
  setIdleLights();

  // I2C / LCD
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Alarm");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  localMac = WiFi.macAddress();
  Serial.print("Device MAC: ");
  Serial.println(localMac);

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to WiFi SSID: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi: Connecting");
  lcd.setCursor(0, 1);
  lcd.print("to hotspot...");

  if (!ensureWifiConnected()) {
    Serial.println("WiFi connection FAILED. Check SSID/password/hotspot.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi failed");
    lcd.setCursor(0, 1);
    lcd.print("Check hotspot");
    delay(1500);
    return;  // stop setup here
  }

  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Show SSID on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Ready:");
  lcd.setCursor(0, 1);
  lcd.print(WIFI_SSID);
  delay(1500);

  // ---- TLS: ignore certificate verification ----
  // WARNING: This is insecure. Use only for testing.
  secureClient.setInsecure();
  Serial.println("WiFiClientSecure set to INSECURE (no cert verification)");

  // MQTT
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);

  // 10-second countdown BEFORE MQTT
  for (int i = 10; i > 0; i--) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MQTT in:");
    lcd.print(i);
    lcd.print("s");
    lcd.setCursor(0, 1);
    lcd.print(WIFI_SSID);
    Serial.print("MQTT connect in ");
    Serial.print(i);
    Serial.println(" s");
    delay(1000);
  }

  Serial.println("Calling ensureMqttConnected()...");
  ensureMqttConnected();   // will check Wi-Fi again internally

  // Ready screen
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready");
  lcd.setCursor(0, 1);
  lcd.print("MQTT OK");
  setIdleLights();
  Serial.println("Setup complete, entering loop()");
}

void loop() {
  if (!mqtt.connected()) {
    Serial.println("Loop: MQTT not connected, calling ensureMqttConnected()");
    ensureMqttConnected();
  }
  mqtt.loop();
}
