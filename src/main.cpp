#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

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
const char* MQTT_HOST = "e70fab49237b417185f60ee78a9ba55a.s1.eu.hivemq.cloud";
const uint16_t MQTT_PORT = 8883;

const char* MQTT_TOPIC_SUB = "smart-alert/sensors/#";

String localMac;

// ISRG Root X1 (Let’s Encrypt) CA certificate
static const char ISRG_Root_X1[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgISA6wZ0pniS3pSke1Mt2rt7EUbMA0GCSqGSIb3DQEBCwUA
MEoxCzAJBgNVBAYTAlVTMRYwFAYDVQQKDA1MZXQncyBFbmNyeXB0MRMwEQYDVQQD
DApMRSBSb290IFgxMB4XDTIxMDkwNjE2MjU0NloXDTQxMDkwNjE2MjU0NlowSjEL
MAkGA1UEBhMCVVMxFjAUBgNVBAoMDUxldCdzIEVuY3J5cHQxEzARBgNVBAMMCkxF
IFJvb3QgWDEwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQCvnD7sRZqD
Z8x2aE7hJ5Q9U4e6dQ4y4m8J7czhyyu6V0cJ5C4mMZ2J4b7q7sJbXGQJv6m7d4iP
7l6c4oyf+eU8VdAELlYqv1y0o8kz2aQ7G5r8SBS6iVQFhHFs3tcQW5+1Cfl3m1mZ
D3r3G5i9S8Ke1vL3Y0j0J6htyT1gqbtppqX0kO2uhYd8r1q0gkWgP8q7g2bWmJrQ
xG8H6oQ0D6arG6kqz5hL5XfC8cBca4luaj9nQz5xgCclq0F2N/0vv0I0kq7xErxU
p2X/9l2mVQz8rAbn3h0KFE8MZ+S1L1iZf9o5a9Z9X3j0kYk3l6nO3Y0cD0u2p6dm
6oAXnEL7HSw3GxWk8iYh2S8kqN7z4yXb7Jf3vW7pH7iF1sT6m8q4fY4dN8F7d8u+
c5G4QF1f3c3r9rj1vZcB6e8RkJcQGm4lOvwXvVX8l6j0E3n0i7sD+X8vna6a5EB6
e4qI0bY8w2B0EwK9hX2dL3cZ1n3m0S8B5Jm7f2hFzE9zP1eGZ3z1S9U7B8o3b3Pq
0i3iO6m1e0tVezZp0nYVqk5dN7m2z0Dho0X8yWn9m6Jj1L9fYHq8tQIDAQABo0Iw
QDAdBgNVHQ4EFgQUYb4U7Qn8g+KmoV0qGdKk+v1pQkcwHwYDVR0jBBgwFoAUYb4U
7Qn8g+KmoV0qGdKk+v1pQkcwDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsF
AAOCAgEAxPz0oDk7KQ3pS7b1ygaTtP3a9xY0vJdV5vYg8l7mVwz8HT9V7uCz5k+f
FQ6lQ9lUNv1q7q4vJ1L8G1yQm3rXhMEd3tQG6w3p8dY0w3xv7sQWc1+UqCjOJf8Q
C2nV8uJp8/jnTgTn3c5JcO8lH8e4xgH5q8q6Ew3eI6mN0qT9mE6m2R0h8VsvjGqj
wQqz4YgQy3H+H7a0w3/8LZ7Z3m6nNl+uI2x9K2y3oO+4g5d8+8qf3iB1Zok3k1mS
4t2kU+y2pX8QHf3xw7sQ7R0Zf1V6mQ7f3q9Gm9X9b7h1KQ6gq/3Jt2eQF9QKxwXz
F8mUTu7S3p7Kz3y3m5x3kWG5dL5n4j3xv7mQhQh0k2r2qB3kqkW9h3oT7hQd1p0D
1r6XStQjPqQ3lqJkVgGgSx8zPvq4n8tVYbFfZ5nK5q9wJ8w5bQ4r3n1uYvYQPG4t
mX5V5JQn8pV4y4bQ0rV3v4j7T3QyB5c9x2k7m3hYb3dT6E7S9I5jxZs8x0iQ9N2U
z2xPZ7U3
-----END CERTIFICATE-----
)EOF";

// Clients
WiFiClientSecure secureClient;
PubSubClient mqtt(secureClient);

// Display state
float lastTemperature = NAN;
int lastMotion = -1;

// Timing
const uint32_t BEEP_MS = 500;
const uint32_t WHITE_BLINK_MS = 120;

// Buzzer wiring: set true if your buzzer is active-low (buzzes when pin is LOW).
const bool BUZZER_ACTIVE_LOW = true;

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



void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  static char buf[512];
  unsigned int len = min(length, (unsigned int)(sizeof(buf) - 1));
  memcpy(buf, payload, len);
  buf[len] = '\0';

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, buf);
  if (err) {
    setBusyStart();
    beepOnce(BEEP_MS);
    setIdleLights();
    return;
  }

  float temperature = doc["temperature"] | NAN;
  int motion = doc["motion"] | -1;
  const char* mac = doc["mac"] | "";

  setBusyStart();
  beepOnce(BEEP_MS);
  lcdShow(temperature, motion);
  setIdleLights();
}

void ensureMqttConnected() {
  while (!mqtt.connected()) {
    String clientId = "esp32-alarm-" + String((uint32_t)ESP.getEfuseMac(), HEX);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MQTT: Connecting");
    lcd.setCursor(0, 1);
    lcd.print("ClientID:");
    lcd.print(clientId.substring(0, 6)); // short for LCD

    if (mqtt.connect(clientId.c_str())) {  // no username/password
      mqtt.subscribe(MQTT_TOPIC_SUB);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MQTT: Connected");
      lcd.setCursor(0, 1);
      lcd.print("Sub:");
      lcd.print(MQTT_TOPIC_SUB);
      delay(1000);
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MQTT: Failed");
      lcd.setCursor(0, 1);
      lcd.print("Retry in 2s");
      delay(2000);
    }
  }
}


void setup() {
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

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi: Connecting");
  lcd.setCursor(0, 1);
  lcd.print("to hotspot...");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(300);
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Show "Ready" once Wi-Fi is connected
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Ready");
    lcd.setCursor(0, 1);
    lcd.print(WIFI_SSID);
    delay(1000);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi failed");
    lcd.setCursor(0, 1);
    lcd.print("Check hotspot");
    delay(1500);
  }

  // TLS
  secureClient.setCACert(ISRG_Root_X1);

  // MQTT
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  ensureMqttConnected();

  // Keep device idle/ready until first data arrives
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready");
  lcd.setCursor(0, 1);
  lcd.print("MQTT OK");
  setIdleLights();
}

void loop() {
  if (!mqtt.connected()) {
    ensureMqttConnected();
  }
  mqtt.loop();
}
