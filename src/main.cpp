/**
 * Project: VAG Freiburg - Minimalist UI
 * Format: "17:26+3   in 4'"
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include "secrets.h"

// --- USER SETTINGS ---
const int awakeTimeMs = 30000;  // Time to display results before sleep (ms)

// Hardware Settings
#define I2C_SDA 21
#define I2C_SCL 22

// Display Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void fetchDepartures();

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== Starting VAG Departure Display ===");

  // 1. Init Display
  Serial.println("1. Initializing display...");
  Wire.begin(I2C_SDA, I2C_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("   FAILED: SSD1306 allocation failed");
    for(;;);
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
    // Outline
    display.drawRect(x, y, barWidth, barHeight, WHITE);
    // Fill
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

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 40) {
    delay(250);
    Serial.print(".");
    showProgress(timeout * 100 / 40 / 3); // 0-33%
    timeout++;
  }

  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("\n   FAILED: Could not connect to WiFi");
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(30, 28);
    display.print("WiFi Error!");
    display.display();
    delay(2000); esp_deep_sleep_start();
  }
  Serial.println("\n   OK - Connected!");
  Serial.print("   IP: ");
  Serial.println(WiFi.localIP());
  showProgress(33);

  // 3. Time Sync
  Serial.println("3. Syncing time with NTP...");
  configTime(0, 0, "pool.ntp.org");
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  time_t now = time(nullptr);
  int retry = 0;
  while (now < 8 * 3600 * 2 && retry < 10) {
    delay(500);
    Serial.print(".");
    showProgress(33 + retry * 100 / 10 / 3); // 33-66%
    now = time(nullptr);
    retry++;
  }
  Serial.println("\n   OK - Time synced");
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  Serial.printf("   Current time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  showProgress(66);

  // 4. Get Data
  Serial.println("4. Fetching departure data...");
  showProgress(80);
  fetchDepartures();

  // 5. Shutdown WiFi
  Serial.println("5. Shutting down WiFi...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("   OK");

  Serial.printf("6. Displaying for %d ms before sleep...\n", awakeTimeMs);
  delay(awakeTimeMs);

  // 6. Sleep
  Serial.println("7. Going to deep sleep...");
  display.clearDisplay();
  display.display();
  esp_deep_sleep_start();
}

void fetchDepartures() {
  Serial.println("   Starting HTTP request...");

  String url = "https://v6.db.transport.rest/stops/" + String(STATION_ID) + "/departures?duration=60&results=15";
  Serial.print("   URL: ");
  Serial.println(url);

  const int maxRetries = 3;
  int httpCode = 0;

  for (int attempt = 1; attempt <= maxRetries; attempt++) {
    Serial.printf("   Attempt %d/%d...\n", attempt, maxRetries);

    WiFiClientSecure client;
    client.setInsecure();
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
      WiFiClient *stream = http.getStreamPtr();

      StaticJsonDocument<200> filter;
      filter["departures"][0]["direction"] = true;
      filter["departures"][0]["when"] = true;
      filter["departures"][0]["delay"] = true;

      DynamicJsonDocument doc(8192);
      deserializeJson(doc, *stream, DeserializationOption::Filter(filter));

      display.clearDisplay();

      JsonArray departures = doc["departures"];
      int matches = 0;
      int rowY[3] = {0, 24, 48};

      time_t now = time(nullptr);

      for (JsonObject dep : departures) {
        if (matches >= 3) break;

        String direction = dep["direction"].as<String>();

        // Check if direction matches any filter keyword
        String filters = DIRECTION_FILTER;
        bool matchesFilter = filters.isEmpty();
        if (!matchesFilter) {
          int start = 0;
          while (start < filters.length()) {
            int comma = filters.indexOf(',', start);
            if (comma == -1) comma = filters.length();
            String keyword = filters.substring(start, comma);
            keyword.trim();
            if (direction.indexOf(keyword) != -1) {
              matchesFilter = true;
              break;
            }
            start = comma + 1;
          }
        }

        if (matchesFilter) {

          String timeString = dep["when"].as<String>();
          int delaySec = dep["delay"] | 0;
          int delayMin = delaySec / 60;

          int yr, mo, dy, hr, mn, sc;
          sscanf(timeString.c_str(), "%d-%d-%dT%d:%d:%d", &yr, &mo, &dy, &hr, &mn, &sc);

          int plannedHr = hr;
          int plannedMn = mn;
          int plannedSc = sc - delaySec;
          while (plannedSc < 0) { plannedSc += 60; plannedMn--; }
          while (plannedMn < 0) { plannedMn += 60; plannedHr--; }
          if (plannedHr < 0) plannedHr += 24;

          Serial.print("Direction: ");
          Serial.println(direction);
          Serial.printf("  Planned: %02d:%02d | Delay: %d sec (%d min) | Real: %02d:%02d\n",
                        plannedHr, plannedMn, delaySec, delayMin, hr, mn);

          struct tm depTm = {0};
          depTm.tm_year = yr - 1900; depTm.tm_mon = mo - 1; depTm.tm_mday = dy;
          depTm.tm_hour = hr; depTm.tm_min = mn; depTm.tm_sec = sc;
          depTm.tm_isdst = -1;

          time_t depTime = mktime(&depTm);
          double diffSeconds = difftime(depTime, now);
          int minutesLeft = diffSeconds / 60;
          if (minutesLeft < 2) continue;

          int y = rowY[matches];

          display.setTextColor(WHITE);
          display.setTextSize(2);
          display.setCursor(0, y);
          char hrBuf[3], mnBuf[3];
          sprintf(hrBuf, "%02d", hr);
          sprintf(mnBuf, "%02d", mn);
          display.print(hrBuf);
          display.print(":");
          display.setCursor(36, y);
          display.print(mnBuf);

          if (delayMin != 0) {
            display.setTextSize(1);
            display.setCursor(62, y);
            if (delayMin > 0) display.print("+");
            display.print(delayMin);
          }

          display.setTextSize(1);
          display.setCursor(83, y + 4);
          display.print("in");

          display.setTextSize(2);
          int xPos = 110;
          if (minutesLeft >= 10) xPos = 98;

          display.setCursor(xPos, y);
          display.print(minutesLeft);
          display.setTextSize(1);
          display.print("'");

          matches++;
        }
      }

      Serial.printf("   Found %d matching departures\n", matches);
      if (matches == 0) {
        display.setCursor(0, 20);
        display.setTextSize(1);
        display.println("No Trams found");
      }
      display.display();
      Serial.println("   Display updated");
      http.end();
      return; // Success, exit function
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