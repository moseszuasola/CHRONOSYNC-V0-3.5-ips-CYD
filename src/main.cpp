#include "MontserratBoldAMPM.h"
#include "MontserratBoldClock56.h"
#include "SplashText.h"
#include "web_index.h"
#include "wifi_qr_data.h"
#include <Arduino.h>
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <time.h>
#include <vector>

#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define BOOT_BUTTON 0

#define TAILWIND_YELLOW 0xF642
#define TAILWIND_CYAN 0x269D
#define TAILWIND_SLATE 0x10A2
#define TAILWIND_ORANGE 0xFC60

const char *ntpServer = "pool.ntp.org";
long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;
const char *apSSID = "ChronoSync-Setup";
const char *apPassword = "";
bool isAPMode = false;
bool isFirstBoot = true;
bool apModeActive = false;
unsigned long wifiConnectStartTime = 0;
const unsigned long WIFI_CONNECT_TIMEOUT = 20000;
unsigned long lastAPScreenUpdate = 0;

#define SD_CS 5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK 18

#define TFT_BL 27
#define BL_PWM_CHANNEL 0
#define BL_PWM_FREQ 5000
#define BL_PWM_RESOLUTION 8

TFT_eSPI tft = TFT_eSPI();
WebServer server(80);
DNSServer dnsServer;
SPIClass sdSPI(HSPI);
File uploadFile;
Preferences preferences;

String ssid = "";
String password = "";
String currentFile = "";
bool isSlideshow = true;
bool sdReady = false;
unsigned long lastSlideTime = 0;
int slideInterval = 5000;
std::vector<String> mediaFiles;
int currentMediaIndex = 0;
String currentOrientation = "landscape";
long bootId = 0;

unsigned long lastMessageUpdate = 0;
int currentMessageIndex = 0;

const char *encouragementMessages[] = {
    "You're doing great", "Stay focused!",       "Keep it up!",
    "You've got this!",   "Believe in yourself", "One step at a time",
    "You're amazing!",    "Never give up!"};
const int messageCount = 8;

bool clockEnabled = true;
bool clockShowAtEnd = true;
bool isShowingClock = false;
bool isClockOnlyMode = false;
bool clockOnlyScreen = false;
int clockDisplayDuration = 30;
int clockPhotoInterval = 0;
int photosSinceLastClock = 0;
unsigned long clockStartTime = 0;
unsigned long lastClockUpdate = 0;
bool use24Hour = false;
String timezoneName = "UTC+8 Manila";
String barPosition = "top";
String customMessages =
    "You're amazing!\\nKeep it up!\\nStay focused!\\nNever give up!";
String clockTheme = "dark";

String devicePIN = "1234";
bool pinChangeRequired = false;
bool isAuthenticated = false;
time_t authTimestamp = 0;          // Track when user authenticated (epoch time)
const time_t AUTH_EXPIRY = 259200; // 3 days in seconds (72 hours)

// Industrial-Grade Stability Features
#define WDT_TIMEOUT 15
int currentBrightness = 204; // 80% brightness (0-255)
int dimBrightness = 26;      // 10% brightness for night mode
unsigned long lastHeapCheck = 0;
unsigned long lastMaintenanceCheck = 0;
const unsigned long HEAP_CHECK_INTERVAL =
    30000; // Check heap every 30 seconds (increased frequency)
const unsigned long MAINTENANCE_CHECK_INTERVAL =
    3600000; // Check for maintenance every hour
const uint32_t MIN_FREE_HEAP =
    40000; // 20% of ~200KB typical free heap (emergency threshold)
const uint32_t LOW_HEAP_WARNING =
    60000; // Warning threshold (30% of typical free heap)

// Thread Safety & SD Card Monitoring
SemaphoreHandle_t stateMutex =
    NULL; // Protects shared variables from race conditions
unsigned long lastSDCheck = 0;
const unsigned long SD_CHECK_INTERVAL = 5000; // Check SD card every 5 seconds
bool sdCardPresent = false;

bool touchEnabled = true;
bool statusBarVisible = true;
unsigned long lastTouchTime = 0;
unsigned long touchStartTime = 0;
int touchStartX = 0, touchStartY = 0;
int lastTapX = 0, lastTapY = 0;
unsigned long lastTapTime = 0;
bool isTouching = false;
bool longPressHandled = false;

bool gifPreviewMode = false;
std::vector<String> gifFiles;
int currentGifPreviewIndex = 0;
bool showingWiFiQR = false;

#define TOUCH_THRESHOLD 50
#define LONG_PRESS_TIME 2000
#define DOUBLE_TAP_TIME 400
#define DEBOUNCE_TIME 200

#define LINE_BUFFER_SIZE 32768
uint8_t lineBuffer[LINE_BUFFER_SIZE];

void showSlideshow();
void showSplashScreen();
void forceClockRedraw();
void drawStatusBar();
void drawStatusBar() {
  // Status bar removed per UX revision - no overlay bar on photos or clock
  return;
}

void showClock();
void updateClock(bool forceRedraw = false);
void forceClockRedraw();
void drawSystemText(const char *text, int16_t x, int16_t y, uint16_t color,
                    uint8_t font);
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);
void scanDirectory(const String &path, std::vector<String> &outFiles);

void drawMontDigitGlyph(int16_t x, int16_t y, char ch,
                        uint16_t fgColor = TFT_BLACK,
                        uint16_t bgColor = TFT_WHITE, float scale = 1.0) {
  const char *chars = MontserratBoldClock56Chars;
  const char *found = strchr(chars, ch);
  if (!found)
    return;
  int idx = found - chars;
  uint16_t offset = pgm_read_word(&MontserratBoldClock56Offsets[idx]);
  uint8_t w = pgm_read_byte(&MontserratBoldClock56Width[idx]);
  uint8_t h = pgm_read_byte(&MontserratBoldClock56Height[idx]);

  int scaledW = (int)(w * scale);
  int scaledH = (int)(h * scale);

  uint16_t rowBuf[128];

  for (int row = 0; row < scaledH; ++row) {
    int srcRow = (int)(row / scale);
    if (srcRow >= h)
      srcRow = h - 1;

    for (int col = 0; col < scaledW; ++col) {
      int srcCol = (int)(col / scale);
      if (srcCol >= w)
        srcCol = w - 1;

      uint16_t v = pgm_read_word(
          &MontserratBoldClock56Bitmaps[offset + srcRow * w + srcCol]);

      // v == 0x0000 is background (transparent), any other value is foreground
      rowBuf[col] = (v == 0x0000) ? bgColor : fgColor;
    }
    tft.pushImage(x, y + row, scaledW, 1, rowBuf);
    
    // Feed WDT & Yield every 8 rows during heavy upscaling rendering (e.g. 2.8x scale)
    if (row % 8 == 0) {
        esp_task_wdt_reset();
        yield();
    }
  }
}

void drawMontDigitString(int16_t x, int16_t y, const char *s,
                         uint8_t spacing = 4, uint16_t fgColor = TFT_BLACK,
                         uint16_t bgColor = TFT_WHITE, float scale = 1.0) {
  int16_t curX = x;
  for (const char *p = s; *p; ++p) {
    const char *found = strchr(MontserratBoldClock56Chars, *p);
    if (!found) {
      // unknown char - skip a bit
      curX += (int)(spacing * scale);
      continue;
    }
    int idx = found - MontserratBoldClock56Chars;
    uint8_t w = pgm_read_byte(&MontserratBoldClock56Width[idx]);
    // Move colon down 1px for better centering
    int16_t yOffset = (*p == ':') ? 1 : 0;
    drawMontDigitGlyph(curX, y + yOffset, *p, fgColor, bgColor, scale);
    curX += (int)(w * scale) + (int)(spacing * scale);
  }
}

int16_t getMontStringWidth(const char *s, uint8_t spacing = 4, float scale = 1.0) {
  int16_t totalW = 0;
  for (const char *p = s; *p; ++p) {
    const char *found = strchr(MontserratBoldClock56Chars, *p);
    if (!found) {
      totalW += (int)(spacing * scale);
      continue;
    }
    int idx = found - MontserratBoldClock56Chars;
    uint8_t w = pgm_read_byte(&MontserratBoldClock56Width[idx]);
    totalW += (int)(w * scale) + (int)(spacing * scale);
  }
  if (totalW > 0) totalW -= (int)(spacing * scale);
  return totalW;
}

// Draw AM/PM string using smaller font
void drawAMPMString(int16_t x, int16_t y, const char *s, uint8_t spacing = 2,
                    uint16_t fgColor = TFT_BLACK, uint16_t bgColor = TFT_WHITE,
                    float scale = 1.0) {
  int16_t curX = x;
  for (const char *p = s; *p; ++p) {
    const char *found = strchr(MontserratBoldAMPMChars, *p);
    if (!found) {
      curX += (int)(spacing * scale);
      continue;
    }
    int idx = found - MontserratBoldAMPMChars;
    uint16_t offset = pgm_read_word(&MontserratBoldAMPMOffsets[idx]);
    uint8_t w = pgm_read_byte(&MontserratBoldAMPMWidth[idx]);
    uint8_t h = pgm_read_byte(&MontserratBoldAMPMHeight[idx]);

    int scaledW = (int)(w * scale);
    int scaledH = (int)(h * scale);
    uint16_t rowBuf[128];

    for (int row = 0; row < scaledH; ++row) {
      int srcRow = (int)(row / scale);
      if (srcRow >= h)
        srcRow = h - 1;

      for (int col = 0; col < scaledW; ++col) {
        int srcCol = (int)(col / scale);
        if (srcCol >= w)
          srcCol = w - 1;

        uint16_t v = pgm_read_word(
            &MontserratBoldAMPMBitmaps[offset + srcRow * w + srcCol]);

        // v == 0x0000 is background, anything else is foreground
        rowBuf[col] = (v == 0x0000) ? bgColor : fgColor;
      }
      tft.pushImage(curX, y + row, scaledW, 1, rowBuf);
    }
    curX += (int)(w * scale) + (int)(spacing * scale);
  }
}
void playRawVideo(String filename, bool infiniteLoop);
void playMJPEG(String filename, bool infiniteLoop);
void loadOrder(String orientation);
void saveOrderConfig(String orientation, DynamicJsonDocument &doc);
void setupWiFiWithFallback();
void startAPMode();
void showAPSetupScreen();
void drawWiFiQRCode(int x, int y);
bool scanAndConnectToWiFi(String newSSID, String newPassword);
String scanWiFiNetworks();
void loadWiFiConfig();
void saveWiFiConfig(String s, String p);
void setHardwareRotation(String orientation);
void saveDeviceState();
void loadDeviceState();
void savePIN();
void loadPIN();
void saveAuthState();
void loadAuthState();
bool checkAuth(); // Check if authentication session is still valid
bool readTouch(uint16_t *x, uint16_t *y);
void handleTouch();
void enterGifPreviewMode();
void exitGifPreviewMode();
void showGifPreview(int index);
void navigateGifPreview(int direction);
void showWiFiQROverlay();
void hideWiFiQROverlay();
void checkHeapHealth();
void checkScheduledMaintenance();
void setBacklightBrightness(int brightness);
void updateBacklightForTimeOfDay();
void checkSDCardStatus();
void handleSDCardRemoval();

void setup() {
  Serial.begin(115200);
  WiFi.setTxPower(WIFI_POWER_11dBm);
  bootId = millis();

  // === THREAD SAFETY: Create mutex for shared variable protection ===
  stateMutex = xSemaphoreCreateMutex();
  if (stateMutex == NULL) {
    Serial.println("[ERROR] Failed to create mutex - race conditions possible");
  } else {
    Serial.println("[MUTEX] State mutex created successfully");
  }

  // === INDUSTRIAL STABILITY: Initialize Hardware Watchdog ===
  Serial.println("Initializing Task Watchdog Timer (30s timeout)...");
  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);
  Serial.println(
      "Hardware WDT enabled - system will auto-reboot if frozen > 30s");

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (SD.begin(SD_CS, sdSPI, 20000000)) { // 20MHz SPI for stability
    sdReady = true;
    sdCardPresent = true;
    SD.mkdir("/photos/landscape");
    SD.mkdir("/photos/portrait");
    SD.mkdir("/gifs/landscape");
    SD.mkdir("/gifs/portrait");
    SD.mkdir("/config");
    loadWiFiConfig();
  } else {
    Serial.println("[SD] SD card not detected at startup");
    sdCardPresent = false;
  }

  // SPI.begin here ensure hardware is ready before TFT/Touch
  SPI.begin(14, 12, 13, -1);
  tft.begin();

  // === INDUSTRIAL STABILITY: PWM Backlight Control ===
  pinMode(TFT_BL, OUTPUT);
  ledcSetup(BL_PWM_CHANNEL, BL_PWM_FREQ, BL_PWM_RESOLUTION);
  ledcAttachPin(TFT_BL, BL_PWM_CHANNEL);
  setBacklightBrightness(currentBrightness); // Start at 80% to reduce heat
  Serial.printf("Backlight PWM initialized: %d%% brightness\n",
                (currentBrightness * 100) / 255);
  delay(100);

  setHardwareRotation(currentOrientation);
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(false);
  delay(50);

  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  pinMode(TOUCH_IRQ, INPUT);
  Serial.println("Touch initialized (XPT2046)");

  showSplashScreen();
  unsigned long splashStart = millis();

  // Init JPEG Decoder with callback
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(
      false); // Default false; playMJPEG sets true locally for speed
  TJpgDec.setCallback(tft_output);

  // Initialize boot button
  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  // Check for reset button hold during splash screen
  bool resetRequested = false;
  while (millis() - splashStart < 3000) {
    if (digitalRead(BOOT_BUTTON) == LOW) {
      resetRequested = true;
    } else {
      resetRequested = false;
    }
    delay(100);
  }

  // If reset button was held, perform factory reset
  if (resetRequested) {
    // Feed watchdog during factory reset
    esp_task_wdt_reset();

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("FACTORY RESET", 240, 120, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Resetting all settings...", 240, 160, 2);

    esp_task_wdt_reset();

    // Clear WiFi credentials from NVS
    preferences.begin("chronosync", false);
    preferences.clear();
    preferences.end();

    esp_task_wdt_reset();

    // Remove WiFi config from SD card
    if (sdReady) {
      SD.remove("/config/wifi.json");
      SD.remove("/config/wifi.txt");
      SD.remove("/config/pin.txt");
    }

    esp_task_wdt_reset();

    // Reset PIN to default and save
    preferences.begin("chronosync", false);
    devicePIN = "1234";
    pinChangeRequired = true;
    preferences.putString("device_pin", devicePIN);
    preferences.putBool("pin_change_required", pinChangeRequired);
    preferences.end();

    esp_task_wdt_reset();

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("RESET COMPLETE!", 240, 120, 4);
    tft.drawString("WiFi cleared", 240, 160, 2);
    tft.drawString("PIN reset to: 1234", 240, 190, 2);
    tft.drawString("Restarting...", 240, 220, 2);

    esp_task_wdt_reset();
    delay(2000);
    esp_task_wdt_reset();

    ESP.restart();
  }

  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(false);

  setupWiFiWithFallback();

  tft.setSwapBytes(true);

  server.on("/", HTTP_GET, []() {
    esp_task_wdt_reset();
    if (isAPMode) {
      server.sendHeader("Location", "/setup", true);
      server.send(302, "text/plain", "");
    } else {
      // Proper chunked transfer for Gzipped page - required for browser compatibility
      server.sendHeader("Content-Encoding", "gzip");
      server.sendHeader("Cache-Control", "no-cache");
      server.setContentLength(CONTENT_LENGTH_UNKNOWN); // Enable chunked transfer
      server.send(200, "text/html", "");

      const size_t chunkSize = 2048; // Smaller chunks for reliability
      size_t sent = 0;
      uint8_t buffer[chunkSize];

      while (sent < MAIN_page_len) {
        uint32_t toSend = min((uint32_t)chunkSize, (uint32_t)(MAIN_page_len - sent));
        memcpy_P(buffer, MAIN_page + sent, toSend);
        server.sendContent((const char *)buffer, toSend);
        sent += toSend;
        esp_task_wdt_reset();
        yield();
      }
      server.sendContent(""); // Terminate chunked transfer
    }
  });

  // Auth endpoints removed - access is via IP address only (no PIN required)
  server.on("/api/auth", HTTP_POST, []() {
    server.send(200, "application/json",
                "{\"success\":true,\"requireChange\":false}");
  });

  server.on("/api/status", HTTP_GET, []() {
    String json = "{";
    json += "\"slideshow\":" + String(isSlideshow ? "true" : "false") + ",";
    json += "\"orientation\":\"" + currentOrientation + "\",";
    json += "\"bootId\":" + String(bootId);
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/api/slideshow/start", HTTP_GET, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }

    // === THREAD SAFETY: Acquire mutex before modifying shared state ===
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      // Load photos for current orientation
      loadOrder(currentOrientation);

      if (mediaFiles.empty()) {
        xSemaphoreGive(stateMutex);
        server.send(400, "application/json",
                    "{\"error\":\"No photos found in this orientation\"}");
        return;
      }

      // Set hardware rotation to match orientation
      setHardwareRotation(currentOrientation);

      // Clear screen
      tft.fillScreen(TFT_BLACK);

      // Start with first photo
      currentMediaIndex = 0;
      String firstPhoto = mediaFiles[0];
      currentFile = firstPhoto;
      isSlideshow = true;
      isShowingClock = false;
      isClockOnlyMode = false;
      clockOnlyScreen =
          false; // Turn off clock only mode when starting slideshow
      photosSinceLastClock = 0;
      lastSlideTime = millis();

      // Display the first photo immediately
      if (firstPhoto.endsWith(".jpg") || firstPhoto.endsWith(".jpeg")) {
        tft.setSwapBytes(true);
        TJpgDec.drawSdJpg(0, 0, firstPhoto);
      }

      Serial.printf("[SLIDESHOW] Started in %s with %s\n",
                    currentOrientation.c_str(), firstPhoto.c_str());

      xSemaphoreGive(stateMutex);
      // === MUTEX RELEASED ===

      saveDeviceState(); // Save state
      server.send(200, "application/json", "{\"ok\":true}");
    } else {
      server.send(503, "application/json",
                  "{\"error\":\"System busy, try again\"}");
    }
  });

  server.on("/api/slideshow/stop", HTTP_GET, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }

    // === THREAD SAFETY: Acquire mutex ===
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      isSlideshow = false;
      isShowingClock = false;
      isClockOnlyMode = false;
      xSemaphoreGive(stateMutex);

      saveDeviceState(); // Save state
      server.send(200, "application/json", "{\"ok\":true}");
    } else {
      server.send(503, "application/json",
                  "{\"error\":\"System busy, try again\"}");
    }
  });

  server.on("/api/play", HTTP_GET, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }
    if (server.hasArg("file")) {
      String f = server.arg("file");
      if (SD.exists(f)) {
        // === THREAD SAFETY: Acquire mutex ===
        if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
          currentFile = f;
          isSlideshow = false;
          isShowingClock = false; // Hide clock when playing any media
          clockOnlyScreen =
              false; // Turn off clock only mode when playing media
          isClockOnlyMode = false;

          // Apply saved orientation when starting new media
          setHardwareRotation(currentOrientation);

          xSemaphoreGive(stateMutex);
          // === MUTEX RELEASED ===

          saveDeviceState(); // Save state

          if (f.endsWith(".vid")) {
            server.send(200, "application/json", "{\"ok\":true}");
            // Playback happens in loop()
          } else {
            tft.fillScreen(TFT_BLACK);
            tft.setSwapBytes(true);
            TJpgDec.drawSdJpg(0, 0, currentFile);
            server.send(200, "application/json", "{\"ok\":true}");
          }
        } else {
          server.send(503, "application/json",
                      "{\"error\":\"System busy, try again\"}");
        }
      } else
        server.send(404, "application/json", "{\"error\":\"File not found\"}");
    }
  });

  server.on("/api/photo/preview", HTTP_GET, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }

    // === HEAP CHECK: Prevent crashes during bulk preview requests ===
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 50000) {
      Serial.printf(
          "[API] /api/photo/preview rejected - low memory: %u bytes\n",
          freeHeap);
      server.send(507, "text/plain", "Low memory");
      return;
    }

    if (server.hasArg("filename")) {
      String f = server.arg("filename");
      if (SD.exists(f)) {
        File file = SD.open(f, "r");
        if (!file) {
          server.send(500, "text/plain", "Failed to open file");
          return;
        }
        server.streamFile(file, "image/jpeg");
        file.close();
      } else {
        server.send(404, "text/plain", "Not Found");
      }
    } else {
      server.send(400, "text/plain", "Missing filename parameter");
    }
  });

  server.on("/api/order", HTTP_POST, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }
    if (!server.hasArg("plain"))
      return server.send(400, "text/plain", "Body missing");
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, server.arg("plain"));
    String orient = doc["orientation"].as<String>();
    saveOrderConfig(orient, doc);
    if (orient == currentOrientation)
      loadOrder(currentOrientation);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/list", HTTP_GET, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }

    // === HEAP CHECK: Prevent memory exhaustion ===
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 60000) {
      Serial.printf("[API] /api/list rejected - low memory: %u bytes\n",
                    freeHeap);
      server.send(507, "text/plain", "Insufficient memory - try again");
      return;
    }

    Serial.printf("[API] /api/list starting - Free heap: %u bytes\n", freeHeap);

    // Use a single vector and build JSON more efficiently
    std::vector<String> allFiles;
    allFiles.reserve(100); // Pre-allocate to reduce reallocations

    // Helper lambda to get files in saved order for a folder
    auto getOrderedFiles = [](const String &folder, const String &orientation,
                              std::vector<String> &out) {
      std::vector<String> savedOrder;
      String orderPath = "/config/order_" + orientation + ".json";

      // Load saved order if exists
      if (SD.exists(orderPath)) {
        File f = SD.open(orderPath);
        if (f) {
          DynamicJsonDocument doc(4096);
          DeserializationError err = deserializeJson(doc, f);
          if (!err) {
            JsonArray arr = doc["files"];
            for (JsonVariant v : arr) {
              String s = v.as<String>();
              // Only include files from this specific folder
              if (s.startsWith(folder) && SD.exists(s)) {
                savedOrder.push_back(s);
              }
            }
          }
          f.close();
        }
      }

      // Scan disk for current files
      std::vector<String> diskFiles;
      scanDirectory(folder, diskFiles);

      // Add saved order files first
      for (const String &f : savedOrder) {
        out.push_back(f);
      }

      // Add any new files not in saved order
      for (const String &f : diskFiles) {
        bool alreadyIn = false;
        for (const String &s : savedOrder) {
          if (s == f) {
            alreadyIn = true;
            break;
          }
        }
        if (!alreadyIn)
          out.push_back(f);
      }

      // Free memory explicitly
      savedOrder.clear();
      savedOrder.shrink_to_fit();
      diskFiles.clear();
      diskFiles.shrink_to_fit();
    };

    // Get ordered files for each folder
    getOrderedFiles("/photos/landscape", "landscape", allFiles);
    getOrderedFiles("/photos/portrait", "portrait", allFiles);
    getOrderedFiles("/gifs/landscape", "landscape", allFiles);
    getOrderedFiles("/gifs/portrait", "portrait", allFiles);

    // Build JSON response efficiently
    String response;
    response.reserve(allFiles.size() * 40 +
                     100); // Estimate size to reduce reallocations
    response = "{\"files\":[";

    for (size_t i = 0; i < allFiles.size(); i++) {
      if (i > 0)
        response += ",";
      response += "\"";
      response += allFiles[i];
      response += "\"";
    }
    response += "]}";

    // Clean up before sending
    allFiles.clear();
    allFiles.shrink_to_fit();

    Serial.printf("[API] /api/list sending response - Size: %d bytes, Free "
                  "heap: %u bytes\n",
                  response.length(), ESP.getFreeHeap());

    server.send(200, "application/json", response);
  });

  server.on("/api/photos", HTTP_GET, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }
    // Return photos for current orientation with full paths
    String out = "{\"photos\":[";
    for (size_t i = 0; i < mediaFiles.size(); i++) {
      out += "\"" + mediaFiles[i] + "\"";
      if (i < mediaFiles.size() - 1)
        out += ",";
    }
    out += "],\"count\":" + String(mediaFiles.size()) + "}";
    server.send(200, "application/json", out);
  });

  server.on("/api/set", HTTP_GET, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }
    if (server.hasArg("orientation")) {
      String newOrientation = server.arg("orientation");

      // Simply save the orientation preference - don't change device display
      // Device will use this orientation when user plays a photo/gif or starts
      // slideshow
      currentOrientation = newOrientation;
      saveDeviceState();

      Serial.printf(
          "[API] Orientation preference set to %s (device display unchanged)\n",
          currentOrientation.c_str());
    }
    if (server.hasArg("duration"))
      slideInterval = server.arg("duration").toInt() * 1000;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/clock/show", HTTP_GET, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }
    // Stop any currently playing media (GIF/video)
    if (currentFile.endsWith(".vid") || currentFile.endsWith(".mjpeg")) {
      currentFile = "";
    }

    // Show clock in clock-only mode (respects clockOnlyScreen setting)
    isShowingClock = true;
    isSlideshow = false;
    isClockOnlyMode =
        clockOnlyScreen; // Use the setting to determine if duration applies
    clockStartTime = millis();
    photosSinceLastClock = 0;
    // Force immediate full redraw
    forceClockRedraw();
    saveDeviceState(); // Save state
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/sync", HTTP_GET, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(100);
    if (isShowingClock)
      forceClockRedraw();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/clock/toggle", HTTP_POST, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(256);
      deserializeJson(doc, server.arg("plain"));
      clockEnabled = doc["enabled"] | false;
      // Duration is now managed through /api/clock/settings
    }
    String response =
        "{\"clockEnabled\":" + String(clockEnabled ? "true" : "false") + "}";
    server.send(200, "application/json", response);
  });

  server.on("/api/clock/config", HTTP_GET, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }
    String response =
        "{\"clockEnabled\":" + String(clockEnabled ? "true" : "false") +
        ",\"clockDisplayDuration\":" + String(clockDisplayDuration) +
        ",\"clockPhotoInterval\":" + String(clockPhotoInterval) + "}";
    server.send(200, "application/json", response);
  });

  server.on(
      "/upload", HTTP_POST, []() { server.send(200, "text/plain", "OK"); },
      []() {
        HTTPUpload &upload = server.upload();

        // === STABILITY: Feed watchdog during large uploads ===
        static unsigned long lastWDTFeed = 0;
        if (millis() - lastWDTFeed > 1000) {
          esp_task_wdt_reset();
          lastWDTFeed = millis();
        }

        if (upload.status == UPLOAD_FILE_START) {
          // === STABILITY: Check available heap before accepting upload ===
          uint32_t freeHeap = ESP.getFreeHeap();
          if (freeHeap < 50000) {
            Serial.printf("[UPLOAD] Rejecting upload - low memory: %u bytes\n",
                          freeHeap);
            server.send(507, "text/plain", "Insufficient memory");
            return;
          }

          Serial.printf("[UPLOAD] Starting upload: %s | Free heap: %u bytes\n",
                        upload.filename.c_str(), freeHeap);

          isSlideshow = false;
          currentFile = "";
          String filename = upload.filename;
          if (filename.startsWith("/"))
            filename = filename.substring(1);
          String targetOrient = server.arg("targetOrient");
          if (targetOrient == "")
            targetOrient = "landscape";

          String lowerName = filename;
          lowerName.toLowerCase();
          // Check if this is a GIF thumbnail (gifType parameter) or a
          // .vid/.mjpeg file
          bool isGifRelated = lowerName.endsWith(".vid") ||
                              lowerName.endsWith(".mjpeg") ||
                              server.hasArg("gifType");
          String folder = isGifRelated ? "/gifs/" + targetOrient
                                       : "/photos/" + targetOrient;

          if (!SD.exists(folder))
            SD.mkdir(folder);
          String path = folder + "/" + filename;
          if (SD.exists(path))
            SD.remove(path);
          uploadFile = SD.open(path, FILE_WRITE);

          if (!uploadFile) {
            Serial.println("[UPLOAD] Failed to open file for writing");
          }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
          if (uploadFile) {
            uploadFile.write(upload.buf, upload.currentSize);

            // === STABILITY: Monitor heap during upload ===
            static unsigned long lastHeapLog = 0;
            if (millis() - lastHeapLog > 5000) {
              uint32_t freeHeap = ESP.getFreeHeap();
              Serial.printf("[UPLOAD] Progress... Free heap: %u bytes\n",
                            freeHeap);

              // Emergency abort if critically low
              if (freeHeap < 30000) {
                Serial.println(
                    "[UPLOAD] CRITICAL: Memory too low, aborting upload");
                uploadFile.close();
                return;
              }
              lastHeapLog = millis();
            }
          }
        } else if (upload.status == UPLOAD_FILE_END) {
          if (uploadFile) {
            uploadFile.close();
            uint32_t freeHeap = ESP.getFreeHeap();
            Serial.printf("[UPLOAD] Upload complete | Free heap: %u bytes\n",
                          freeHeap);
            loadOrder(currentOrientation);
          }
        }
      });

  server.on("/api/photo/delete", HTTP_POST, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }
    DynamicJsonDocument doc(512);
    deserializeJson(doc, server.arg("plain"));
    String f = doc["filename"];
    if (SD.exists(f))
      SD.remove(f);
    loadOrder(currentOrientation);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/wifi", HTTP_POST, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }
    DynamicJsonDocument doc(512);
    deserializeJson(doc, server.arg("plain"));
    saveWiFiConfig(doc["ssid"], doc["pass"]);
    server.send(200, "text/plain", "Rebooting");
    delay(500);
    ESP.restart();
  });

  server.on("/api/clock/settings", HTTP_POST, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(2048); // expanded from 512 to handle large payloads
      deserializeJson(doc, server.arg("plain"));

      bool wasClockOnlyMode = clockOnlyScreen; // Track previous state

      // Update clock format (12hr/24hr)
      if (doc.containsKey("use24Hour")) {
        use24Hour = doc["use24Hour"] | false;
      }
      if (doc.containsKey("timezoneOffset")) {
        gmtOffset_sec = doc["timezoneOffset"].as<int>();
      }
      if (doc.containsKey("timezoneName")) {
        timezoneName = doc["timezoneName"].as<String>();
      }
      if (doc.containsKey("clockDisplayDuration")) {
        clockDisplayDuration = doc["clockDisplayDuration"].as<int>();
      }
      if (doc.containsKey("clockPhotoInterval")) {
        clockPhotoInterval = doc["clockPhotoInterval"].as<int>();
      }
      if (doc.containsKey("clockOnlyScreen")) {
        clockOnlyScreen = doc["clockOnlyScreen"].as<bool>();
        if (clockOnlyScreen) {
          isSlideshow = false;
          isClockOnlyMode = true;
          isShowingClock = true;
        }
      }
      if (doc.containsKey("clockEnabled")) {
        clockEnabled = doc["clockEnabled"].as<bool>();
      }
      if (doc.containsKey("clockShowAtEnd")) {
        clockShowAtEnd = doc["clockShowAtEnd"].as<bool>();
      }
      if (doc.containsKey("barPosition")) {
        barPosition = doc["barPosition"].as<String>();
      }
      if (doc.containsKey("customMessages")) {
        customMessages = doc["customMessages"].as<String>();
      }
      
      bool themeChanged = false;
      if (doc.containsKey("clockTheme")) {
        String newTheme = doc["clockTheme"].as<String>();
        if (newTheme != "dark" && newTheme != "light")
          newTheme = "dark";
        if (clockTheme != newTheme) {
            clockTheme = newTheme;
            themeChanged = true;
        }
      }

      // Save to config file
      saveWiFiConfig(ssid, password);

      // Save device state since clockOnlyScreen may have changed
      saveDeviceState();

      // Force immediate screen update on ANY clock state change
      if (doc.containsKey("clockOnlyScreen") || themeChanged) {
        if (clockOnlyScreen) {
          isShowingClock = true;
          isSlideshow = false;
          isClockOnlyMode = true;
          forceClockRedraw();
        } else if (themeChanged && isShowingClock) {
          forceClockRedraw();
        }
      }

      // Reconfigure NTP with new timezone
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

      // If clock-only mode was turned OFF, automatically start slideshow
      if (wasClockOnlyMode && !clockOnlyScreen) {
        // Load media files if not already loaded
        if (mediaFiles.empty()) {
          loadOrder(currentOrientation);
        }

        // Start slideshow mode
        isSlideshow = true;
        isShowingClock = false;
        isClockOnlyMode = false;
        currentMediaIndex = 0;
        photosSinceLastClock = 0;
        lastSlideTime = millis() - slideInterval; // Force immediate first slide

        // Show first photo immediately
        if (!mediaFiles.empty()) {
          showSlideshow();
        }

        saveDeviceState();
      }
      // Force immediate clock redraw if clock is showing
      else if (isShowingClock) {
        forceClockRedraw();
      }

      server.send(200, "application/json", "{\"ok\":true}");
    } else {
      server.send(400, "application/json",
                  "{\"error\":\"Missing configuration\"}");
    }
  });

  server.on("/api/clock/settings", HTTP_GET, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }
    
    DynamicJsonDocument doc(2048);
    doc["use24Hour"] = use24Hour;
    doc["timezoneOffset"] = gmtOffset_sec;
    doc["timezoneName"] = timezoneName;
    doc["clockDisplayDuration"] = clockDisplayDuration;
    doc["clockPhotoInterval"] = clockPhotoInterval;
    doc["clockOnlyScreen"] = clockOnlyScreen;
    doc["clockEnabled"] = clockEnabled;
    doc["clockShowAtEnd"] = clockShowAtEnd;
    doc["barPosition"] = barPosition;
    doc["customMessages"] = customMessages;
    doc["clockTheme"] = clockTheme;
    
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
  });

  server.on("/api/clock/show", HTTP_POST, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }
    // Show clock permanently (stops slideshow)
    isSlideshow = false; // Stop slideshow
    isShowingClock = true;
    isClockOnlyMode = clockOnlyScreen; // Use saved setting
    clockStartTime = millis();
    lastClockUpdate = millis();
    forceClockRedraw();
    server.send(200, "application/json",
                "{\"ok\":true,\"message\":\"Clock displayed\"}");
  });

  server.on("/api/clock/temp", HTTP_POST, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }
    // Show clock temporarily (keeps slideshow running)
    isShowingClock = true;
    isClockOnlyMode = false; // Force temporary mode (duration limit applies)
    clockStartTime = millis();
    lastClockUpdate = millis();
    photosSinceLastClock = 0; // Reset counter so slideshow resumes properly
    forceClockRedraw();
    server.send(200, "application/json",
                "{\"ok\":true,\"message\":\"Clock displayed temporarily\"}");
  });

  server.on("/api/durations", HTTP_POST, []() {
    if (!checkAuth()) {
      server.send(403, "text/plain", "Unauthorized");
      return;
    }
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(8192);
      deserializeJson(doc, server.arg("plain"));

      if (doc.containsKey("photoOrder") && doc["photoOrder"] == "custom") {
        JsonArray customOrder = doc["customOrder"];
        if (customOrder) {
          DynamicJsonDocument orderDoc(4096);
          JsonArray filesArray = orderDoc.createNestedArray("files");

          for (JsonVariant v : customOrder) {
            String filename = v.as<String>();
            if (!filename.startsWith("/")) {
              filename = "/photos/" + currentOrientation + "/" + filename;
            }
            if (SD.exists(filename)) {
              filesArray.add(filename);
            }
          }

          saveOrderConfig(currentOrientation, orderDoc);
          loadOrder(currentOrientation);
          currentMediaIndex = 0;

          server.send(200, "application/json",
                      "{\"success\":true,\"message\":\"Photo order saved\"}");
          return;
        }
      }

      // Handle clock/photo duration settings (legacy)
      if (doc.containsKey("clockDuration")) {
        clockDisplayDuration =
            doc["clockDuration"].as<int>() / 1000; // Convert ms to seconds
      }
      if (doc.containsKey("photoDuration")) {
        slideInterval = doc["photoDuration"].as<int>();
      }
      if (doc.containsKey("clockEnabled")) {
        clockEnabled = doc["clockEnabled"].as<bool>();
      }
      if (doc.containsKey("photosEnabled")) {
        // Just acknowledge, photos are always enabled
      }

      server.send(200, "application/json", "{\"success\":true}");
    } else {
      server.send(400, "application/json", "{\"error\":\"No data\"}");
    }
  });

  server.on("/setup", HTTP_GET, []() {
    String ssid = server.hasArg("ssid") ? server.arg("ssid") : "";

    server.send(
        200, "text/html",
        "<!DOCTYPE html>"
        "<html><head><title>ChronoSync WiFi Setup</title>"
        "<meta charset='UTF-8'><meta name='viewport' "
        "content='width=device-width, initial-scale=1'>"
        "<style>"
        "body{font-family:Arial,sans-serif;margin:20px;background:#1a1a1a;"
        "color:#fff}"
        "h1{color:#00d4aa;text-align:center}"
        ".container{max-width:500px;margin:0 auto}"
        ".card{background:#2a2a2a;padding:20px;border-radius:8px;margin:10px 0}"
        "input,select{width:100%;padding:10px;margin:5px 0;border:1px solid "
        "#555;border-radius:4px;background:#333;color:#fff}"
        "button{width:100%;padding:12px;margin:10px "
        "0;border:none;border-radius:4px;cursor:pointer;font-size:16px;"
        "background:#00d4aa;color:#000}"
        ".btn-secondary{background:#555;color:#fff}"
        ".network{padding:10px;border:1px solid "
        "#555;border-radius:4px;margin:5px 0;cursor:pointer}"
        ".network:hover{background:#333}"
        "#status{margin:10px 0;padding:10px;border-radius:4px;display:none}"
        ".success{background:#004d40;color:#00e676}"
        ".error{background:#4d0000;color:#ff5252}"
        ".info{background:#003d82;color:#40c4ff}"
        ".qr-info{text-align:center;padding:15px;border:2px dashed "
        "#555;margin:10px 0;border-radius:8px}"
        "</style></head><body>"
        "<div class='container'>"
        "<h1>ChronoSync WiFi Setup</h1>"
        "<div class='qr-info'>"
        "<h3>Quick Connect - Open Network</h3>"
        "<p>Network: <strong>" +
            String(apSSID) +
            "</strong></p>"
            "<p style='color:#00e676'>No password required - Open WiFi</p>"
            "<p>Scan the QR code on the device screen or connect manually</p>"
            "</div>"
            "<div class='card'>"
            "<h3>WiFi Configuration</h3>"
            "<div id='status'></div>"
            "<form id='wifiForm' onsubmit='return connectWiFi()'>"
            "<input type='text' id='ssid' value='" +
            ssid +
            "' placeholder='Network Name (SSID)' required>"
            "<input type='password' id='password' placeholder='WiFi Password'>"
            "<button type='submit'>Connect to WiFi</button>"
            "</form>"
            "<button class='btn-secondary' "
            "onclick='window.location.href=\"/networks\"'>Scan "
            "Networks</button>"
            "</div>"
            "<div class='card'>"
            "<h3>Instructions</h3>"
            "<ol>"
            "<li>Click 'Scan Networks' to see available WiFi networks</li>"
            "<li>Enter your WiFi network name and password</li>"
            "<li>Click 'Connect to WiFi'</li>"
            "<li>Device will restart and you'll be redirected to the main "
            "interface</li>"
            "</ol>"
            "</div></div>"
            "<script>"
            "function connectWiFi(){"
            "var ssid=document.getElementById('ssid').value;"
            "var password=document.getElementById('password').value;"
            "if(!ssid){alert('Please enter network name');return false;}"
            "document.getElementById('status').style.display='block';"
            "document.getElementById('status').innerHTML='Connecting to "
            "'+ssid+'...';"
            "document.getElementById('status').className='info';"
            "var xhr=new XMLHttpRequest();"
            "xhr.open('POST','/api/wifi/connect',true);"
            "xhr.setRequestHeader('Content-Type','application/json');"
            "xhr.onreadystatechange=function(){"
            "if(xhr.readyState==4){"
            "var response=JSON.parse(xhr.responseText);"
            "if(response.success){"
            "document.getElementById('status').innerHTML='Connected! "
            "Redirecting to device interface...';"
            "document.getElementById('status').className='success';"
            "setTimeout(function(){"
            "if(response.deviceIP){"
            "window.location.href='http://'+response.deviceIP;"
            "}else{"
            "window.location.href='http://192.168.1.100';"
            "}"
            "},3000);"
            "}else{"
            "document.getElementById('status').innerHTML='Failed: "
            "'+(response.error||'Unknown error');"
            "document.getElementById('status').className='error';"
            "}}};"
            "xhr.send(JSON.stringify({ssid:ssid,password:password}));"
            "return false;}"
            "</script>"
            "</body></html>");
  });

  // Add favicon.ico 204 handler to prevent browser stalls
  server.on("/favicon.ico", HTTP_GET, []() {
    server.send(204, "text/plain", ""); // No content, prevents 404 stalls
  });

  // Networks list page
  server.on("/networks", HTTP_GET, []() {
    String networks = scanWiFiNetworks();
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, networks);

    String html =
        "<!DOCTYPE html><html><head><title>Available Networks</title>"
        "<meta charset='UTF-8'><meta name='viewport' "
        "content='width=device-width, initial-scale=1'>"
        "<style>body{font-family:Arial,sans-serif;margin:20px;background:#"
        "1a1a1a;color:#fff}"
        "h1{color:#00d4aa}.network{padding:10px;border:1px solid "
        "#555;border-radius:4px;margin:5px 0;cursor:pointer;background:#2a2a2a}"
        ".network:hover{background:#333}.signal{float:right}</style></"
        "head><body>"
        "<h1>Available Networks</h1>";

    if (doc["count"].as<int>() == 0) {
      html += "<p>No networks found</p>";
    } else {
      JsonArray networksArray = doc["networks"];
      for (JsonObject network : networksArray) {
        String ssid = network["ssid"];
        int rssi = network["rssi"];
        int encryption = network["encryption"];
        String security = encryption == 0 ? "Open" : "Secured";
        String signal = rssi > -50 ? "Strong" : rssi > -70 ? "Good" : "Weak";

        html +=
            "<div class='network' onclick='selectNetwork(\"" + ssid + "\")'>";
        html += security + " - " + ssid + " <span class='signal'>" + signal +
                " (" + String(rssi) + "dBm)</span>";
        html += "</div>";
      }
    }

    html += "<br><button onclick='window.location.href=\"/setup\"'>Back to "
            "Setup</button>";
    html += "<script>function "
            "selectNetwork(ssid){window.location.href='/"
            "setup?ssid='+encodeURIComponent(ssid);}</script>";
    html += "</body></html>";

    server.send(200, "text/html", html);
  });

  // WiFi scan endpoint
  server.on("/api/scan", HTTP_GET, []() {
    String json = scanWiFiNetworks();
    server.send(200, "application/json", json);
  });

  // QR Code data endpoint for WiFi connection
  server.on("/api/qr", HTTP_GET, []() {
    // Generate WiFi QR code data string
    String qrData = "WIFI:T:nopass;S:" + String(apSSID) + ";P:;H:false;;";
    String json =
        "{\"qrData\":\"" + qrData + "\",\"ssid\":\"" + String(apSSID) + "\"}";
    server.send(200, "application/json", json);
  });

  // WiFi connection endpoint for AP mode
  server.on("/api/wifi/connect", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(512);
      deserializeJson(doc, server.arg("plain"));

      String newSSID = doc["ssid"].as<String>();
      String newPassword = doc["password"].as<String>();

      if (newSSID.length() == 0) {
        server.send(400, "application/json",
                    "{\"success\":false,\"error\":\"SSID required\"}");
        return;
      }

      // Send immediate response
      server.send(200, "application/json",
                  "{\"success\":true,\"message\":\"Connecting...\"}");

      // Small delay to let response send
      delay(100);

      // Attempt connection
      if (scanAndConnectToWiFi(newSSID, newPassword)) {
        // Connection successful - device will now be on the new network
        // The device has already been configured and is ready to use
        Serial.println("WiFi connection successful - device ready");
      }
      // If failed, scanAndConnectToWiFi already handles returning to AP mode
    } else {
      server.send(400, "application/json",
                  "{\"success\":false,\"error\":\"Invalid request\"}");
    }
  });

  server.on("/ncsi.txt", HTTP_GET, []() {
    server.sendHeader("Location", "http://192.168.4.1/setup", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  server.enableCORS(true);

  // === START mDNS for easy discovery (works across 5GHz/2.4GHz isolation) ===
  if (!isAPMode) {
    if (MDNS.begin("chronosync")) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("mDNS responder started!");
      Serial.println("Access device at: http://chronosync.local");
      Serial.printf("Or via IP: http://%s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("mDNS failed to start");
      Serial.printf("Access device via IP: http://%s\n", WiFi.localIP().toString().c_str());
    }
    // Optimization for 5G modems: Disable sleep for lower latency
    WiFi.setSleep(false);
  }

  loadOrder(currentOrientation);

  // --- RESTORE LAST DEVICE STATE ---
  loadDeviceState();
  loadPIN();
  loadAuthState(); // Load saved authentication session

  // Restore the display to the last state if not in AP mode
  if (!isAPMode && sdReady) {
    // Set the correct orientation
    setHardwareRotation(currentOrientation);

    if (clockOnlyScreen || isShowingClock) {
      // Restore clock display
      isShowingClock = true;
      isClockOnlyMode = clockOnlyScreen;
      clockStartTime = millis();
      lastClockUpdate = millis();
      forceClockRedraw();
      Serial.println("Restored to clock display");
    } else if (!isSlideshow && currentFile.length() > 0) {
      // Restore to specific file (GIF/video) - check if file exists first
      if (!SD.exists(currentFile)) {
        Serial.printf(
            "Restore failed: file not found: %s - switching to slideshow\n",
            currentFile.c_str());
        currentFile = "";
        isSlideshow = true;
        lastSlideTime = millis() - slideInterval;
      } else if (currentFile.endsWith(".vid") ||
                 currentFile.endsWith(".mjpeg")) {
        Serial.printf("Restored to video: %s\\n", currentFile.c_str());
        // Video will be handled in loop()
      } else if (currentFile.endsWith(".jpg") ||
                 currentFile.endsWith(".jpeg")) {
        tft.fillScreen(TFT_BLACK);
        tft.setSwapBytes(true);
        TJpgDec.drawSdJpg(0, 0, currentFile);
        Serial.printf("Restored to image: %s\\n", currentFile.c_str());
      }
    } else if (isSlideshow) {
      // Restore to slideshow mode - will automatically continue in loop()
      lastSlideTime = millis() - slideInterval; // Trigger immediate slide
      Serial.println("Restored to slideshow mode");
    }
  }
}

void loop() {
  // === INDUSTRIAL STABILITY: Feed Watchdog ===
  esp_task_wdt_reset();

  server.handleClient();
  yield(); // Allow ESP32 to handle background tasks

  // === SD CARD MONITORING: Check for card removal ===
  if (millis() - lastSDCheck > SD_CHECK_INTERVAL) {
    checkSDCardStatus();
    lastSDCheck = millis();
  }

  // === INDUSTRIAL STABILITY: Heap Health Monitoring ===
  if (millis() - lastHeapCheck > HEAP_CHECK_INTERVAL) {
    checkHeapHealth();
    lastHeapCheck = millis();
  }

  // === INDUSTRIAL STABILITY: Scheduled Maintenance Reboot ===
  if (millis() - lastMaintenanceCheck > MAINTENANCE_CHECK_INTERVAL) {
    checkScheduledMaintenance();
    updateBacklightForTimeOfDay(); // Also adjust brightness based on time
    lastMaintenanceCheck = millis();
  }

  // Handle touch input (all gestures)
  handleTouch();

  // === EMERGENCY RECOVERY: 5s BOOT Button Hold ===
  static unsigned long bootPressStart = 0;
  if (digitalRead(BOOT_BUTTON) == LOW) {
    if (bootPressStart == 0) {
      bootPressStart = millis();
    } else if (millis() - bootPressStart > 5000) {
      Serial.println("[EMERGENCY] 5s BOOT Hold - Manual Safe Reset triggered");
      bootPressStart = 0;

      // Forced Safe State
      currentOrientation = "landscape";
      isSlideshow = true;
      isShowingClock = false;
      isClockOnlyMode = false;
      clockOnlyScreen = false;
      currentFile = "";
      tft.setRotation(1);
      
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_ORANGE, TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("SYSTEM RESET - OK", 240, 160, 4);
      delay(2000);
      
      saveDeviceState();
      ESP.restart();
    }
  } else {
    bootPressStart = 0;
  }

  // If in AP mode, only handle web server and keep showing setup screen
  if (isAPMode) {
    dnsServer.processNextRequest(); // Process DNS requests for Captive Portal

    // Refresh AP setup screen every 30 seconds to keep it visible
    if (millis() - lastAPScreenUpdate > 30000) {
      showAPSetupScreen();
      lastAPScreenUpdate = millis();
    }
    return; // Don't proceed with normal operations
  }

  // Draw status bar overlay (always visible on top)
  drawStatusBar();

  // If showing clock, update every minute
  if (isShowingClock) {
    // Update clock display every minute
    if (millis() - lastClockUpdate > 60000) {
      updateClock();
      lastClockUpdate = millis();
    }

    // Only auto-hide if NOT in clock-only mode and in slideshow mode
    if (!isClockOnlyMode && isSlideshow) {
      if (millis() - clockStartTime > (clockDisplayDuration * 1000)) {
        isShowingClock = false;
        lastSlideTime = millis() - slideInterval;
        // Don't save state during automatic slideshow transitions
      }
    }
    return;
  }

  // If clock only mode is enabled, keep showing clock and prevent slideshow
  if (clockOnlyScreen) {
    if (!isShowingClock) {
      isShowingClock = true;
      isSlideshow = false;
      isClockOnlyMode = true;
      forceClockRedraw();
    }
    return; // Don't run slideshow or play videos when clock only mode is on
  }

  if (!isSlideshow &&
      (currentFile.endsWith(".vid") || currentFile.endsWith(".mjpeg"))) {
    if (currentFile.endsWith(".mjpeg")) {
      playMJPEG(currentFile, true);
    } else {
      playRawVideo(currentFile, true);
    }
  } else if (isSlideshow) {
    if (millis() - lastSlideTime > slideInterval) {
      if (clockEnabled) {
        bool shouldShowClock = false;

        if (clockShowAtEnd || clockPhotoInterval == 0) {
          if (photosSinceLastClock >= (int)mediaFiles.size()) {
            shouldShowClock = true;
          }
        } else if (clockPhotoInterval > 0) {
          if (photosSinceLastClock >= clockPhotoInterval) {
            shouldShowClock = true;
          }
        }

        if (shouldShowClock) {
          isShowingClock = true;
          clockStartTime = millis();
          lastClockUpdate = millis();
          photosSinceLastClock = 0;
          forceClockRedraw();
          // Don't save state during automatic slideshow transitions
        } else {
          showSlideshow();
          photosSinceLastClock++;
        }
      } else {
        showSlideshow();
      }
      lastSlideTime = millis();
    }
  }
}

void setupWiFiWithFallback() {
  Serial.println("=== WiFi Setup ===");

  // Check if we have saved WiFi credentials
  if (ssid.length() == 0) {
    Serial.println("No WiFi credentials found - starting AP mode");
    startAPMode();
    return;
  }

  Serial.printf("Attempting to connect to: %s\n", ssid.c_str());
  drawSystemText("Connecting to WiFi...", 5, 5, TFT_CYAN, 2);
  drawSystemText(ssid.c_str(), 5, 25, TFT_WHITE, 2);

  // Try to connect to saved WiFi
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), password.c_str());

  wifiConnectStartTime = millis();
  bool connected = false;

  // Wait for connection with timeout
  while (WiFi.status() != WL_CONNECTED &&
         millis() - wifiConnectStartTime < WIFI_CONNECT_TIMEOUT) {
    esp_task_wdt_reset(); // Feed watchdog during WiFi connection
    delay(500);
    Serial.print(".");

    // Update display with dots
    static int dotCount = 0;
    String dots = "";
    for (int i = 0; i <= dotCount; i++) {
      dots += ".";
    }
    drawSystemText(("Connecting" + dots + "       ").c_str(), 5, 45, TFT_CYAN,
                   2);
    dotCount = (dotCount + 1) % 4;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    connected = true;
    isAPMode = false;
    String ipAddress = WiFi.localIP().toString();
    Serial.printf("Connected! IP: %s\n", ipAddress.c_str());

    drawSystemText("WiFi Connected!", 5, 65, TFT_GREEN, 2);
    drawSystemText(ipAddress.c_str(), 5, 85, TFT_GREEN, 2);

    // Sync time
    drawSystemText("Syncing time...", 5, 105, TFT_CYAN, 2);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    struct tm timeinfo;
    int syncAttempts = 0;
    while (!getLocalTime(&timeinfo) && syncAttempts < 10) {
      esp_task_wdt_reset(); // Feed watchdog during time sync
      delay(500);
      syncAttempts++;
    }

    if (getLocalTime(&timeinfo)) {
      drawSystemText("Time synced!", 5, 125, TFT_GREEN, 2);
    } else {
      drawSystemText("Time sync failed", 5, 125, TFT_YELLOW, 2);
    }

    // Show mDNS hostname for easy access
    drawSystemText("Access via:", 5, 145, TFT_CYAN, 2);
    drawSystemText("http://chronosync.local", 5, 165, TFT_YELLOW, 2);

    delay(3000);
  } else {
    Serial.println("WiFi connection failed - starting AP mode");
    drawSystemText("WiFi Connection Failed!", 5, 65, TFT_RED, 2);
    drawSystemText("Starting setup mode...", 5, 85, TFT_YELLOW, 2);
    delay(2000);
    startAPMode();
  }
}

void startAPMode() {
  Serial.println("Starting Access Point mode");
  isAPMode = true;
  apModeActive = true;
  lastAPScreenUpdate = millis();

  // Start AP with better configuration
  WiFi.mode(WIFI_AP);
  WiFi.softAPdisconnect(true);
  delay(100);

  // Create open AP (no password) for easier connection
  bool apStarted = WiFi.softAP(
      apSSID, "", 1, 0, 4); // Open network, channel 1, hidden=0, max_clients=4

  if (apStarted) {
    Serial.println("AP Started Successfully");
    // Start DNS server for captive portal (port 53, redirect all to AP IP)
    dnsServer.start(53, "*", WiFi.softAPIP());
    Serial.println("DNS Server started for Captive Portal");
  } else {
    Serial.println("AP Start Failed");
  }

  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("AP IP address: %s\n", apIP.toString().c_str());

  showAPSetupScreen();
}

void showAPSetupScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(false);

  // Clean header
  tft.fillRect(0, 0, 480, 55, 0x0020); // very dark blue
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, 0x0020);
  tft.setTextSize(1);
  tft.drawString("ChronoSync - WiFi Setup", 240, 14, 4);
  tft.setTextColor(TFT_CYAN, 0x0020);
  tft.drawString("Connect to configure your device", 240, 38, 2);

  // Left panel - Network info
  int panelY = 70;
  tft.setTextDatum(ML_DATUM);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("STEP 1: Join WiFi Network", 20, panelY, 2);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Network:", 20, panelY + 22, 2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(apSSID, 100, panelY + 22, 2);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("No password required", 20, panelY + 42, 2);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("STEP 2: Open Browser", 20, panelY + 72, 2);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Address:", 20, panelY + 94, 2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("192.168.4.1", 100, panelY + 94, 4);

  // Right panel - QR Code in a white box for scanning
  int qrX = 280, qrY = 80;
  int qrBoxSize = 190;
  tft.fillRect(qrX - 5, qrY - 5, qrBoxSize, qrBoxSize, TFT_WHITE);
  drawWiFiQRCode(qrX + 2, qrY + 2);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("or scan to connect", 375, qrY + qrBoxSize + 5, 2);

  tft.setSwapBytes(true);
}

void drawWiFiQRCode(int x, int y) {
  tft.setSwapBytes(false);

  // Use the real QR code generated by Python (from wifi_qr_data.h)
  // QR_SIZE and qrCodeMatrix are defined in the header file

  int moduleSize = 4; // Size of each QR module in pixels
  int quietZone = 8;  // White border around QR code
  int totalSize = QR_SIZE * moduleSize + (quietZone * 2);

  // Draw white background with quiet zone
  tft.fillRect(x - quietZone, y - quietZone, totalSize, totalSize, TFT_WHITE);

  // Draw black border
  tft.drawRect(x - quietZone - 1, y - quietZone - 1, totalSize + 2,
               totalSize + 2, TFT_BLACK);

  // Draw the QR code modules - BLACK on WHITE
  for (int i = 0; i < QR_SIZE; i++) {
    for (int j = 0; j < QR_SIZE; j++) {
      if (qrCodeMatrix[i][j]) { // true = black module
        tft.fillRect(x + j * moduleSize, y + i * moduleSize, moduleSize,
                     moduleSize, TFT_BLACK);
      }
    }
  }

  tft.setSwapBytes(true);
}

bool scanAndConnectToWiFi(String newSSID, String newPassword) {
  Serial.printf("Attempting to connect to new WiFi: %s\n", newSSID.c_str());

  // Show connecting status on screen
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(false);
  drawSystemText("Connecting to WiFi...", 10, 50, TFT_CYAN, 2);
  drawSystemText(newSSID.c_str(), 10, 80, TFT_WHITE, 2);
  tft.setSwapBytes(true);

  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(newSSID.c_str(), newPassword.c_str());

  unsigned long startTime = millis();
  int dots = 0;

  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
    delay(500);
    Serial.print(".");

    // Update connecting display
    tft.setSwapBytes(false);
    String connectMsg = "Connecting";
    for (int i = 0; i <= dots % 4; i++) {
      connectMsg += ".";
    }
    for (int i = connectMsg.length(); i < 15; i++) {
      connectMsg += " ";
    }
    drawSystemText(connectMsg.c_str(), 10, 110, TFT_YELLOW, 2);
    tft.setSwapBytes(true);
    dots++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Successfully connected to %s\n", newSSID.c_str());

    // Save credentials
    saveWiFiConfig(newSSID, newPassword);
    ssid = newSSID;
    password = newPassword;
    isAPMode = false;
    apModeActive = false;

    // Show success message
    tft.fillScreen(TFT_BLACK);
    tft.setSwapBytes(false);
    drawSystemText("WiFi Connected!", 10, 50, TFT_GREEN, 3);
    String ipText = "IP: " + WiFi.localIP().toString();
    drawSystemText(ipText.c_str(), 10, 90, TFT_GREEN, 2);
    drawSystemText("Configuring time...", 10, 120, TFT_CYAN, 2);
    tft.setSwapBytes(true);

    // Configure NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    int syncAttempts = 0;
    while (!getLocalTime(&timeinfo) && syncAttempts < 10) {
      esp_task_wdt_reset(); // Feed watchdog during time sync
      delay(500);
      syncAttempts++;
    }

    tft.setSwapBytes(false);
    if (getLocalTime(&timeinfo)) {
      drawSystemText("Time synced!", 10, 150, TFT_GREEN, 2);
    } else {
      drawSystemText("Time sync failed", 10, 150, TFT_YELLOW, 2);
    }
    drawSystemText("Starting device...", 10, 180, TFT_CYAN, 2);
    tft.setSwapBytes(true);

    // Re-initialize mDNS on new connection
    if (MDNS.begin("chronosync")) {
        MDNS.addService("http", "tcp", 80);
        Serial.println("mDNS responder re-started!");
    }
    WiFi.setSleep(false); // Ensure low latency

    delay(3000);
    return true;
  } else {
    Serial.printf("Failed to connect to %s\n", newSSID.c_str());

    // Show error and return to AP mode
    tft.fillScreen(TFT_BLACK);
    tft.setSwapBytes(false);
    drawSystemText("Connection Failed!", 10, 50, TFT_RED, 2);
    drawSystemText("Returning to setup...", 10, 80, TFT_YELLOW, 2);
    tft.setSwapBytes(true);
    delay(2000);

    startAPMode();
    return false;
  }
}

String scanWiFiNetworks() {
  Serial.println("Scanning for WiFi networks...");

  // Scan for networks
  int networkCount = WiFi.scanNetworks();

  String json = "{\"networks\":[";

  if (networkCount == 0) {
    json += "],\"count\":0}";
  } else {
    for (int i = 0; i < networkCount; ++i) {
      if (i > 0)
        json += ",";

      json += "{";
      json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
      json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
      json += "\"encryption\":" + String(WiFi.encryptionType(i));
      json += "}";
    }
    json += "],\"count\":" + String(networkCount) + "}";
  }

  Serial.printf("Found %d networks\n", networkCount);
  return json;
}

void loadWiFiConfig() {
  // Load WiFi credentials from NVS (permanent storage)
  preferences.begin("chronosync", true); // Read-only
  ssid = preferences.getString("wifi_ssid", "");
  password = preferences.getString("wifi_pass", "");
  preferences.end();

  // Load other settings from SD card if available
  if (SD.exists("/config/wifi.json")) {
    File f = SD.open("/config/wifi.json");
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, f);

    // Load clock settings (these can stay on SD card)
    use24Hour = doc["use24Hour"] | false;
    gmtOffset_sec = doc["gmtOffset"] | (8 * 3600);
    timezoneName = doc["timezoneName"] | "UTC+8 Manila";
    clockDisplayDuration = doc["clockDisplayDuration"] | 30;
    clockPhotoInterval = doc["clockPhotoInterval"] | 0;
    clockOnlyScreen = doc["clockOnlyScreen"] | false;
    clockEnabled = doc["clockEnabled"] | true;        // Default on
    clockShowAtEnd =
        doc["clockShowAtEnd"] | true; // Default on (show at end of slideshow)
    clockTheme = doc["clockTheme"] | "dark";
    barPosition = doc["barPosition"] | "top";
    customMessages = doc["customMessages"] | "Creating memories...\\nLife is beautiful!\\nEnjoy the moments!\\nYou're amazing!\\nNever give up!";

    // Sync state flags if clock only mode is active
    if (clockOnlyScreen) {
        isClockOnlyMode = true;
        isSlideshow = false;
        isShowingClock = true;
    }

    f.close();
  }
}

void saveWiFiConfig(String s, String p) {
  // Save WiFi credentials to NVS (permanent storage in ESP32 flash)
  preferences.begin("chronosync", false); // Read-write
  preferences.putString("wifi_ssid", s);
  preferences.putString("wifi_pass", p);
  preferences.end();

  Serial.printf("WiFi credentials saved to NVS: %s\n", s.c_str());

  // Also save all settings to SD card if available (backup + other settings)
  if (sdReady) {
    File f = SD.open("/config/wifi.json", FILE_WRITE);
    if (f) {
      DynamicJsonDocument doc(2048);
      doc["ssid"] = s;
      doc["pass"] = p;
      doc["use24Hour"] = use24Hour;
      doc["gmtOffset"] = gmtOffset_sec;
      doc["timezoneName"] = timezoneName;
      doc["clockDisplayDuration"] = clockDisplayDuration;
      doc["clockPhotoInterval"] = clockPhotoInterval;
      doc["clockOnlyScreen"] = clockOnlyScreen;
      doc["clockEnabled"] = clockEnabled;
      doc["clockShowAtEnd"] = clockShowAtEnd;
      doc["barPosition"] = barPosition;
      doc["customMessages"] = customMessages;
      doc["clockTheme"] = clockTheme;
      serializeJson(doc, f);
      f.close();
      Serial.println("WiFi config also backed up to SD card");
    }
  }
}

void setHardwareRotation(String orientation) {
  // The ST7796 is 320x480. Rotation 0 is portrait (320 wide), Rotation 1 is
  // landscape (480 wide)
  if (orientation == "portrait")
    tft.setRotation(0);
  else
    tft.setRotation(1);
}

void saveDeviceState() {
  preferences.begin("chronosync", false);

  // Save display state
  preferences.putBool("isSlideshow", isSlideshow);
  preferences.putBool("isShowingClk", isShowingClock);
  preferences.putBool("isClkOnlyMd", isClockOnlyMode);
  preferences.putBool("clkOnlyScr", clockOnlyScreen);
  preferences.putInt("slideIntrvl", slideInterval);

  // Save current media information
  preferences.putString("currFile", currentFile);
  preferences.putInt("mediaIdx", currentMediaIndex);
  preferences.putString("orientation", currentOrientation);
  preferences.putString("clockTheme", clockTheme);

  preferences.end();

  Serial.println("Device state saved");
}

void loadDeviceState() {
  preferences.begin("chronosync", true);

  isSlideshow = preferences.getBool("isSlideshow", true);
  isShowingClock = preferences.getBool("isShowingClk", false);
  isClockOnlyMode = preferences.getBool("isClkOnlyMd", false);
  clockOnlyScreen = preferences.getBool("clkOnlyScr", false);
  slideInterval = preferences.getInt("slideIntrvl", 5000);

  currentFile = preferences.getString("currFile", "");
  currentMediaIndex = preferences.getInt("mediaIdx", 0);
  currentOrientation = preferences.getString("orientation", "landscape");
  clockTheme = preferences.getString("clockTheme", "dark");

  preferences.end();

  // If clockOnlyScreen is on, ensure display flags are synced
  if (clockOnlyScreen) {
    isSlideshow = false;
    isClockOnlyMode = true;
    isShowingClock = true;
  }
  // If saved file no longer exists on SD, fall back to slideshow
  if (currentFile.length() > 0 && sdReady && !SD.exists(currentFile)) {
    Serial.printf("[STATE] Saved file not found: %s — resetting to slideshow\n",
                  currentFile.c_str());
    currentFile = "";
    isSlideshow = true;
  }

  Serial.println("Device state loaded");
  Serial.printf("  isSlideshow: %d, isShowingClock: %d, clockOnlyScreen: %d\n",
                isSlideshow, isShowingClock, clockOnlyScreen);
  Serial.printf("  currentFile: %s, currentMediaIndex: %d\n",
                currentFile.c_str(), currentMediaIndex);
}

void savePIN() {
  esp_task_wdt_reset();
  if (!sdReady)
    return;
  File file = SD.open("/config/pin.txt", FILE_WRITE);
  if (file) {
    file.println(devicePIN);
    file.close();
  }
  esp_task_wdt_reset();
}

void loadPIN() {
  esp_task_wdt_reset();
  // Try loading from NVS first (preferences)
  preferences.begin("chronosync", true); // Read-only
  esp_task_wdt_reset();
  if (preferences.isKey("device_pin")) {
    devicePIN = preferences.getString("device_pin", "1234");
    pinChangeRequired = preferences.getBool("pin_change_required", false);
    preferences.end();
    esp_task_wdt_reset();
    Serial.printf("[PIN] Loaded from NVS: %s (changeRequired=%d)\n",
                  devicePIN.c_str(), pinChangeRequired);

    // Also save to SD card for backup
    if (sdReady) {
      if (!SD.exists("/config"))
        SD.mkdir("/config");
      File file = SD.open("/config/pin.txt", FILE_WRITE);
      if (file) {
        file.println(devicePIN);
        file.close();
      }
    }
    return;
  }
  preferences.end();
  esp_task_wdt_reset();

  // Fallback: Try loading from SD card
  if (!sdReady) {
    devicePIN = "1234";
    pinChangeRequired = true;
    return;
  }

  esp_task_wdt_reset();
  File file = SD.open("/config/pin.txt", FILE_READ);
  if (file) {
    devicePIN = file.readStringUntil('\n');
    devicePIN.trim();
    file.close();
    esp_task_wdt_reset();

    // Save to NVS for next time
    preferences.begin("chronosync", false);
    esp_task_wdt_reset();
    preferences.putString("device_pin", devicePIN);
    preferences.end();
    esp_task_wdt_reset();
  } else {
    devicePIN = "1234";
    pinChangeRequired = true;

    // Save default to NVS
    esp_task_wdt_reset();
    preferences.begin("chronosync", false);
    esp_task_wdt_reset();
    preferences.putString("device_pin", devicePIN);
    preferences.putBool("pin_change_required", true);
    preferences.end();
    esp_task_wdt_reset();
  }

  Serial.printf("[PIN] Loaded: %s (changeRequired=%d)\n", devicePIN.c_str(),
                pinChangeRequired);
}

// Save authentication state to Preferences (non-volatile storage)
void saveAuthState() {
  esp_task_wdt_reset(); // Reset watchdog before NVS operations
  preferences.begin("chronosync", false);
  esp_task_wdt_reset();
  preferences.putBool("isAuth", isAuthenticated);
  preferences.putUInt("authTime", (uint32_t)authTimestamp);
  esp_task_wdt_reset();
  preferences.end();
  esp_task_wdt_reset();
  Serial.printf("[AUTH] Saved state: authenticated=%d, timestamp=%lu\n",
                isAuthenticated, (unsigned long)authTimestamp);
}

// Load authentication state from Preferences
void loadAuthState() {
  esp_task_wdt_reset();
  preferences.begin("chronosync", true);
  esp_task_wdt_reset();
  isAuthenticated = preferences.getBool("isAuth", false);
  authTimestamp = (time_t)preferences.getUInt("authTime", 0);
  preferences.end();
  esp_task_wdt_reset();

  if (isAuthenticated) {
    Serial.printf("[AUTH] Loaded saved session from %lu\n",
                  (unsigned long)authTimestamp);
    // Check if it's still valid
    checkAuth();
  } else {
    Serial.println("[AUTH] No saved session found");
  }
}

// Auth removed - any user who knows the IP address has access
bool checkAuth() { return true; }

bool readTouch(uint16_t *x, uint16_t *y) {
  static unsigned long lastPrint = 0;
  bool shouldPrint = (millis() - lastPrint > 500);

  // Check IRQ pin first - it goes LOW when touched
  bool irqActive = (digitalRead(TOUCH_IRQ) == LOW);

  if (shouldPrint) {
    Serial.printf("Touch check: IRQ=%s\n", irqActive ? "ACTIVE" : "idle");
    lastPrint = millis();
  }

  // Only read touch data if IRQ indicates touch
  if (!irqActive) {
    return false;
  }

  Serial.println(">>> Touch IRQ detected, attempting SPI read...");

  // Try a very simple single read first to test SPI communication
  digitalWrite(TOUCH_CS, LOW);
  delayMicroseconds(100); // Longer delay to ensure chip select

  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));

  // Read X position - single sample first to test
  SPI.transfer(0x90); // X position command
  delayMicroseconds(100);
  uint8_t x_msb = SPI.transfer(0x00);
  delayMicroseconds(10);
  uint8_t x_lsb = SPI.transfer(0x00);

  Serial.printf(">>> X read: MSB=0x%02X, LSB=0x%02X\n", x_msb, x_lsb);

  // Read Y position
  delayMicroseconds(50);
  SPI.transfer(0xD0); // Y position command
  delayMicroseconds(100);
  uint8_t y_msb = SPI.transfer(0x00);
  delayMicroseconds(10);
  uint8_t y_lsb = SPI.transfer(0x00);

  Serial.printf(">>> Y read: MSB=0x%02X, LSB=0x%02X\n", y_msb, y_lsb);

  // Read Z1
  delayMicroseconds(50);
  SPI.transfer(0xB0); // Z1 command
  delayMicroseconds(100);
  uint8_t z1_msb = SPI.transfer(0x00);
  delayMicroseconds(10);
  uint8_t z1_lsb = SPI.transfer(0x00);

  Serial.printf(">>> Z1 read: MSB=0x%02X, LSB=0x%02X\n", z1_msb, z1_lsb);

  // Read Z2
  delayMicroseconds(50);
  SPI.transfer(0xC0); // Z2 command
  delayMicroseconds(100);
  uint8_t z2_msb = SPI.transfer(0x00);
  delayMicroseconds(10);
  uint8_t z2_lsb = SPI.transfer(0x00);

  Serial.printf(">>> Z2 read: MSB=0x%02X, LSB=0x%02X\n", z2_msb, z2_lsb);

  // Power down
  SPI.transfer(0x00);

  SPI.endTransaction();
  digitalWrite(TOUCH_CS, HIGH);

  // Calculate values
  uint16_t rawX = (((uint16_t)x_msb << 8) | x_lsb) >> 3;
  uint16_t rawY = (((uint16_t)y_msb << 8) | y_lsb) >> 3;
  uint16_t z1 = (((uint16_t)z1_msb << 8) | z1_lsb) >> 3;
  uint16_t z2 = (((uint16_t)z2_msb << 8) | z2_lsb) >> 3;

  // For XPT2046: pressure = Z2 - Z1 (reversed from typical)
  // Valid touch: Z1 should be > 100, and pressure should be positive
  int pressure = z2 - z1;

  // Debug output
  Serial.printf("Touch read: rawX=%d, rawY=%d, Z1=%d, Z2=%d, Pressure=%d\n",
                rawX, rawY, z1, z2, pressure);

  // Validate touch - Z1 must be present and pressure reasonable
  if (z1 < 100 || pressure < 200 || pressure > 3500) {
    Serial.printf("Invalid pressure - Z1 too low or pressure out of range "
                  "(Z1=%d, P=%d)\n",
                  z1, pressure);
    return false;
  }

  if (rawX < 100 || rawX > 4000 || rawY < 100 || rawY > 4000) {
    Serial.println("Invalid coordinates - ignoring");
    return false;
  }

  // Calibrate coordinates based on current screen orientation
  // Map from raw ADC values to screen coordinates
  int screenWidth = tft.width();
  int screenHeight = tft.height();

  // Portrait mode (320x480) vs Landscape mode (480x320)
  if (currentOrientation == "portrait") {
    // In portrait: swap X/Y and adjust mapping
    *x = map(rawY, 300, 3800, 0, screenWidth);
    *y = map(rawX, 300, 3800, screenHeight, 0); // Inverted Y
  } else {
    // In landscape: normal mapping
    *x = map(rawX, 300, 3800, 0, screenWidth);
    *y = map(rawY, 300, 3800, 0, screenHeight);
  }

  // Constrain to screen bounds
  *x = constrain(*x, 0, screenWidth - 1);
  *y = constrain(*y, 0, screenHeight - 1);

  Serial.printf("Mapped to screen: x=%d, y=%d\n", *x, *y);

  return true;
}

// Show WiFi QR code overlay
void showWiFiQROverlay() {
  showingWiFiQR = true;
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(false);

  // Title
  tft.setTextColor(TAILWIND_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("WiFi Setup - Scan QR Code");

  // Draw QR code centered
  int qrX = (tft.width() - 200) / 2;
  int qrY = 60;
  drawWiFiQRCode(qrX, qrY);

  // Instructions
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 280);
  tft.println("Tap anywhere to close");

  tft.setSwapBytes(true);
}

// Hide WiFi QR code overlay
void hideWiFiQROverlay() {
  showingWiFiQR = false;
  tft.fillScreen(TFT_BLACK);
  // Redraw current screen
  if (isShowingClock) {
    forceClockRedraw();
  } else if (!mediaFiles.empty() && currentMediaIndex < mediaFiles.size()) {
    String f = mediaFiles[currentMediaIndex];
    if (f.endsWith(".jpg") || f.endsWith(".jpeg")) {
      tft.setSwapBytes(true);
      TJpgDec.drawSdJpg(0, 0, f);
    }
  }
}

// Enter GIF preview/selection mode
void enterGifPreviewMode() {
  Serial.println("[GIF] Entering GIF preview mode...");
  uint32_t heapBefore = ESP.getFreeHeap();

  gifPreviewMode = true;
  gifFiles.clear();
  gifFiles.shrink_to_fit(); // Release old memory
  currentGifPreviewIndex = 0;

  // Scan for GIF files in current orientation
  String gifPath = "/gifs/" + currentOrientation;
  std::vector<String> allFiles;
  scanDirectory(gifPath, allFiles);

  // Filter to only include JPG/JPEG cover images (not .vid files)
  for (const String &file : allFiles) {
    if (file.endsWith(".jpg") || file.endsWith(".jpeg")) {
      gifFiles.push_back(file);
    }
  }

  // Clean up temp vector
  allFiles.clear();
  allFiles.shrink_to_fit();

  uint32_t heapAfter = ESP.getFreeHeap();
  Serial.printf("[GIF] Found %d GIF covers | Heap: %u -> %u bytes\n",
                gifFiles.size(), heapBefore, heapAfter);

  if (gifFiles.empty()) {
    // No GIFs found, show message and exit
    tft.fillScreen(TFT_BLACK);
    tft.setSwapBytes(false);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(80, 140);
    tft.println("No GIFs found!");
    tft.setTextSize(1);
    tft.setCursor(60, 180);
    tft.println("Tap to continue...");
    delay(1500);
    gifPreviewMode = false;
    gifFiles.shrink_to_fit();
    return;
  }

  // Show first GIF preview
  showGifPreview(0);
}

// Exit GIF preview mode
void exitGifPreviewMode() {
  Serial.println("[GIF] Exiting GIF preview mode");
  gifPreviewMode = false;
  gifFiles.clear();
  gifFiles.shrink_to_fit(); // Free memory
}

// Show GIF preview at given index
void showGifPreview(int index) {
  if (index < 0 || index >= (int)gifFiles.size())
    return;

  currentGifPreviewIndex = index;
  String gifFile = gifFiles[index];

  // Clear screen
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);

  // Load and show first frame of GIF as preview
  File gifFileHandle = SD.open(gifFile);
  if (gifFileHandle) {
    // For JPG/JPEG (static images that might be in gifs folder)
    if (gifFile.endsWith(".jpg") || gifFile.endsWith(".jpeg")) {
      TJpgDec.drawSdJpg(0, 0, gifFile); // Full screen, no offset
    }
    // For .vid files, just show black screen (no placeholder text)
    gifFileHandle.close();
  }
}

// Navigate GIF preview (direction: -1 = previous, +1 = next)
void navigateGifPreview(int direction) {
  if (!gifPreviewMode || gifFiles.empty())
    return;

  currentGifPreviewIndex += direction;

  // Wrap around
  if (currentGifPreviewIndex < 0) {
    currentGifPreviewIndex = gifFiles.size() - 1;
  } else if (currentGifPreviewIndex >= (int)gifFiles.size()) {
    currentGifPreviewIndex = 0;
  }

  showGifPreview(currentGifPreviewIndex);
}

// Main touch handler - processes all touch gestures
void handleTouch() {
  if (!touchEnabled)
    return;

  uint16_t x, y;
  bool touching = readTouch(&x, &y);

  // === SIMPLIFIED TOUCH: Only 5-second long press to show IP ===
  static bool ipDisplayActive = false;
  static unsigned long ipDisplayStart = 0;

  // If IP is currently showing, wait for 20s then restore
  if (ipDisplayActive) {
    if (millis() - ipDisplayStart >= 20000) {
      Serial.println("[TOUCH] IP display timeout - restoring screen");
      ipDisplayActive = false;

      // Restore screen to previous state
      if (isShowingClock || isClockOnlyMode || clockOnlyScreen) {
        forceClockRedraw();
      } else if (!mediaFiles.empty()) {
        // Redraw current media
        if (!currentFile.isEmpty()) {
          if (currentFile.endsWith(".jpg") || currentFile.endsWith(".jpeg")) {
            tft.fillScreen(TFT_BLACK);
            tft.setSwapBytes(true);
            TJpgDec.drawSdJpg(0, 0, currentFile);
          }
        } else {
          tft.fillScreen(TFT_BLACK);
        }
      } else {
        tft.fillScreen(TFT_BLACK);
      }
    }
    return; // Do not process any other touches while IP is displayed
  }

  // Track touch state
  if (touching && !isTouching) {
    isTouching = true;
    touchStartTime = millis();
    longPressHandled = false;
  } else if (!touching && isTouching) {
    isTouching = false;
    longPressHandled = false;
  } else if (touching && isTouching && !longPressHandled) {
    unsigned long touchDuration = millis() - touchStartTime;

    if (touchDuration >= 5000) {
      longPressHandled = true;
      ipDisplayActive = true;
      ipDisplayStart = millis();

      Serial.println("[TOUCH] 5s press - showing IP address");

      tft.fillScreen(TFT_BLACK);

      String ipAddr = WiFi.localIP().toString();
      if (isAPMode || ipAddr == "0.0.0.0") {
        ipAddr = "192.168.4.1";
      }
      String fullUrl = "http://" + ipAddr;

      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("WEB MANAGER", tft.width() / 2, 50, 4);

      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("Type this in your browser:", tft.width() / 2, 100, 2);

      // Big green URL - the raw IP that Sir can always use
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.drawString(fullUrl.c_str(), tft.width() / 2, 155, 4);

      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString("- OR -", tft.width() / 2, 200, 2);
      tft.drawString("http://chronosync.local", tft.width() / 2, 230, 2);

      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("(touch to dismiss)", tft.width() / 2, 290, 2);


    } else if (touchDuration > 1000) {
      // Countdown feedback for discoverability
      int remaining = 5 - (touchDuration / 1000);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.setTextDatum(BC_DATUM);
      char buf[32];
      sprintf(buf, "Hold for IP: %ds", remaining);
      tft.drawString(buf, tft.width() / 2, tft.height() - 20, 2);
    }
  }
}

void drawSystemText(const char *text, int16_t x, int16_t y, uint16_t color,
                    uint8_t font) {
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(x, y);
  tft.print(text); // Use simple print with setCursor
}

void showSplashScreen() {
  tft.fillScreen(TFT_BLACK);
  delay(100);
  tft.setSwapBytes(true);

  int titleX = (480 - CHRONOSYNC_WIDTH) / 2;
  int titleY = 90;
  tft.pushImage(titleX, titleY, CHRONOSYNC_WIDTH, CHRONOSYNC_HEIGHT,
                &SplashTextBitmaps[CHRONOSYNC_OFFSET]);

  int subtitleX = (480 - DIGITAL_MEMORY_VAULT_WIDTH) / 2;
  int subtitleY = 150;
  tft.pushImage(subtitleX, subtitleY, DIGITAL_MEMORY_VAULT_WIDTH,
                DIGITAL_MEMORY_VAULT_HEIGHT,
                &SplashTextBitmaps[DIGITAL_MEMORY_VAULT_OFFSET]);

  tft.setSwapBytes(false);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TAILWIND_CYAN, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("Initializing...", 240, 250, 2);

  delay(3000);
}

void loadOrder(String orientation) {
  Serial.printf("[LOAD] Loading order for %s orientation...\n",
                orientation.c_str());
  uint32_t heapBefore = ESP.getFreeHeap();

  // Feed watchdog at start of potentially long operation
  esp_task_wdt_reset();

  mediaFiles.clear();
  mediaFiles.shrink_to_fit(); // Release old memory

  std::vector<String> savedOrder;
  String path = "/config/order_" + orientation + ".json";

  if (SD.exists(path)) {
    File f = SD.open(path);
    if (f) {
      DynamicJsonDocument doc(4096);
      DeserializationError err = deserializeJson(doc, f);
      if (!err) {
        JsonArray arr = doc["files"];
        // Only load files from /photos/ folder for slideshow
        for (JsonVariant v : arr) {
          String s = v.as<String>();
          if (SD.exists(s) && s.indexOf("/photos/") >= 0) {
            savedOrder.push_back(s);
          }
        }
      } else {
        Serial.printf("[LOAD] Failed to parse order JSON: %s\n", err.c_str());
      }
      f.close();
    } else {
      Serial.println("[LOAD] Failed to open order file");
    }
  }

  // Feed watchdog before scanning directory
  esp_task_wdt_reset();

  std::vector<String> diskFiles;
  // ONLY scan photos folder for slideshow
  scanDirectory("/photos/" + orientation, diskFiles);

  // Feed watchdog after scanning
  esp_task_wdt_reset();

  // Add saved order files first
  for (const String &f : savedOrder) {
    mediaFiles.push_back(f);
  }

  // Add any new files not in saved order
  for (const String &f : diskFiles) {
    bool alreadyIn = false;
    for (const String &s : savedOrder) {
      if (s == f) {
        alreadyIn = true;
        break;
      }
    }
    if (!alreadyIn)
      mediaFiles.push_back(f);
  }

  // Feed watchdog before cleanup
  esp_task_wdt_reset();

  // Clean up temporary vectors
  savedOrder.clear();
  savedOrder.shrink_to_fit();
  diskFiles.clear();
  diskFiles.shrink_to_fit();

  uint32_t heapAfter = ESP.getFreeHeap();
  Serial.printf(
      "[LOAD] Loaded %d media files | Heap: %u -> %u bytes (used: %d)\n",
      mediaFiles.size(), heapBefore, heapAfter, (int)(heapBefore - heapAfter));
}

void saveOrderConfig(String orientation, DynamicJsonDocument &doc) {
  String path = "/config/order_" + orientation + ".json";
  File f = SD.open(path, FILE_WRITE);
  serializeJson(doc, f);
  f.close();
}

void playRawVideo(String filename, bool infiniteLoop) {
  File vFile = SD.open(filename);
  if (!vFile || vFile.isDirectory()) {
    currentFile = "";
    return;
  }

  // 1. Calculate Frame and Line size based on current rotation
  int screenWidth = tft.width();   // 320 or 480
  int screenHeight = tft.height(); // 480 or 320

  // Bytes per frame: (Total Pixels) * 2 bytes/pixel (RGB565)
  size_t bytesPerFrame = screenWidth * screenHeight * 2;

  // 2. Use swapBytes TRUE - the .vid file is RGB565 Little Endian from
  // JavaScript
  tft.setSwapBytes(true);

  // 3. TRANSACTION START: Optimize bus - keep SPI bus active
  tft.startWrite();

  bool keepPlaying = true;
  unsigned long frameStart;
  unsigned long lastWebCheck = 0;
  unsigned long touchStartTime = 0;
  unsigned long videoLastTapTime = 0; // Last tap time for double-tap detection
  bool wasTouching = false;
  const unsigned long WEB_CHECK_INTERVAL =
      200; // Check web every 200ms instead of every frame
  const unsigned long LONG_PRESS_EXIT_TIME = 1500; // 1.5 seconds to exit
  const unsigned long VIDEO_DOUBLE_TAP_TIME =
      1000; // Max time between taps for double-tap (ms) - generous for video
            // playback

  // Target ~120 FPS for much smoother animation (8ms per frame)
  // Provides safety cap while allowing much faster playback
  const unsigned long TARGET_FRAME_TIME = 8;

  while (keepPlaying) {
    frameStart = millis();

    // === INDUSTRIAL STABILITY: Feed watchdog during video playback ===
    esp_task_wdt_reset();

    if (vFile.available() < bytesPerFrame) {
      if (infiniteLoop && !isSlideshow && currentFile == filename) {
        vFile.seek(0);
      } else {
        keepPlaying = false;
        break;
      }
    }

    // Check touch IRQ (doesn't use SPI, safe during video playback)
    bool isTouching = (digitalRead(TOUCH_IRQ) == LOW);
    if (isTouching && !wasTouching) {
      // Touch started
      touchStartTime = millis();
      wasTouching = true;
      Serial.println(">>> Video: Touch started");
    } else if (isTouching && wasTouching) {
      // Touch continuing - check for long press
      if (millis() - touchStartTime > LONG_PRESS_EXIT_TIME) {
        Serial.println(
            ">>> Long press during video - returning to GIF preview");
        keepPlaying = false;
        currentFile = "";
        // Re-enter GIF preview mode
        enterGifPreviewMode();
        break;
      }
    } else if (!isTouching && wasTouching) {
      // Touch released
      wasTouching = false;
    }

    // Set the write window for a full screen
    tft.setAddrWindow(0, 0, screenWidth, screenHeight);

    // Read and push frame in large chunks for maximum speed
    size_t totalRead = 0;
    while (totalRead < bytesPerFrame) {
      size_t toRead = min((size_t)LINE_BUFFER_SIZE, bytesPerFrame - totalRead);
      int bytesRead = vFile.read(lineBuffer, toRead);

      if (bytesRead <= 0) {
        keepPlaying = false;
        break;
      }

      // Push pixels directly
      tft.pushColors((uint16_t *)lineBuffer, bytesRead / 2);
      totalRead += bytesRead;
    }

    // High-priority web server polling during media playback
    if (millis() - lastWebCheck > 200) { 
      server.handleClient();
      ArduinoOTA.handle(); // Crucial for OTA recovery if UI is stuck
      lastWebCheck = millis();
      if (!isSlideshow && currentFile != filename)
        keepPlaying = false;
    }

    // Minimal delay - only if frame was too fast (target 120 FPS)
    unsigned long elapsed = millis() - frameStart;
    if (elapsed < TARGET_FRAME_TIME) {
      delay(TARGET_FRAME_TIME - elapsed);
    }
    // If frame took longer than target, no delay - run as fast as possible
  }

  // 4. TRANSACTION END
  tft.endWrite();
  vFile.close();

  // 5. PHOTO MODE: Re-enable Byte Swapping for JPEGs
  tft.setSwapBytes(true);
}

void playMJPEG(String filename, bool infiniteLoop) {
  File mjpegFile = SD.open(filename);
  if (!mjpegFile || mjpegFile.isDirectory()) {
    currentFile = "";
    return;
  }

  bool keepPlaying = true;
  unsigned long lastWebCheck = 0;
  unsigned long touchStartTime = 0;
  bool wasTouching = false;
  const unsigned long WEB_CHECK_INTERVAL = 1000;
  const unsigned long LONG_PRESS_EXIT_TIME = 1500;

  const size_t MAX_JPEG_SIZE = 65536;
  uint8_t *jpegBuffer = (uint8_t *)malloc(MAX_JPEG_SIZE);

  if (!jpegBuffer) {
    mjpegFile.close();
    currentFile = "";
    return;
  }

  while (keepPlaying) {
    esp_task_wdt_reset();

    if (mjpegFile.available() < 4) {
      if (infiniteLoop && !isSlideshow && currentFile == filename) {
        mjpegFile.seek(0);
      } else {
        keepPlaying = false;
        break;
      }
    }

    uint32_t frameSize = 0;
    if (mjpegFile.read((uint8_t *)&frameSize, 4) != 4)
      break;
    if (frameSize == 0 || frameSize > MAX_JPEG_SIZE)
      break;
    if (mjpegFile.read(jpegBuffer, frameSize) != frameSize)
      break;

    TJpgDec.drawJpg(0, 0, jpegBuffer, frameSize);

    // Minimal touch check
    if (digitalRead(TOUCH_IRQ) == LOW) {
      if (!wasTouching) {
        touchStartTime = millis();
        wasTouching = true;
      } else if (millis() - touchStartTime > LONG_PRESS_EXIT_TIME) {
        keepPlaying = false;
        currentFile = "";
        enterGifPreviewMode();
        break;
      }
    } else {
      wasTouching = false;
    }

    // High-priority web server polling during media playback
    // MUST execute every frame so chunked index.html data doesn't stall the browser
    server.handleClient();
    ArduinoOTA.handle();

    if (millis() - lastWebCheck > 200) {
      lastWebCheck = millis();
      if (!isSlideshow && currentFile != filename)
        keepPlaying = false;
    }
  }

  tft.setSwapBytes(false); // Restore
  free(jpegBuffer);
  mjpegFile.close();
}

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h,
                uint16_t *bitmap) {
  if (y >= tft.height())
    return 0;
  // pushImage within outer startWrite/endWrite keeps SPI bus open for full
  // frame (DMA variant requires explicit DMA init in TFT_eSPI config — not
  // enabled)
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

void scanDirectory(const String &path, std::vector<String> &outFiles) {
  File root = SD.open(path);
  if (!root || !root.isDirectory())
    return;

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String n = String(file.name());
      if (!n.startsWith("/"))
        n = path + "/" + n;
      if (n.indexOf("._") == -1)
        outFiles.push_back(n);
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
}

void showSlideshow() {
  if (mediaFiles.empty())
    return;

  currentMediaIndex = (currentMediaIndex + 1) % mediaFiles.size();
  String f = mediaFiles[currentMediaIndex];

  if (f.indexOf("/portrait/") >= 0)
    tft.setRotation(0);
  else
    tft.setRotation(1);

  if (f.endsWith(".vid") || f.endsWith(".mjpeg")) {
    currentFile = f;
  } else if (f.endsWith(".jpg") || f.endsWith(".jpeg")) {
    currentFile = f;
    tft.fillScreen(TFT_BLACK);
    tft.setSwapBytes(true);
    TJpgDec.drawSdJpg(0, 0, f);
  }
}

void drawPlainDigit(int x, int y, char digit, bool isDarkTheme, float scale) {
  uint16_t digitColor = isDarkTheme ? TFT_WHITE : TFT_BLACK;
  uint16_t bgColor = isDarkTheme ? TFT_BLACK : TFT_WHITE;
  
  tft.setSwapBytes(true);
  drawMontDigitGlyph(x, y, digit, digitColor, bgColor, scale);
  tft.setSwapBytes(false);
  yield();
}

void drawFlipCard(int x, int y, int w, int h, char digit, bool isDarkTheme,
                  float scale) {
  // Dark: dark charcoal card on black background
  // Light: light grey card on white background
  uint16_t cardColor = isDarkTheme ? 0x2945 : 0xD6BA;
  uint16_t digitColor = isDarkTheme ? TFT_WHITE : TFT_BLACK;
  uint16_t bgColor = isDarkTheme ? TFT_BLACK : TFT_WHITE;

  // Draw rounded card background
  tft.fillRoundRect(x, y, w, h, 8, cardColor);

  // Calculate digit dimensions for centering
  int digitW = 0;
  const char *f = strchr(MontserratBoldClock56Chars, digit);
  if (f) {
    digitW =
        (int)(pgm_read_byte(
                  &MontserratBoldClock56Width[f - MontserratBoldClock56Chars]) *
              scale);
  }

  int digitX = x + (w - digitW) / 2;
  int digitY = y + (h - (int)(56 * scale)) / 2 + 1;

  tft.setSwapBytes(true);
  drawMontDigitGlyph(digitX, digitY, digit, digitColor, cardColor, scale);
  tft.setSwapBytes(false);

  // Draw horizontal split line
  uint16_t lineColor = isDarkTheme ? 0x0841 : TFT_WHITE;
  tft.drawFastHLine(x, y + h / 2 - 1, w, lineColor);
  tft.drawFastHLine(x, y + h / 2, w, lineColor);

  // Feed WDT — critical when scale is large (each card draws ~7000 pushImage
  // rows)
  yield();
}

void drawFlipClockString(int x, int y, const char *str, bool isDarkTheme,
                         float scale, int cardW, int cardH, int spacing) {
  uint16_t mainBgColor = isDarkTheme ? TFT_BLACK : TFT_WHITE;
  uint16_t digitColor = isDarkTheme ? TFT_WHITE : TFT_BLACK;
  int curX = x;

  for (const char *p = str; *p; ++p) {
    if (*p == ':') {
      int colonW = 0;
      const char *f = strchr(MontserratBoldClock56Chars, ':');
      if (f)
        colonW =
            (int)(pgm_read_byte(
                      &MontserratBoldClock56Width[f -
                                                  MontserratBoldClock56Chars]) *
                  scale);

      int colonY = y + (cardH - (int)(56 * scale)) / 2 + 1;
      tft.setSwapBytes(true);
      drawMontDigitGlyph(curX, colonY, ':', digitColor, mainBgColor, scale);
      tft.setSwapBytes(false);
      curX += colonW + spacing;
    } else if (*p >= '0' && *p <= '9') {
      drawFlipCard(curX, y, cardW, cardH, *p, isDarkTheme, scale);
      curX += cardW + spacing;
    } else {
      curX += spacing;
    }
  }
}

void showClock() {
  uint16_t bgColor = (clockTheme == "light") ? TFT_WHITE : TFT_BLACK;
  tft.fillScreen(bgColor);
  tft.setSwapBytes(false); // Text mode
  updateClock(true);
}

// Force clock redraw (resets static tracking variables)
void forceClockRedraw() {
  // Use current universal orientation (no separate clock orientation)
  if (currentOrientation == "portrait") {
    tft.setRotation(0); // Portrait
  } else {
    tft.setRotation(1); // Landscape
  }
  uint16_t bgColor = (clockTheme == "light") ? TFT_WHITE : TFT_BLACK;
  tft.fillScreen(bgColor);
  tft.setSwapBytes(false); // Text mode
  updateClock(true);
}

// Updates clock display (called every second for live time)
void updateClock(bool forceRedraw) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setTextSize(2);
    int centerX = tft.width() / 2;
    int centerY = tft.height() / 2;
    tft.fillRect(0, centerY - 30, tft.width(), 60, TFT_WHITE);
    tft.drawString("--:--", centerX, centerY, 4);
    return;
  }

  // Capture time components
  char hoursStr[3], minsStr[3], secsStr[3], ampmStr[3] = "";
  if (use24Hour) {
    strftime(hoursStr, sizeof(hoursStr), "%H", &timeinfo);
  } else {
    strftime(hoursStr, sizeof(hoursStr), "%I", &timeinfo);
    strftime(ampmStr, sizeof(ampmStr), "%p", &timeinfo);
  }
  strftime(minsStr, sizeof(minsStr), "%M", &timeinfo);
  strftime(secsStr, sizeof(secsStr), "%S", &timeinfo);

  // Static tracking
  static int prevMin = -1;
  static int prevSec = -1;
  static int prevWidth = 0;
  static bool prevMode = use24Hour;

  bool minChanged = (timeinfo.tm_min != prevMin || tft.width() != prevWidth || use24Hour != prevMode || forceRedraw);
  bool secChanged = (timeinfo.tm_sec != prevSec || minChanged);

  if (!secChanged) return; // Nothing to update

  bool isDark = (clockTheme == "dark");
  uint16_t mainBgColor = isDark ? TFT_BLACK : TFT_WHITE;
  uint16_t textColor = isDark ? TFT_WHITE : TFT_BLACK;
  uint16_t accentColor = isDark ? 0xDEFB : 0x2104;

  int screenW = tft.width();
  int screenH = tft.height();

  // Full Redraw or Minute Change
  if (forceRedraw || minChanged) {
    tft.fillScreen(mainBgColor);
    prevMin = timeinfo.tm_min;
    prevWidth = screenW;
    prevMode = use24Hour; // Corrected from use24H

    if (screenW == 320) {
      // === PORTRAIT LAYOUT v4.0 — TFT Font 7 (crisp, no scaling) ===
      int screenCenterX = screenW / 2;

      // AM/PM top-left, Date top-right
      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(accentColor, mainBgColor);
      if (!use24Hour) tft.drawString(ampmStr, 12, 18, 2);

      tft.setTextDatum(TR_DATUM);
      char dateStr[64]; strftime(dateStr, sizeof(dateStr), "%a %b %d", &timeinfo);
      tft.drawString(dateStr, screenW - 12, 18, 2);

      // HH — font 7 (48px crisp 7-segment digits) centered
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(isDark ? TFT_WHITE : TFT_BLACK, mainBgColor);
      float clockScale = 2.0;
      int hhW = getMontStringWidth(hoursStr, 4, clockScale);
      drawMontDigitString(screenCenterX - hhW/2, screenH / 2 - 120, hoursStr, 4, textColor, mainBgColor, clockScale);
      
      int mmW = getMontStringWidth(minsStr, 4, clockScale);
      drawMontDigitString(screenCenterX - mmW/2, screenH / 2 + 10, minsStr, 4, textColor, mainBgColor, clockScale);

      // Timezone footer
      tft.setTextDatum(BC_DATUM);
      tft.setTextColor(accentColor, mainBgColor);
      tft.drawString(timezoneName, screenCenterX, screenH - 14, 2);

    } else {
      // === LANDSCAPE LAYOUT v4.0 — TFT Font 8 (75px crisp, zero WDT risk) ===
      int screenCenterX = screenW / 2;
      int screenCenterY = screenH / 2;

      tft.setTextColor(isDark ? TFT_WHITE : TFT_BLACK, mainBgColor);

      // HOURS left-center, MINS right-center — font 8 is 75px, native 7-seg
      float clockScale = 1.8;
      int hhW = getMontStringWidth(hoursStr, 4, clockScale);
      drawMontDigitString(screenCenterX - hhW - 25, screenCenterY - 60, hoursStr, 4, textColor, mainBgColor, clockScale);

      int mmW = getMontStringWidth(minsStr, 4, clockScale);
      drawMontDigitString(screenCenterX + 25, screenCenterY - 60, minsStr, 4, textColor, mainBgColor, clockScale);

      // Colon separator
      int colonW = getMontStringWidth(":", 0, clockScale);
      drawMontDigitString(screenCenterX - colonW/2, screenCenterY - 60, ":", 0, accentColor, mainBgColor, clockScale);

      // Footer: Date left | AM/PM center | Timezone right
      char dateStr[64]; strftime(dateStr, sizeof(dateStr), "%A, %B %d, %Y", &timeinfo);
      tft.setTextDatum(BL_DATUM);
      tft.setTextColor(textColor, mainBgColor);
      tft.drawString(dateStr, 14, screenH - 10, 2);

      tft.setTextDatum(BC_DATUM);
      if (!use24Hour) {
        tft.setTextColor(accentColor, mainBgColor);
        tft.drawString(ampmStr, screenCenterX, screenH - 10, 4);
      }

      tft.setTextDatum(BR_DATUM);
      tft.setTextColor(accentColor, mainBgColor);
      tft.drawString(timezoneName, screenW - 14, screenH - 10, 2);
    }
  }

  // The original update SECONDS block is removed as per instruction.
  // prevSec = timeinfo.tm_sec; // No longer needed as seconds are not tracked for redraw
  yield();
}

// === INDUSTRIAL STABILITY FUNCTIONS ===

void checkHeapHealth() {
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t minHeap = ESP.getMinFreeHeap();
  uint32_t largestBlock = ESP.getMaxAllocHeap();

  Serial.printf("[HEAP] Free: %u bytes | Min Ever: %u bytes | Largest Block: "
                "%u bytes | Media Files: %d\n",
                freeHeap, minHeap, largestBlock, mediaFiles.size());

  // Emergency restart if free heap drops below critical threshold
  if (freeHeap < MIN_FREE_HEAP) {
    Serial.println(
        "[CRITICAL] Low memory detected! Triggering emergency restart...");
    Serial.printf("Free heap: %u < %u (threshold)\n", freeHeap, MIN_FREE_HEAP);
    delay(500); // Allow serial to flush
    ESP.restart();
  }

  // Warning at 60KB threshold
  if (freeHeap < LOW_HEAP_WARNING) {
    Serial.printf(
        "[WARNING] Memory getting low - %u bytes free (threshold: %u)\n",
        freeHeap, LOW_HEAP_WARNING);
    // Try to free some memory by shrinking vectors
    if (!mediaFiles.empty()) {
      mediaFiles.shrink_to_fit();
    }
    if (!gifFiles.empty()) {
      gifFiles.shrink_to_fit();
    }
  }
}

void checkScheduledMaintenance() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return; // Can't check time if not synced
  }

  // Check if current time is 3:00 AM (hour == 3, minute 0-5 to avoid multiple
  // triggers)
  if (timeinfo.tm_hour == 3 && timeinfo.tm_min < 5) {
    Serial.println(
        "[MAINTENANCE] Scheduled 3:00 AM maintenance reboot triggered");
    Serial.println("Clearing heap fragmentation and refreshing system...");

    // Save current state before restart
    saveDeviceState();

    delay(500); // Allow serial to flush
    ESP.restart();
  }
}

void setBacklightBrightness(int brightness) {
  // Clamp brightness to 0-255 range
  brightness = constrain(brightness, 0, 255);
  currentBrightness = brightness;
  ledcWrite(BL_PWM_CHANNEL, brightness);
}

void updateBacklightForTimeOfDay() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return; // Can't adjust if time not synced
  }

  int hour = timeinfo.tm_hour;

  // Dim during late night hours (11 PM - 6 AM)
  if (hour >= 23 || hour < 6) {
    if (currentBrightness != dimBrightness) {
      setBacklightBrightness(dimBrightness);
      Serial.printf("[BACKLIGHT] Night mode: Dimmed to %d%% (hour: %d)\n",
                    (dimBrightness * 100) / 255, hour);
    }
  } else {
    // Normal brightness during day (80%)
    if (currentBrightness != 204) {
      setBacklightBrightness(204);
      Serial.printf("[BACKLIGHT] Day mode: Normal brightness 80%% (hour: %d)\n",
                    hour);
    }
  }
}

// === SD CARD MONITORING FUNCTIONS ===

void checkSDCardStatus() {
  if (!sdReady || !sdCardPresent) {
    // Try to detect if SD card is inserted
    bool cardDetected =
        SD.begin(SD_CS, sdSPI, 20000000); // 20MHz SPI for stability

    if (cardDetected) {
      Serial.println("[SD] SD card detected! Reinitializing...");
      sdCardPresent = true;
      sdReady = true;

      // Recreate directory structure
      SD.mkdir("/photos/landscape");
      SD.mkdir("/photos/portrait");
      SD.mkdir("/gifs/landscape");
      SD.mkdir("/gifs/portrait");
      SD.mkdir("/config");

      // Reload media files
      if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        loadOrder(currentOrientation);
        xSemaphoreGive(stateMutex);
      }

      Serial.println("[SD] SD card reinitialized successfully");
    }
  } else {
    // SD is currently marked as ready. Do a lightweight check:
    File root = SD.open("/");
    if (!root) {
      // SD card was removed or lost connection
      Serial.println("[SD] SD card removed! Triggering fallback...");
      SD.end(); // Unmount to clean up driver state
      handleSDCardRemoval();
      sdCardPresent = false;
      sdReady = false;
    } else {
      root.close();
    }
  }
}

void handleSDCardRemoval() {
  // Take mutex to safely update state
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Clear media files
    mediaFiles.clear();
    currentFile = "";
    isSlideshow = false;

    xSemaphoreGive(stateMutex);
  }

  // Display error message on screen
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);

  if (currentOrientation == "landscape") {
    tft.drawString("SD CARD REMOVED", 240, 140, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Please reinsert SD card", 240, 180, 2);
  } else {
    tft.drawString("SD CARD", 160, 200, 4);
    tft.drawString("REMOVED", 160, 240, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Please reinsert", 160, 290, 2);
    tft.drawString("SD card", 160, 315, 2);
  }
}