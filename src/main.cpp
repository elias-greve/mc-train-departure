/**
 * Project: VAG Freiburg - Minimalist UI
 * Format: "17:26+3   in 4'"
 */

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Wire.h>

#include "departure_logic.h"
#include "secrets.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

// --- USER SETTINGS ---
const int awakeTimeMs = 150000;  // Time to display results before sleep (ms); switch cuts power on release

// Hardware Settings
#define I2C_SDA 21
#define I2C_SCL 22

// Display Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void fetchDepartures();

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Disable brownout detector

  Serial.begin(115200);
  Serial.println("\n\n=== Starting VAG Departure Display ===");

  // 1. Init Display
  Serial.println("1. Initializing display...");
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("   FAILED: SSD1306 allocation failed");
    for (;;);
  }
  Serial.println("   OK");
  display.clearDisplay();
  display.setTextColor(WHITE);

  // Progress bar helper: draws bar at center of screen
  auto showProgress = [](int percent) {
    display.clearDisplay();
    int barWidth = 80;
    int barHeight = 6;
    int x = (128 - barWidth) / 2;
    int y = (64 - barHeight) / 2;
    display.drawRect(x, y, barWidth, barHeight, WHITE);
    int fillWidth = (barWidth - 2) * percent / 100;
    if (fillWidth > 0) {
      display.fillRect(x + 1, y + 1, fillWidth, barHeight - 2, WHITE);
    }
    display.display();
  };

  // 2. WiFi
  Serial.print("2. Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  showProgress(0);

  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_11dBm);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 40) {
    delay(250);
    Serial.print(".");
    showProgress(timeout * 100 / 40 / 2);  // 0-50%
    timeout++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n   FAILED: Could not connect to WiFi");
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(30, 28);
    display.print("WiFi Error!");
    display.display();
    delay(2000);
    esp_deep_sleep_start();
  }
  Serial.println("\n   OK - Connected!");
  Serial.print("   IP: ");
  Serial.println(WiFi.localIP());
  showProgress(50);

  // 3. Get Data
  Serial.println("3. Fetching departure data...");
  showProgress(80);
  fetchDepartures();

  // 4. Shutdown WiFi
  Serial.println("4. Shutting down WiFi...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("   OK");

  Serial.printf("5. Displaying for %d ms before sleep...\n", awakeTimeMs);
  delay(awakeTimeMs);

  // 5. Sleep
  Serial.println("6. Going to deep sleep...");
  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  esp_deep_sleep_start();
}

void fetchDepartures() {
  Serial.println("   Starting HTTP request...");

  String url =
      "http://efa.vagfr.de/vagfr3/XSLT_DM_REQUEST"
      "?outputFormat=JSON&language=de&stateless=1"
      "&type_dm=stop&name_dm=" +
      String(STATION_ID) + "&mode=direct&useRealtime=1&limit=15&depType=stopEvents";
  Serial.print("   URL: ");
  Serial.println(url);

  const int maxRetries = 3;
  int httpCode = 0;

  for (int attempt = 1; attempt <= maxRetries; attempt++) {
    Serial.printf("   Attempt %d/%d...\n", attempt, maxRetries);

    WiFiClient client;
    client.setTimeout(15000);
    HTTPClient http;
    http.setTimeout(15000);

    if (!http.begin(client, url)) {
      Serial.println("   HTTP begin failed!");
      http.end();
      if (attempt < maxRetries) {
        delay(1000);
        continue;
      }
      // All retries failed
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 24);
      display.print("Connection failed");
      display.setCursor(0, 40);
      display.print("Check WiFi");
      display.display();
      return;
    }

    httpCode = http.GET();
    Serial.printf("   HTTP response code: %d\n", httpCode);

    // Retry on timeout or connection errors
    if (httpCode < 0 && attempt < maxRetries) {
      Serial.println("   Retrying...");
      http.end();
      delay(2000);
      continue;
    }

    if (httpCode == HTTP_CODE_OK) {
      Serial.println("   Parsing JSON...");
      String body = http.getString();
      http.end();

      // Parse via shared logic; retry on parse failure just like HTTP errors.
      DeparturesResult parsed = parseDeparturesJson(body.c_str(), 15);
      if (!parsed.success) {
        Serial.println("   JSON parse error, retrying...");
        if (attempt < maxRetries) {
          delay(2000);
          continue;
        }
        // All retries exhausted with parse errors
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 20);
        display.print("Parse error");
        display.setCursor(0, 36);
        display.print("Try again later");
        display.display();
        return;
      }

      display.clearDisplay();

      int matches = 0;
      int rowY[3] = {0, 24, 48};

      for (int i = 0; i < parsed.count && matches < 3; i++) {
        Departure* dep = &parsed.departures[i];
        if (!dep->valid) continue;
        if (!matchesDirectionFilter(dep->direction, DIRECTION_FILTER)) continue;
        if (dep->countdown < 2) continue;

        Serial.print("Direction: ");
        Serial.println(dep->direction);
        Serial.printf("  Sched: %02d:%02d | Real: %02d:%02d | Delay: %d min | Countdown: %d\n", dep->schedHour,
                      dep->schedMinute, dep->realHour, dep->realMinute, dep->delayMin, dep->countdown);

        int y = rowY[matches];

        display.setTextColor(WHITE);
        display.setTextSize(2);
        display.setCursor(0, y);
        char hrBuf[3], mnBuf[3];
        snprintf(hrBuf, sizeof(hrBuf), "%02d", dep->realHour);
        snprintf(mnBuf, sizeof(mnBuf), "%02d", dep->realMinute);
        display.print(hrBuf);
        display.print(":");
        display.setCursor(36, y);
        display.print(mnBuf);

        if (dep->delayMin != 0) {
          display.setTextSize(1);
          display.setCursor(62, y);
          // Clamp displayed delay to [-99, +99]; show "!" when out of range.
          int displayDelay = dep->delayMin;
          bool delayOverflow = (displayDelay > 99 || displayDelay < -99);
          if (delayOverflow) {
            display.print("!");
          } else {
            if (displayDelay > 0) display.print("+");
            display.print(displayDelay);
          }
        }

        display.setTextSize(1);
        display.setCursor(83, y + 4);
        display.print("in");

        display.setTextSize(2);
        int xPos = 110;
        if (dep->countdown >= 10) xPos = 98;

        display.setCursor(xPos, y);
        display.print(dep->countdown);
        display.setTextSize(1);
        display.print("'");

        matches++;
      }

      Serial.printf("   Found %d matching departures\n", matches);
      if (matches == 0) {
        display.setCursor(0, 20);
        display.setTextSize(1);
        display.println("No Trams found");
      }
      display.display();
      Serial.println("   Display updated");
      return;  // Success, exit function
    }

    // Error handling
    Serial.printf("   HTTP error: %d\n", httpCode);
    http.end();

    if (attempt == maxRetries) {
      // All retries exhausted, show error
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 20);
      if (httpCode < 0) {
        display.print("Network error");
        display.setCursor(0, 36);
        display.print("Code: ");
        display.print(httpCode);
      } else {
        display.print("Server error: ");
        display.print(httpCode);
      }
      display.setCursor(0, 52);
      display.print("Try again later");
      display.display();
    }
  }
}

void loop() {}
