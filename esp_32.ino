#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>

// ==== Настройки Wi-Fi ====
const char* ssid = "TP-Link_093A";         // Укажи свой SSID
const char* password = "11262704";         // Укажи свой пароль

// ==== Пины ====
#define FAN_PWM_PIN 12     // PWM пин (управление вентилятором)
#define FAN_RPM_PIN 13     // RPM пин (считывание импульсов)

// ==== Переменные ====
volatile byte fanRPMCount = 0;
volatile unsigned long last_interrupt_time = 0;
unsigned long lastRPMSample = 0;
int fanSpeed = 128;  // Стартовая скорость (0–255)
const int pulsesPerRevolution = 2;

AsyncWebServer server(80);

// ==== Обработка прерывания для RPM ====
void IRAM_ATTR rpmISR() {
  unsigned long interrupt_time = micros();
  if (interrupt_time - last_interrupt_time > 1000) {
    fanRPMCount++;
    last_interrupt_time = interrupt_time;
  }
}

// ==== Настройка скорости вентилятора ====
void setFanSpeed(int speed) {
  ledcWrite(0, speed);
}

// ==== Отправка IP на Flask-бэкенд ====
void sendIPToBackend() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://192.168.0.124:5000/receive_ip";  // ← Укажи IP своего ПК
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String jsonData = "{\"ip\":\"";
    jsonData += WiFi.localIP().toString();
    jsonData += "\"}";

    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
      Serial.printf("HTTP Response code: %d\n", httpResponseCode);
      String response = http.getString();
      Serial.println("Response: " + response);
    } else {
      Serial.print("Error sending POST: ");
      Serial.println(http.errorToString(httpResponseCode).c_str());
    }

    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

void setup() {
  Serial.begin(115200);

  // ==== Настройка вентилятора ====
  pinMode(FAN_PWM_PIN, OUTPUT);
  ledcSetup(0, 25000, 8);           // канал 0, 25 кГц, 8 бит
  ledcAttachPin(FAN_PWM_PIN, 0);
  setFanSpeed(fanSpeed);

  // ==== Настройка RPM ====
  pinMode(FAN_RPM_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FAN_RPM_PIN), rpmISR, FALLING);

  // ==== Подключение к Wi-Fi ====
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nConnected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // ==== Отправка IP на Flask ====
  sendIPToBackend();

  // ==== HTTP GET: /setSpeed?speed=... ====
  server.on("/setSpeed", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("speed")) {
      int newSpeed = request->getParam("speed")->value().toInt();
      if (newSpeed >= 0 && newSpeed <= 255) {
        fanSpeed = newSpeed;
        setFanSpeed(fanSpeed);
        Serial.printf("[INFO] Установлена скорость: %d\n", newSpeed);
        request->send(200, "text/plain", "OK");
      } else {
        request->send(400, "text/plain", "Недопустимое значение скорости (0–255)");
      }
    } else {
      request->send(400, "text/plain", "Не передан параметр speed");
    }
  });

  server.begin();
}

void loop() {
  // ==== Вывод RPM каждую секунду ====
  if (millis() - lastRPMSample >= 1000) {
    int rpm = (fanRPMCount * 60) / pulsesPerRevolution;
    fanRPMCount = 0;
    lastRPMSample = millis();
    Serial.printf("RPM: %d\n", rpm);
  }
}