#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h> 
#include <ArduinoJson.h>
#include <vector>
#include <time.h>
#include "web_index.h"
#include "MontserratBoldClock56.h"
#include "MontserratBoldAMPM.h"
#include "SplashText.h"

// --- TAILWIND COLORS (RGB565 Format) ---
#define TAILWIND_YELLOW 0xF642 // Matches text-yellow-400 (#FACC15)
#define TAILWIND_CYAN   0x269D // Matches text-cyan-400 (#22D3EE)
#define TAILWIND_SLATE  0x10A2 // Matches bg-slate-800
#define TAILWIND_ORANGE 0xFC60 // Orange for IP address

// --- NTP TIME SYNC (Manila, Philippines - UTC+8) ---
const char* ntpServer = "pool.ntp.org";
long gmtOffset_sec = 8 * 3600;  // UTC+8 for Manila (default, can be changed)
const int daylightOffset_sec = 0;     // No daylight saving in Philippines

// --- HARDWARE PINOUT (ESP32-3248S035 3.5" IPS CYD) ---
// SD Card uses HSPI bus (separate from TFT)
#define SD_CS    5
#define SD_MOSI  23
#define SD_MISO  19
#define SD_SCK   18

// Backlight pin for 3.5" CYD
#define TFT_BL   27

// --- OBJECTS ---
TFT_eSPI tft = TFT_eSPI();
WebServer server(80);
SPIClass sdSPI(HSPI);
File uploadFile;

// --- GLOBAL STATE ---
String ssid = ""; // [Inference] Loaded from config/wifi.json
String password = ""; // [Inference] Loaded from config/wifi.json
String currentFile = "";
bool isSlideshow = true;
bool sdReady = false;
unsigned long lastSlideTime = 0;
int slideInterval = 5000;
std::vector<String> mediaFiles;
int currentMediaIndex = 0;
String currentOrientation = "landscape"; 
long bootId = 0;

// --- STATUS BAR ---
unsigned long lastMessageUpdate = 0; // Last time encouragement message changed
int currentMessageIndex = 0;        // Current encouragement message

// Encouragement messages
const char* encouragementMessages[] = {
  "You're doing great!",
  "Stay focused!",
  "Keep it up!",
  "You've got this!",
  "Believe in yourself!",
  "One step at a time!",
  "You're amazing!",
  "Never give up!"
};
const int messageCount = 8;

// --- CLOCK STATE ---
bool clockEnabled = true;          // Whether clock is enabled in slideshow rotation (default ON)
bool clockShowAtEnd = true;        // Show clock at end of slideshow cycle (default ON)
bool isShowingClock = false;       // Currently showing clock
bool isClockOnlyMode = false;      // Clock-only mode (no auto-hide)
bool clockOnlyScreen = false;      // Clock-only screen setting (no duration when showing clock)
int clockDisplayDuration = 30;     // How long clock shows in slideshow (seconds, default 30)
int clockPhotoInterval = 0;        // Show clock every N photos (0=at end of slideshow, default)
int photosSinceLastClock = 0;      // Counter for photos shown since last clock
unsigned long clockStartTime = 0;  // When clock started showing
unsigned long lastClockUpdate = 0; // Last time clock was redrawn (for live updates)
bool use24Hour = false;            // 12-hour or 24-hour format (default 12-hour)
String timezoneName = "UTC+8 Manila"; // Timezone display name 
String barPosition = "top";        // Slideshow info bar position (top/bottom)
String customMessages = "Creating memories...\\nLife is beautiful!\\nEnjoy the moments!\\nYou're amazing!\\nNever give up!"; // Custom messages for slideshow

// --- VIDEO BUFFER ---
// Larger buffer for smoother video playback (reduces "curtain" effect)
// 480x320 screen = 153600 pixels * 2 bytes = 307200 bytes per frame
// Using 32KB buffer for maximum speed on 3.5" display
#define LINE_BUFFER_SIZE 32768
uint8_t lineBuffer[LINE_BUFFER_SIZE];

// --- PROTOTYPES ---
void showSlideshow();
void showSplashScreen();
void forceClockRedraw();
void drawStatusBar();

// --- STATUS BAR FUNCTION ---

void drawStatusBar() {
  // Don't draw status bar when showing clock (clock has its own overlay)
  if (isShowingClock) return;
  
  static unsigned long lastStatusBarDraw = 0;
  
  // Limit status bar redraws to 10 times per second to reduce flicker
  if (millis() - lastStatusBarDraw < 100) return;
  lastStatusBarDraw = millis();
  
  int barHeight = 20; // Original size
  int screenWidth = (currentOrientation == "landscape") ? 480 : 320;
  int screenHeight = (currentOrientation == "landscape") ? 320 : 480;
  
  // Parse custom messages (split by \n)
  static std::vector<String> parsedMessages;
  static String lastCustomMessages = "";
  if (customMessages != lastCustomMessages) {
    parsedMessages.clear();
    String temp = customMessages;
    temp.replace("\\n", "\n"); // Convert escaped newlines
    int start = 0;
    int end = temp.indexOf('\n');
    while (end != -1) {
      parsedMessages.push_back(temp.substring(start, end));
      start = end + 1;
      end = temp.indexOf('\n', start);
    }
    if (start < temp.length()) {
      parsedMessages.push_back(temp.substring(start));
    }
    lastCustomMessages = customMessages;
  }
  
  // Update message index every 5 seconds (faster rotation)
  if (millis() - lastMessageUpdate > 5000) {
    currentMessageIndex = (currentMessageIndex + 1) % (parsedMessages.empty() ? messageCount : parsedMessages.size());
    lastMessageUpdate = millis();
  }
  
  // Get current time with spaces around colon
  struct tm timeinfo;
  String timeDate = "No Time";
  if(getLocalTime(&timeinfo)) {
    char timeBuf[20];
    if(use24Hour) {
      strftime(timeBuf, sizeof(timeBuf), "%H : %M", &timeinfo);
    } else {
      strftime(timeBuf, sizeof(timeBuf), "%I : %M %p", &timeinfo);
    }
    timeDate = String(timeBuf);
  }
  
  // Determine bar position and coordinates
  int barY = (barPosition == "top") ? 0 : (screenHeight - barHeight);
  
  // Clear the entire status bar area
  tft.fillRect(0, barY, screenWidth, barHeight, TFT_BLACK);
  
  // Draw encouragement message on the left
  String currentMessage;
  if (parsedMessages.empty()) {
    currentMessage = encouragementMessages[currentMessageIndex];
  } else {
    currentMessage = parsedMessages[currentMessageIndex];
  }
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(1);
  tft.drawString(currentMessage, 5, barY + (barHeight / 2), 2); // Back to original font size
  
  // Draw time and date on the right
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(timeDate, screenWidth - 5, barY + (barHeight / 2), 2); // Back to original font size
}

void showClock();
void updateClock(bool forceRedraw = false);
void forceClockRedraw();
void drawSystemText(const char* text, int16_t x, int16_t y, uint16_t color, uint8_t font);
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
void scanDirectory(const String &path, std::vector<String> &outFiles);
// Draw a single glyph (from MontserratBoldClock56) at x,y. Uses PROGMEM arrays
void drawMontDigitGlyph(int16_t x, int16_t y, char ch, uint16_t fgColor = TFT_BLACK, uint16_t bgColor = TFT_WHITE, float scale = 1.0) {
  const char* chars = MontserratBoldClock56Chars;
  const char* found = strchr(chars, ch);
  if(!found) return; // char not in the set
  int idx = found - chars;
  uint16_t offset = pgm_read_word(&MontserratBoldClock56Offsets[idx]);
  uint8_t w = pgm_read_byte(&MontserratBoldClock56Width[idx]);
  uint8_t h = pgm_read_byte(&MontserratBoldClock56Height[idx]);

  // Scale dimensions
  int scaledW = (int)(w * scale);
  int scaledH = (int)(h * scale);

  // Buffer for scaled row
  uint16_t rowBuf[128]; // Larger buffer for scaled output

  for(int row=0; row < scaledH; ++row) {
    int srcRow = (int)(row / scale);
    if(srcRow >= h) srcRow = h - 1;
    
    for(int col=0; col < scaledW; ++col) {
      int srcCol = (int)(col / scale);
      if(srcCol >= w) srcCol = w - 1;
      
      uint16_t v = pgm_read_word(&MontserratBoldClock56Bitmaps[offset + srcRow * w + srcCol]);
      // Invert: original has white text on black, we want black text on white
      // If pixel is black (0x0000), make it bgColor (white)
      // Otherwise, it's part of the text anti-aliasing, invert it
      if(v == 0x0000) {
        rowBuf[col] = bgColor; // Background pixel
      } else {
        // For anti-aliased pixels, invert them to work on white background
        // Convert grayscale values: brighter pixels become darker
        uint8_t r = (v >> 11) & 0x1F;
        uint8_t g = (v >> 5) & 0x3F;
        uint8_t b = v & 0x1F;
        
        // Invert the brightness (31-r for red/blue, 63-g for green)
        r = 31 - r;
        g = 63 - g;
        b = 31 - b;
        
        rowBuf[col] = (r << 11) | (g << 5) | b;
      }
    }
    // push a single-row image with scaled width
    tft.pushImage(x, y + row, scaledW, 1, rowBuf);
  }
}

// Draw a string of digit glyphs (supports 0-9 and colon) with optional spacing
void drawMontDigitString(int16_t x, int16_t y, const char* s, uint8_t spacing=4, uint16_t fgColor = TFT_BLACK, uint16_t bgColor = TFT_WHITE, float scale = 1.0) {
  int16_t curX = x;
  for(const char* p = s; *p; ++p) {
    const char* found = strchr(MontserratBoldClock56Chars, *p);
    if(!found) {
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

// Draw AM/PM string using smaller font
void drawAMPMString(int16_t x, int16_t y, const char* s, uint8_t spacing=2, uint16_t fgColor = TFT_BLACK, uint16_t bgColor = TFT_WHITE, float scale = 1.0) {
  int16_t curX = x;
  for(const char* p = s; *p; ++p) {
    const char* found = strchr(MontserratBoldAMPMChars, *p);
    if(!found) {
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
    
    for(int row=0; row < scaledH; ++row) {
      int srcRow = (int)(row / scale);
      if(srcRow >= h) srcRow = h - 1;
      
      for(int col=0; col < scaledW; ++col) {
        int srcCol = (int)(col / scale);
        if(srcCol >= w) srcCol = w - 1;
        
        uint16_t v = pgm_read_word(&MontserratBoldAMPMBitmaps[offset + srcRow * w + srcCol]);
        
        if(v == 0x0000) {
          rowBuf[col] = bgColor;
        } else {
          uint8_t r = (v >> 11) & 0x1F;
          uint8_t g = (v >> 5) & 0x3F;
          uint8_t b = v & 0x1F;
          r = 31 - r;
          g = 63 - g;
          b = 31 - b;
          rowBuf[col] = (r << 11) | (g << 5) | b;
        }
      }
      tft.pushImage(curX, y + row, scaledW, 1, rowBuf);
    }
    curX += (int)(w * scale) + (int)(spacing * scale);
  }
}
void playRawVideo(String filename, bool infiniteLoop);
void loadOrder(String orientation);
void saveOrderConfig(String orientation, DynamicJsonDocument& doc);
void loadWiFiConfig();
void saveWiFiConfig(String s, String p);
void setHardwareRotation(String orientation);

void setup() {
  Serial.begin(115200);
  WiFi.setTxPower(WIFI_POWER_11dBm); // Eco Mode
  bootId = millis(); 

  // --- 1. SD CARD INIT ---
  // Note: SPI pins are shared with the TFT display
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  
  // INCREASED SPEED: 16MHz for faster SD reads
  if (SD.begin(SD_CS, sdSPI, 16000000)) {
    sdReady = true;
    SD.mkdir("/photos/landscape");
    SD.mkdir("/photos/portrait");
    SD.mkdir("/gifs/landscape");
    SD.mkdir("/gifs/portrait");
    SD.mkdir("/config");
    loadWiFiConfig(); 
  }

  // --- 2. STATUS BAR INIT ---
  Serial.println("\n========================================");
  Serial.println("STATUS BAR INITIALIZED");
  Serial.println("Showing IP address and encouragement messages");
  Serial.println("========================================\n");
  
  // --- 3. TFT DISPLAY INIT (3.5" IPS 480x320) ---
  tft.begin();
  
  // Enable backlight FIRST (GPIO27 for 3.5" CYD)
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  delay(100); // Let backlight stabilize
  
  setHardwareRotation(currentOrientation);
  tft.fillScreen(TFT_BLACK);
  
  //Test: Draw colored rectangles to verify display is working
  /*
  Serial.println("Drawing test rectangles (480x320)...");
  tft.fillRect(0, 0, 50, 50, TFT_RED);
  tft.fillRect(430, 0, 50, 50, TFT_GREEN);
  tft.fillRect(0, 270, 50, 50, TFT_BLUE);
  tft.fillRect(430, 270, 50, 50, TFT_YELLOW);
  delay(1000); 
  */

  tft.fillScreen(TFT_BLACK);
  
  // CRITICAL: Initialize with swapBytes OFF for text
  tft.setSwapBytes(false);
  delay(50);
  
  Serial.println("TFT initialized, showing splash...");
  
  // --- SPLASH SCREEN ---
  showSplashScreen();
  unsigned long splashStart = millis();

  // Init JPEG Decoder with callback
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(tft_output);
  
  // --- 3. WIFI & WEB SERVER INIT ---
  Serial.println("=====================================");
  Serial.println("WiFi Connection Starting...");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Password length: ");
  Serial.println(password.length());
  Serial.println("=====================================");
  
  WiFi.disconnect(true);  // Clear any previous connection
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long start = millis();
  
  /*
  // Show WiFi connecting message on splash screen
  tft.setSwapBytes(false); // Disable for text
  drawSystemText("Connecting to WiFi...", 10, 205, TFT_YELLOW, 2);
  */
  
  // Extended timeout for mobile hotspots (15 seconds)
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts % 10 == 0) {
      Serial.println();
      Serial.print("WiFi Status: ");
      switch(WiFi.status()) {
        case WL_IDLE_STATUS: Serial.println("IDLE"); break;
        case WL_NO_SSID_AVAIL: Serial.println("NO SSID AVAILABLE - Check hotspot name!"); break;
        case WL_SCAN_COMPLETED: Serial.println("SCAN COMPLETED"); break;
        case WL_CONNECT_FAILED: Serial.println("CONNECT FAILED - Check password!"); break;
        case WL_CONNECTION_LOST: Serial.println("CONNECTION LOST"); break;
        case WL_DISCONNECTED: Serial.println("DISCONNECTED"); break;
        default: Serial.println(WiFi.status()); break;
      }
    }
  }
  Serial.println();
  
  // Ensure splash screen shows for minimum 3 seconds
  while(millis() - splashStart < 3000) {
    delay(100);
  }
  
  // Clear splash screen
  tft.fillScreen(TFT_BLACK);
  
  if (WiFi.status() == WL_CONNECTED) {
      String ipAddress = WiFi.localIP().toString();
      Serial.println("=====================================");
      Serial.println("WiFi Connected!");
      Serial.print("IP Address: ");
      Serial.println(ipAddress);
      Serial.println("=====================================");
      
      drawSystemText("WiFi Connected", 5, 5, TFT_GREEN, 2);
      drawSystemText(ipAddress.c_str(), 5, 25, TFT_GREEN, 2);
      
      // --- NTP TIME SYNC ---
      Serial.println("Syncing time with NTP server...");
      drawSystemText("Syncing time...", 5, 45, TFT_CYAN, 2);
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      
      // Wait for time sync (max 5 seconds)
      struct tm timeinfo;
      int syncAttempts = 0;
      while(!getLocalTime(&timeinfo) && syncAttempts < 10) {
        delay(500);
        syncAttempts++;
      }
      
      if(getLocalTime(&timeinfo)) {
        char timeStr[30];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
        Serial.print("Time synced: ");
        Serial.println(timeStr);
        drawSystemText("Time synced!", 5, 65, TFT_GREEN, 2);
      } else {
        Serial.println("Time sync failed!");
        drawSystemText("Time sync failed", 5, 65, TFT_YELLOW, 2);
      }
      
      delay(2000);
  } else {
      Serial.println("=====================================");
      Serial.println("WiFi Connection Failed!");
      Serial.print("Final Status: ");
      switch(WiFi.status()) {
        case WL_NO_SSID_AVAIL: Serial.println("SSID not found - Is hotspot ON and visible?"); break;
        case WL_CONNECT_FAILED: Serial.println("Wrong password!"); break;
        default: Serial.print("Code: "); Serial.println(WiFi.status()); break;
      }
      Serial.println("=====================================");
      
      drawSystemText("WiFi FAILED", 5, 5, TFT_RED, 2);
      drawSystemText("Check hotspot & password", 5, 25, TFT_YELLOW, 2);
      delay(3000);
  }
  
  // NOW set swapBytes for image rendering
  tft.setSwapBytes(true);

  // --- WEB SERVER ROUTING ---
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", MAIN_page); });
  
  server.on("/api/status", HTTP_GET, []() {
    String json = "{";
    json += "\"slideshow\":" + String(isSlideshow ? "true" : "false") + ",";
    json += "\"orientation\":\"" + currentOrientation + "\",";
    json += "\"bootId\":" + String(bootId);
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/api/slideshow/start", HTTP_GET, []() {
    // Validate: require at least one photo to be loaded
    if (mediaFiles.empty()) {
      loadOrder(currentOrientation);
    }
    
    if (mediaFiles.empty()) {
      server.send(400, "application/json", "{\"error\":\"Please select and play a photo first before enabling slideshow\"}");
      return;
    }
    
    isSlideshow = true;
    isShowingClock = false;
    isClockOnlyMode = false;
    photosSinceLastClock = 0;
    lastSlideTime = millis();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/slideshow/stop", HTTP_GET, []() {
    isSlideshow = false;
    isShowingClock = false;
    isClockOnlyMode = false;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/play", HTTP_GET, []() {
    if (server.hasArg("file")) {
      String f = server.arg("file");
      if (SD.exists(f)) {
        currentFile = f;
        isSlideshow = false;
        isShowingClock = false; // Hide clock when playing any media
        
        // Apply saved orientation when starting new media
        setHardwareRotation(currentOrientation);
        
        if (f.endsWith(".vid")) {
           server.send(200, "application/json", "{\"ok\":true}");
           // Playback happens in loop()
        } else {
           tft.fillScreen(TFT_BLACK);
           tft.setSwapBytes(true);
           TJpgDec.drawSdJpg(0, 0, currentFile);
           server.send(200, "application/json", "{\"ok\":true}");
        }
      } else server.send(404, "application/json", "{\"error\":\"File not found\"}");
    }
  });

  server.on("/api/photo/preview", HTTP_GET, []() {
    if(server.hasArg("filename")) {
        String f = server.arg("filename");
        if(SD.exists(f)) {
            File file = SD.open(f, "r");
            server.streamFile(file, "image/jpeg");
            file.close();
        } else server.send(404, "text/plain", "Not Found");
    }
  });

  server.on("/api/order", HTTP_POST, []() {
    if(!server.hasArg("plain")) return server.send(400, "text/plain", "Body missing");
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, server.arg("plain"));
    String orient = doc["orientation"].as<String>();
    saveOrderConfig(orient, doc); 
    if(orient == currentOrientation) loadOrder(currentOrientation);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/list", HTTP_GET, []() {
    std::vector<String> allFiles;
    
    // Helper lambda to get files in saved order for a folder
    auto getOrderedFiles = [](const String& folder, const String& orientation, std::vector<String>& out) {
      std::vector<String> savedOrder;
      String orderPath = "/config/order_" + orientation + ".json";
      
      // Load saved order if exists
      if(SD.exists(orderPath)) {
        File f = SD.open(orderPath);
        DynamicJsonDocument doc(4096);
        deserializeJson(doc, f);
        JsonArray arr = doc["files"];
        for(JsonVariant v : arr) {
          String s = v.as<String>();
          // Only include files from this specific folder
          if(s.startsWith(folder) && SD.exists(s)) {
            savedOrder.push_back(s);
          }
        }
        f.close();
      }
      
      // Scan disk for current files
      std::vector<String> diskFiles;
      scanDirectory(folder, diskFiles);
      
      // Add saved order files first
      for(const String& f : savedOrder) {
        out.push_back(f);
      }
      
      // Add any new files not in saved order
      for(const String& f : diskFiles) {
        bool alreadyIn = false;
        for(const String& s : savedOrder) { 
          if(s == f) { alreadyIn = true; break; } 
        }
        if(!alreadyIn) out.push_back(f);
      }
    };
    
    // Get ordered files for each folder
    std::vector<String> photosLandscape, photosPortrait, gifsLandscape, gifsPortrait;
    getOrderedFiles("/photos/landscape", "landscape", photosLandscape);
    getOrderedFiles("/photos/portrait", "portrait", photosPortrait);
    getOrderedFiles("/gifs/landscape", "landscape", gifsLandscape);
    getOrderedFiles("/gifs/portrait", "portrait", gifsPortrait);
    
    // Combine all files
    for(const String& f : photosLandscape) allFiles.push_back(f);
    for(const String& f : photosPortrait) allFiles.push_back(f);
    for(const String& f : gifsLandscape) allFiles.push_back(f);
    for(const String& f : gifsPortrait) allFiles.push_back(f);
    
    String out = "{\"files\":[";
    for(size_t i=0; i<allFiles.size(); i++) {
      out += "\"" + allFiles[i] + "\"";
      if(i < allFiles.size()-1) out += ",";
    }
    out += "]}";
    server.send(200, "application/json", out);
  });

  server.on("/api/photos", HTTP_GET, []() {
    // Return photos for current orientation with full paths
    String out = "{\"photos\":[";
    for(size_t i=0; i<mediaFiles.size(); i++) {
      out += "\"" + mediaFiles[i] + "\"";
      if(i < mediaFiles.size()-1) out += ",";
    }
    out += "],\"count\":" + String(mediaFiles.size()) + "}";
    server.send(200, "application/json", out);
  });

  server.on("/api/set", HTTP_GET, []() {
    if (server.hasArg("orientation")) {
      currentOrientation = server.arg("orientation");
      
      // Don't rotate screen if currently playing GIF/video
      // Just save the orientation preference for next media
      if (!currentFile.endsWith(".vid")) {
        setHardwareRotation(currentOrientation);
        loadOrder(currentOrientation);
        
        // If clock is currently showing, redraw it with new orientation
        if(isShowingClock) {
          forceClockRedraw();
        }
      }
    }
    if (server.hasArg("duration")) slideInterval = server.arg("duration").toInt() * 1000;
    server.send(200, "application/json", "{\"ok\":true}");
  });
  
  server.on("/api/clock/show", HTTP_GET, []() {
    // Stop any currently playing media (GIF/video)
    if (currentFile.endsWith(".vid")) {
      currentFile = "";
    }
    
    // Show clock in clock-only mode (respects clockOnlyScreen setting)
    isShowingClock = true;
    isSlideshow = false;
    isClockOnlyMode = clockOnlyScreen; // Use the setting to determine if duration applies
    clockStartTime = millis();
    photosSinceLastClock = 0;
    // Force immediate full redraw
    forceClockRedraw();
    server.send(200, "application/json", "{\"ok\":true}");
  });
  
  server.on("/api/sync", HTTP_GET, []() {
    // Force NTP time synchronization
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    // Wait a moment for sync to complete
    delay(100);
    
    // If clock is showing, force redraw with new time
    if(isShowingClock) {
      forceClockRedraw();
    }
    
    server.send(200, "application/json", "{\"ok\":true}");
  });
  
  server.on("/api/sync", HTTP_GET, []() {
    // Force NTP time synchronization
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    // Wait a moment for sync to complete
    delay(100);
    
    // If clock is showing, force redraw with new time
    if(isShowingClock) {
      forceClockRedraw();
    }
    
    server.send(200, "application/json", "{\"ok\":true}");
  });
  
  server.on("/api/sync", HTTP_GET, []() {
    // Force NTP time synchronization
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    // Wait a moment for sync to complete
    delay(100);
    
    // If clock is showing, force redraw with new time
    if(isShowingClock) {
      forceClockRedraw();
    }
    
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/clock/toggle", HTTP_POST, []() {
    if(server.hasArg("plain")) {
      DynamicJsonDocument doc(256);
      deserializeJson(doc, server.arg("plain"));
      clockEnabled = doc["enabled"] | false;
      // Duration is now managed through /api/clock/settings
    }
    String response = "{\"clockEnabled\":" + String(clockEnabled ? "true" : "false") + "}";
    server.send(200, "application/json", response);
  });

  server.on("/api/clock/config", HTTP_GET, []() {
    String response = "{\"clockEnabled\":" + String(clockEnabled ? "true" : "false") + 
                      ",\"clockDisplayDuration\":" + String(clockDisplayDuration) + 
                      ",\"clockPhotoInterval\":" + String(clockPhotoInterval) + "}";
    server.send(200, "application/json", response);
  });

  server.on("/upload", HTTP_POST, []() { server.send(200, "text/plain", "OK"); }, []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        isSlideshow = false; currentFile = ""; 
        String filename = upload.filename;
        if(filename.startsWith("/")) filename = filename.substring(1);
        String targetOrient = server.arg("targetOrient");
        if (targetOrient == "") targetOrient = "landscape";
        
        String lowerName = filename; lowerName.toLowerCase();
        // Check if this is a GIF thumbnail (gifType parameter) or a .vid file
        bool isGifRelated = lowerName.endsWith(".vid") || server.hasArg("gifType");
        String folder = isGifRelated ? "/gifs/" + targetOrient : "/photos/" + targetOrient;
        
        if (!SD.exists(folder)) SD.mkdir(folder);
        String path = folder + "/" + filename;
        if(SD.exists(path)) SD.remove(path);
        uploadFile = SD.open(path, FILE_WRITE);
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
      } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            loadOrder(currentOrientation); 
        }
      }
  });
  
  server.on("/api/photo/delete", HTTP_POST, []() {
      DynamicJsonDocument doc(512);
      deserializeJson(doc, server.arg("plain"));
      String f = doc["filename"];
      if(SD.exists(f)) SD.remove(f);
      loadOrder(currentOrientation); 
      server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/wifi", HTTP_POST, []() {
     DynamicJsonDocument doc(512);
     deserializeJson(doc, server.arg("plain"));
     saveWiFiConfig(doc["ssid"], doc["pass"]);
     server.send(200, "text/plain", "Rebooting");
     delay(500);
     ESP.restart();
  });

  server.on("/api/clock/settings", HTTP_POST, []() {
    if(server.hasArg("plain")) {
      DynamicJsonDocument doc(512);
      deserializeJson(doc, server.arg("plain"));
      
      // Update clock format (12hr/24hr)
      if(doc.containsKey("use24Hour")) {
        use24Hour = doc["use24Hour"] | false;
      }
      
      // Update timezone offset (in seconds, not hours)
      if(doc.containsKey("timezoneOffset")) {
        gmtOffset_sec = doc["timezoneOffset"].as<int>();
      }
      
      // Update timezone name
      if(doc.containsKey("timezoneName")) {
        timezoneName = doc["timezoneName"].as<String>();
      }
      
      // Update clock display duration (seconds)
      if(doc.containsKey("clockDisplayDuration")) {
        clockDisplayDuration = doc["clockDisplayDuration"].as<int>();
        Serial.print("Clock display duration set to: ");
        Serial.print(clockDisplayDuration);
        Serial.println(" seconds");
      }
      
      // Update clock photo interval (show every N photos)
      if(doc.containsKey("clockPhotoInterval")) {
        clockPhotoInterval = doc["clockPhotoInterval"].as<int>();
        Serial.print("Clock photo interval set to: ");
        Serial.print(clockPhotoInterval);
        Serial.println(" photos (0 = end of slideshow)");
      }
      
      // Update clock-only screen setting
      if(doc.containsKey("clockOnlyScreen")) {
        clockOnlyScreen = doc["clockOnlyScreen"].as<bool>();
        Serial.print("Clock-only screen: ");
        Serial.println(clockOnlyScreen ? "enabled" : "disabled");
      }
      
      // Update clock enabled setting
      if(doc.containsKey("clockEnabled")) {
        clockEnabled = doc["clockEnabled"].as<bool>();
        Serial.print("Clock enabled: ");
        Serial.println(clockEnabled ? "yes" : "no");
      }
      
      // Update clock show at end setting
      if(doc.containsKey("clockShowAtEnd")) {
        clockShowAtEnd = doc["clockShowAtEnd"].as<bool>();
        Serial.print("Clock show at end of slideshow: ");
        Serial.println(clockShowAtEnd ? "yes" : "no");
      }
      
      // Update slideshow bar position
      if(doc.containsKey("barPosition")) {
        barPosition = doc["barPosition"].as<String>();
        Serial.print("Slideshow bar position: ");
        Serial.println(barPosition);
      }
      
      // Update custom messages
      if(doc.containsKey("customMessages")) {
        customMessages = doc["customMessages"].as<String>();
        Serial.print("Custom messages updated: ");
        Serial.println(customMessages);
      }
      
      // Save to config file
      saveWiFiConfig(ssid, password);
      
      // Reconfigure NTP with new timezone
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      
      // Force immediate clock redraw if clock is showing
      if(isShowingClock) {
        forceClockRedraw();
      }
      
      server.send(200, "application/json", "{\"ok\":true}");
    } else {
      server.send(400, "application/json", "{\"error\":\"Missing configuration\"}");
    }
  });

  server.on("/api/clock/settings", HTTP_GET, []() {
    String json = "{";
    json += "\"use24Hour\":" + String(use24Hour ? "true" : "false") + ",";
    json += "\"timezoneOffset\":" + String(gmtOffset_sec) + ",";
    json += "\"timezoneName\":\"" + timezoneName + "\",";
    json += "\"clockDisplayDuration\":" + String(clockDisplayDuration) + ",";
    json += "\"clockPhotoInterval\":" + String(clockPhotoInterval) + ",";
    json += "\"clockOnlyScreen\":" + String(clockOnlyScreen ? "true" : "false") + ",";
    json += "\"clockEnabled\":" + String(clockEnabled ? "true" : "false") + ",";
    json += "\"clockShowAtEnd\":" + String(clockShowAtEnd ? "true" : "false") + ",";
    json += "\"barPosition\":\"" + barPosition + "\",";
    json += "\"customMessages\":\"" + customMessages + "\"";
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/api/clock/show", HTTP_POST, []() {
    // Show clock permanently (stops slideshow)
    isSlideshow = false;  // Stop slideshow
    isShowingClock = true;
    isClockOnlyMode = clockOnlyScreen;  // Use saved setting
    clockStartTime = millis();
    lastClockUpdate = millis();
    forceClockRedraw();
    server.send(200, "application/json", "{\"ok\":true,\"message\":\"Clock displayed\"}");
  });

  server.on("/api/clock/temp", HTTP_POST, []() {
    // Show clock temporarily (keeps slideshow running)
    isShowingClock = true;
    isClockOnlyMode = false;  // Force temporary mode (duration limit applies)
    clockStartTime = millis();
    lastClockUpdate = millis();
    photosSinceLastClock = 0; // Reset counter so slideshow resumes properly
    forceClockRedraw();
    server.send(200, "application/json", "{\"ok\":true,\"message\":\"Clock displayed temporarily\"}");
  });

  server.on("/api/durations", HTTP_POST, []() {
    if(server.hasArg("plain")) {
      DynamicJsonDocument doc(8192);
      deserializeJson(doc, server.arg("plain"));
      
      // Handle photo order saving
      if(doc.containsKey("photoOrder") && doc["photoOrder"] == "custom") {
        JsonArray customOrder = doc["customOrder"];
        if(customOrder) {
          Serial.println("[PHOTO ORDER] Saving custom order...");
          Serial.print("[PHOTO ORDER] Received ");
          Serial.print(customOrder.size());
          Serial.println(" files");
          
          // Save custom order to config file
          DynamicJsonDocument orderDoc(4096);
          JsonArray filesArray = orderDoc.createNestedArray("files");
          
          int validCount = 0;
          for(JsonVariant v : customOrder) {
            String filename = v.as<String>();
            Serial.print("[PHOTO ORDER] Processing: ");
            Serial.println(filename);
            
            // Ensure full path format
            if(!filename.startsWith("/")) {
              filename = "/photos/" + currentOrientation + "/" + filename;
              Serial.print("[PHOTO ORDER]   -> Converted to: ");
              Serial.println(filename);
            }
            
            if(SD.exists(filename)) {
              filesArray.add(filename);
              validCount++;
              Serial.println("[PHOTO ORDER]   -> Added to order");
            } else {
              Serial.println("[PHOTO ORDER]   -> File not found, skipped");
            }
          }
          
          Serial.print("[PHOTO ORDER] Saved ");
          Serial.print(validCount);
          Serial.print(" files for orientation: ");
          Serial.println(currentOrientation);
          
          saveOrderConfig(currentOrientation, orderDoc);
          loadOrder(currentOrientation);
          
          Serial.print("[PHOTO ORDER] Loaded order has ");
          Serial.print(mediaFiles.size());
          Serial.println(" files");
          
          // Reset to first photo in new order
          currentMediaIndex = 0;
          
          server.send(200, "application/json", "{\"success\":true,\"message\":\"Photo order saved\"}");
          return;
        }
      }
      
      // Handle clock/photo duration settings (legacy)
      if(doc.containsKey("clockDuration")) {
        clockDisplayDuration = doc["clockDuration"].as<int>() / 1000; // Convert ms to seconds
      }
      if(doc.containsKey("photoDuration")) {
        slideInterval = doc["photoDuration"].as<int>();
      }
      if(doc.containsKey("clockEnabled")) {
        clockEnabled = doc["clockEnabled"].as<bool>();
      }
      if(doc.containsKey("photosEnabled")) {
        // Just acknowledge, photos are always enabled
      }
      
      server.send(200, "application/json", "{\"success\":true}");
    } else {
      server.send(400, "application/json", "{\"error\":\"No data\"}");
    }
  });

  server.begin();
  loadOrder(currentOrientation);
}

void loop() {
  server.handleClient();

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
    // isClockOnlyMode is set based on clockOnlyScreen setting or manual activation
    if (!isClockOnlyMode && isSlideshow) {
      // Check if display duration expired (convert seconds to milliseconds)
      if (millis() - clockStartTime > (clockDisplayDuration * 1000)) {
        Serial.println("Clock duration expired - returning to slideshow");
        isShowingClock = false;
        // Force immediate next slide
        lastSlideTime = millis() - slideInterval;
      }
    }
    return; // Don't process other modes while showing clock
  }

  // If a raw video file is set, play it continuously until stopped
  if (!isSlideshow && currentFile.endsWith(".vid")) {
      playRawVideo(currentFile, true); 
  }
  // If in slideshow mode, cycle through photos
  else if (isSlideshow) {
      if (millis() - lastSlideTime > slideInterval) {
          // Only show clock if enabled
          if (clockEnabled) {
            // Check if it's time to show clock
            bool shouldShowClock = false;
            
            if (clockShowAtEnd || clockPhotoInterval == 0) {
              // Show clock at end of slideshow cycle (when we've shown all photos)
              if (photosSinceLastClock >= (int)mediaFiles.size()) {
                shouldShowClock = true;
                Serial.println("Showing clock at end of slideshow cycle");
              }
            } else if (clockPhotoInterval > 0) {
              // Show clock every N photos
              if (photosSinceLastClock >= clockPhotoInterval) {
                shouldShowClock = true;
                Serial.print("Showing clock after ");
                Serial.print(photosSinceLastClock);
                Serial.println(" photos");
              }
            }
            
            if (shouldShowClock) {
              isShowingClock = true;
              clockStartTime = millis();
              lastClockUpdate = millis();
              photosSinceLastClock = 0;
              forceClockRedraw();
            } else {
              showSlideshow();
              photosSinceLastClock++;
            }
          } else {
            // Clock disabled - just show slideshow
            showSlideshow();
          }
          lastSlideTime = millis();
      }
  }
}

void loadWiFiConfig() {
    if(SD.exists("/config/wifi.json")) {
        File f = SD.open("/config/wifi.json");
        DynamicJsonDocument doc(512);
        deserializeJson(doc, f);
        ssid = doc["ssid"].as<String>();
        password = doc["pass"].as<String>();
        
        // Load clock settings
        use24Hour = doc["use24Hour"] | false;
        gmtOffset_sec = doc["gmtOffset"] | (8 * 3600);
        timezoneName = doc["timezoneName"] | "UTC+8 Manila";
        clockDisplayDuration = doc["clockDisplayDuration"] | 30; // Default 30 seconds
        clockPhotoInterval = doc["clockPhotoInterval"] | 0; // Default 0 = show at end of slideshow
        clockOnlyScreen = doc["clockOnlyScreen"] | false; // Default off
        clockEnabled = doc["clockEnabled"] | true; // Default on
        clockShowAtEnd = doc["clockShowAtEnd"] | true; // Default on (show at end of slideshow)
        barPosition = doc["barPosition"] | "top"; // Default top
        customMessages = doc["customMessages"] | "Creating memories...\\nLife is beautiful!\\nEnjoy the moments!\\nYou're amazing!\\nNever give up!"; // Default messages
        
        f.close();
    }
}

void saveWiFiConfig(String s, String p) {
    File f = SD.open("/config/wifi.json", FILE_WRITE);
    DynamicJsonDocument doc(512);
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
    serializeJson(doc, f);
    f.close();
}

void setHardwareRotation(String orientation) {
    // The ST7796 is 320x480. Rotation 0 is portrait (320 wide), Rotation 1 is landscape (480 wide)
    if (orientation == "portrait") tft.setRotation(0); 
    else tft.setRotation(1); // Landscape
}

// --- HELPER FUNCTION: ROBUST TEXT RENDERING ---
// MANDATORY: Use this function for ALL system text rendering
// This ensures proper text display with correct datum and background clearing
// NOTE: Caller MUST set tft.setSwapBytes(false) before calling this function
void drawSystemText(const char* text, int16_t x, int16_t y, uint16_t color, uint8_t font) {
    tft.setTextColor(color, TFT_BLACK); // CRITICAL: Background color required to overwrite pixels
    tft.setTextSize(1);
    tft.setCursor(x, y);
    tft.print(text); // Use simple print with setCursor
}

// --- SPLASH SCREEN FUNCTION ---
// Displays CHRONOSYNC branding with smooth bold Montserrat font
void showSplashScreen() {
    Serial.println("=== Splash Screen Start ===");
    
    tft.fillScreen(TFT_BLACK);
    delay(100);
    
    // Enable swap bytes for bitmap rendering
    tft.setSwapBytes(true);
    
    Serial.println("Drawing title...");
    // Draw "CHRONOSYNC" bitmap - Bold Montserrat 60pt, centered
    int titleX = (480 - CHRONOSYNC_WIDTH) / 2;
    int titleY = 90;
    tft.pushImage(titleX, titleY, CHRONOSYNC_WIDTH, CHRONOSYNC_HEIGHT, 
                  &SplashTextBitmaps[CHRONOSYNC_OFFSET]);
    
    Serial.println("Drawing subtitle...");
    // Draw "Digital Memory Vault" bitmap - Montserrat Regular 28pt, centered
    int subtitleX = (480 - DIGITAL_MEMORY_VAULT_WIDTH) / 2;
    int subtitleY = 150;
    tft.pushImage(subtitleX, subtitleY, DIGITAL_MEMORY_VAULT_WIDTH, DIGITAL_MEMORY_VAULT_HEIGHT,
                  &SplashTextBitmaps[DIGITAL_MEMORY_VAULT_OFFSET]);
    
    Serial.println("Drawing status...");
    // Draw "Initializing..." - Cyan, Font 2
    tft.setSwapBytes(false);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TAILWIND_CYAN, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("Initializing...", 240, 250, 2);
    
    Serial.println("=== Splash Screen Complete ===");
    
    // Display for 3 seconds
    delay(3000);
}

void loadOrder(String orientation) {
    mediaFiles.clear();
    std::vector<String> savedOrder;
    String path = "/config/order_" + orientation + ".json";
    
    if(SD.exists(path)) {
        File f = SD.open(path);
        DynamicJsonDocument doc(4096);
        deserializeJson(doc, f);
        JsonArray arr = doc["files"];
        // Only load files from /photos/ folder for slideshow
        for(String s : arr) {
            if(SD.exists(s) && s.indexOf("/photos/") >= 0) {
                savedOrder.push_back(s);
            }
        }
        f.close();
    }

    std::vector<String> diskFiles;
    // ONLY scan photos folder for slideshow
    scanDirectory("/photos/" + orientation, diskFiles);

    for(String f : savedOrder) mediaFiles.push_back(f);
    for(String f : diskFiles) {
        bool alreadyIn = false;
        for(String s : savedOrder) { if(s == f) { alreadyIn = true; break; } }
        if(!alreadyIn) mediaFiles.push_back(f);
    }
    Serial.printf("Loaded %d files for %s\n", mediaFiles.size(), orientation.c_str());
}

void saveOrderConfig(String orientation, DynamicJsonDocument& doc) {
    String path = "/config/order_" + orientation + ".json";
    File f = SD.open(path, FILE_WRITE);
    serializeJson(doc, f);
    f.close();
}

// --- THE RAW VIDEO ENGINE (Optimized for Speed) ---
void playRawVideo(String filename, bool infiniteLoop) {
  File vFile = SD.open(filename);
  if (!vFile || vFile.isDirectory()) { currentFile = ""; return; }

  // 1. Calculate Frame and Line size based on current rotation
  int screenWidth = tft.width(); // 320 or 480
  int screenHeight = tft.height(); // 480 or 320
  
  // Bytes per frame: (Total Pixels) * 2 bytes/pixel (RGB565)
  size_t bytesPerFrame = screenWidth * screenHeight * 2;
  
  // 2. Use swapBytes TRUE - the .vid file is RGB565 Little Endian from JavaScript
  tft.setSwapBytes(true); 

  // 3. TRANSACTION START: Optimize bus - keep SPI bus active
  tft.startWrite();

  bool keepPlaying = true;
  unsigned long frameStart;
  unsigned long lastWebCheck = 0;
  const unsigned long WEB_CHECK_INTERVAL = 200; // Check web every 200ms instead of every frame
  
  // Target ~120 FPS for much smoother animation (8ms per frame)
  // Provides safety cap while allowing much faster playback
  const unsigned long TARGET_FRAME_TIME = 8;
  
  while (keepPlaying) {
      frameStart = millis();
      
      if (vFile.available() < bytesPerFrame) {
          if (infiniteLoop && !isSlideshow && currentFile == filename) {
              vFile.seek(0); 
          } else {
              keepPlaying = false; 
              break;
          }
      }
      
      // Set the write window for a full screen
      tft.setAddrWindow(0, 0, screenWidth, screenHeight);

      // Read and push frame in large chunks for maximum speed
      size_t totalRead = 0;
      while(totalRead < bytesPerFrame) {
          size_t toRead = min((size_t)LINE_BUFFER_SIZE, bytesPerFrame - totalRead);
          int bytesRead = vFile.read(lineBuffer, toRead);
          
          if (bytesRead <= 0) {
              keepPlaying = false;
              break; 
          }
          
          // Push pixels directly
          tft.pushColors((uint16_t*)lineBuffer, bytesRead / 2);
          totalRead += bytesRead;
      }
      
      // Only check web server periodically (not every frame) for better FPS
      if (millis() - lastWebCheck > WEB_CHECK_INTERVAL) {
          server.handleClient();
          lastWebCheck = millis();
          if (!isSlideshow && currentFile != filename) keepPlaying = false;
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

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

void scanDirectory(const String &path, std::vector<String> &outFiles) {
  File root = SD.open(path);
  if(!root || !root.isDirectory()) return;
  File file = root.openNextFile();
  while(file){
    if(!file.isDirectory()) {
        String n = String(file.name());
        if (!n.startsWith("/")) n = path + "/" + n;
        if(n.indexOf("._") == -1) outFiles.push_back(n);
    }
    file = root.openNextFile();
  }
}

void showSlideshow() {
  if (mediaFiles.empty()) return;
  
  currentMediaIndex = (currentMediaIndex + 1) % mediaFiles.size();
  String f = mediaFiles[currentMediaIndex];
  
  // Set rotation based on the file's folder (portrait/landscape)
  if(f.indexOf("/portrait/") >= 0) tft.setRotation(0); else tft.setRotation(1);
  
  if (f.endsWith(".vid")) {
      currentFile = f; // Set current file so loop() takes over
  } else if (f.endsWith(".jpg") || f.endsWith(".jpeg")) {
      tft.fillScreen(TFT_BLACK);
      tft.setSwapBytes(true);
      TJpgDec.drawSdJpg(0, 0, f);
  }
}

// --- CLOCK DISPLAY FUNCTIONS ---
// Shows clock with smooth Montserrat bold time, centered layout
void showClock() {
  // Preserve current orientation from photo/GIF mode
  // Only use clockOrientation setting when explicitly changed in settings
  // Keep current rotation as-is (don't force orientation change)
  tft.fillScreen(TFT_WHITE);
  tft.setSwapBytes(false); // Text mode
  
  updateClock(); // Draw initial clock display
}

// Force clock redraw (resets static tracking variables)
void forceClockRedraw() {
  // Use current universal orientation (no separate clock orientation)
  if(currentOrientation == "portrait") {
    tft.setRotation(0); // Portrait
  } else {
    tft.setRotation(1); // Landscape
  }
  // Clear the entire clock area to prevent glitches
  tft.fillScreen(TFT_WHITE);
  tft.setSwapBytes(false); // Text mode
  // Trigger immediate update with force flag to bypass static variable check
  updateClock(true);
}

// Updates clock display (called every second for live time)
void updateClock(bool forceRedraw) {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) {
    // No time sync - show error (only clear time area, not full screen)
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setTextSize(2);
    int centerX = (currentOrientation == "portrait") ? 160 : 240;
    int centerY = (currentOrientation == "portrait") ? 240 : 160;
    tft.fillRect(0, centerY - 30, (currentOrientation == "portrait") ? 320 : 480, 60, TFT_WHITE);
    tft.drawString("--:--", centerX, centerY, 4);
    tft.setTextSize(1);
    tft.drawString("No Time Sync", centerX, centerY + 40, 2);
    return;
  }
  
  // Draw time in 12-hour or 24-hour format (HH:MM only, no seconds)
  char timeStr[12];
  if(use24Hour) {
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo); // 24-hour format, no seconds
  } else {
    strftime(timeStr, sizeof(timeStr), "%I:%M", &timeinfo); // 12-hour format, no seconds
  }
  
  // Get AM/PM (only for 12-hour format)
  char ampmStr[3] = "";
  if(!use24Hour) {
    strftime(ampmStr, sizeof(ampmStr), "%p", &timeinfo);
  }
  
  // Track previous time to avoid redrawing when minute hasn't changed
  static String prevTimeStr = "";
  static String prevAmpmStr = "";
  static bool prevUse24Hour = use24Hour;
  static String prevOrientation = currentOrientation;
  
  String currentTimeStr = String(timeStr);
  String currentAmpmStr = String(ampmStr);
  
  // Only redraw if time changed or format changed (unless forced)
  if(!forceRedraw && currentTimeStr == prevTimeStr && currentAmpmStr == prevAmpmStr && 
     prevUse24Hour == use24Hour && prevOrientation == currentOrientation) {
    return; // No change, skip redraw to prevent blinking
  }
  
  // Update previous values
  prevTimeStr = currentTimeStr;
  prevAmpmStr = currentAmpmStr;
  prevUse24Hour = use24Hour;
  prevOrientation = currentOrientation;
  
  if(currentOrientation == "portrait") {
    // PORTRAIT LAYOUT: Vertical stacking with centered time, AM/PM next to minutes
    // Split time into hours and minutes
    char hoursStr[3], minsStr[3];
    hoursStr[0] = timeStr[0]; hoursStr[1] = timeStr[1]; hoursStr[2] = '\0';
    minsStr[0] = timeStr[3]; minsStr[1] = timeStr[4]; minsStr[2] = '\0';
    
    float timeScale = 1.5; // Larger time digits
    float ampmScale = 0.7; // Smaller AM/PM
    const int spacing = 6;
    
    // Calculate hours width
    int hoursW = 0;
    for(const char* p = hoursStr; *p; ++p) {
      const char* f = strchr(MontserratBoldClock56Chars, *p);
      if(f) {
        hoursW += (int)(pgm_read_byte(&MontserratBoldClock56Width[f - MontserratBoldClock56Chars]) * timeScale) + (int)(spacing * timeScale);
      }
    }
    
    // Calculate minutes width
    int minsW = 0;
    for(const char* p = minsStr; *p; ++p) {
      const char* f = strchr(MontserratBoldClock56Chars, *p);
      if(f) {
        minsW += (int)(pgm_read_byte(&MontserratBoldClock56Width[f - MontserratBoldClock56Chars]) * timeScale) + (int)(spacing * timeScale);
      }
    }
    
    // Calculate AM/PM width with smaller scale
    int ampmW = 0;
    if(!use24Hour) {
      for(const char* p = ampmStr; *p; ++p) {
        const char* f = strchr(MontserratBoldAMPMChars, *p);
        if(f) {
          ampmW += (int)(pgm_read_byte(&MontserratBoldAMPMWidth[f - MontserratBoldAMPMChars]) * ampmScale) + 2;
        }
      }
    }
    
    // Portrait layout positions (320x480 screen)
    // In 12-hour mode, shift everything 10px left to balance AM/PM visual weight
    // In 24-hour mode, use centered alignment
    int leftShift = use24Hour ? 23 : 33; // 24hr: 23px left, 12hr: 33px left (10px more)
    int leftMargin = (320 - hoursW) / 2 - leftShift;
    int hoursX = leftMargin;
    int hoursY = 105; // Moved up 10px more (was 115)
    
    // Align minutes with hours, add more spacing between hour and min
    int minsX = leftMargin;
    int minsY = 220; // Increased spacing from hours (was 210)
    
    // Position AM/PM to the right of minutes with spacing
    int ampmSpacing = 12; // Space between minutes and AM/PM
    int ampmX = minsX + minsW + ampmSpacing;
    int ampmY = minsY + (int)(50 * timeScale); // Adjust vertical position for scaled AM/PM
    
    // Calculate date position with more spacing from minutes
    int dateY = minsY + (int)(65 * timeScale) + 25; // Increased spacing (was 12px) below minutes
    
    // Clear time area
    tft.fillRect(0, hoursY - 10, 320, 240, TFT_WHITE);
    
    // Draw hours (left-aligned, larger)
    tft.setSwapBytes(true);
    drawMontDigitString(hoursX, hoursY, hoursStr, spacing, TFT_BLACK, TFT_WHITE, timeScale);
    
    // Draw minutes (aligned with hours, larger)
    drawMontDigitString(minsX, minsY, minsStr, spacing, TFT_BLACK, TFT_WHITE, timeScale);
    
    // Draw AM/PM to the right of minutes (smaller)
    if(!use24Hour) {
      drawAMPMString(ampmX, ampmY, ampmStr, 2, TFT_BLACK, TFT_WHITE, ampmScale);
    }
    tft.setSwapBytes(false);
    
    // Draw date: Mon, Dec 08, 2025 (12px spacing from minutes)
    char dateStr[40];
    strftime(dateStr, sizeof(dateStr), "%a, %b %d, %Y", &timeinfo);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setTextSize(1);
    tft.drawString(dateStr, 160, dateY, 4);
    
    // Draw timezone at bottom
    tft.drawString(timezoneName, 160, 420, 4);
    
    // Draw IP address in top right corner (small black text)
    String ipAddr = WiFi.localIP().toString();
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setTextSize(1);
    tft.drawString(ipAddr, 310, 10, 2);
    
    // Draw motivational message in top left corner (small black text) - same as slideshow bar
    String currentMessage;
    if (!customMessages.isEmpty() && customMessages != "Creating memories...\\nLife is beautiful!\\nEnjoy the moments!\\nYou're amazing!\\nNever give up!") {
      // Parse custom messages for clock display
      std::vector<String> clockMessages;
      String temp = customMessages;
      temp.replace("\\n", "\n");
      int start = 0;
      int end = temp.indexOf('\n');
      while (end != -1) {
        clockMessages.push_back(temp.substring(start, end));
        start = end + 1;
        end = temp.indexOf('\n', start);
      }
      if (start < temp.length()) {
        clockMessages.push_back(temp.substring(start));
      }
      if (!clockMessages.empty()) {
        int msgIndex = (millis() / 5000) % clockMessages.size();
        currentMessage = clockMessages[msgIndex];
      } else {
        currentMessage = encouragementMessages[(millis() / 5000) % messageCount];
      }
    } else {
      currentMessage = encouragementMessages[(millis() / 5000) % messageCount];
    }
    tft.setTextDatum(TL_DATUM);
    tft.drawString(currentMessage, 10, 10, 2);
    
  } else {
    // LANDSCAPE LAYOUT: Original horizontal layout with larger size and more spacing
    // Calculate width for centering (larger scale)
    float timeScale = 1.2; // Increased from 1.0 for larger digits
    int totalW = 0;
    const int spacing = 12; // Increased spacing between digits and colon (was 6)
    for(const char* p = timeStr; *p; ++p) {
      const char* f = strchr(MontserratBoldClock56Chars, *p);
      if(f) {
        totalW += (int)(pgm_read_byte(&MontserratBoldClock56Width[f - MontserratBoldClock56Chars]) * timeScale) + (int)(spacing * timeScale);
      }
    }
    
    // Add AM/PM width to total (smaller 40pt font)
    int ampmW = 0;
    if(!use24Hour) {
      for(const char* p = ampmStr; *p; ++p) {
        const char* f = strchr(MontserratBoldAMPMChars, *p);
        if(f) {
          ampmW += (int)(pgm_read_byte(&MontserratBoldAMPMWidth[f - MontserratBoldAMPMChars]) * timeScale) + 2;
        }
      }
    }
    
    // Center entire time + AM/PM together
    int combinedW = totalW + (use24Hour ? 0 : 10 + ampmW);
    int timeX = (480 - combinedW) / 2;
    int timeY = 90; // Moved up slightly for larger text
    
    // Clear time area before redrawing (larger area for bigger font)
    tft.fillRect(0, timeY - 10, 480, 110, TFT_WHITE);
    
    // Draw time with anti-aliased Montserrat bitmaps (large scaled font)
    tft.setSwapBytes(true); // Image mode for bitmaps
    drawMontDigitString(timeX, timeY, timeStr, spacing, TFT_BLACK, TFT_WHITE, timeScale);
    
    // Draw AM/PM with smaller bitmap font (only in 12-hour mode)
    if(!use24Hour) {
      // Position AM/PM slightly lower to align with time baseline
      drawAMPMString(timeX + totalW + 10, timeY + 45, ampmStr, 2, TFT_BLACK, TFT_WHITE, timeScale);
    }
    tft.setSwapBytes(false); // Back to text mode
    
    // Draw date with year (black, centered)
    char dateStr[40];
    strftime(dateStr, sizeof(dateStr), "%A, %B %d, %Y", &timeinfo);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setTextSize(1);
    tft.drawString(dateStr, 240, 210, 4); // Adjusted position for larger time
    
    // Draw timezone at bottom (black, centered) - increased font size
    tft.setTextSize(1);
    tft.drawString(timezoneName, 240, 280, 4); // Changed from font 2 to font 4
    
    // Draw IP address in top right corner (small black text)
    String ipAddr = WiFi.localIP().toString();
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setTextSize(1);
    tft.drawString(ipAddr, 470, 10, 2);
    
    // Draw motivational message in top left corner (small black text) - same as slideshow bar
    String currentMessage;
    if (!customMessages.isEmpty() && customMessages != "Creating memories...\\nLife is beautiful!\\nEnjoy the moments!\\nYou're amazing!\\nNever give up!") {
      // Parse custom messages for clock display
      std::vector<String> clockMessages;
      String temp = customMessages;
      temp.replace("\\n", "\n");
      int start = 0;
      int end = temp.indexOf('\n');
      while (end != -1) {
        clockMessages.push_back(temp.substring(start, end));
        start = end + 1;
        end = temp.indexOf('\n', start);
      }
      if (start < temp.length()) {
        clockMessages.push_back(temp.substring(start));
      }
      if (!clockMessages.empty()) {
        int msgIndex = (millis() / 5000) % clockMessages.size();
        currentMessage = clockMessages[msgIndex];
      } else {
        currentMessage = encouragementMessages[(millis() / 5000) % messageCount];
      }
    } else {
      currentMessage = encouragementMessages[(millis() / 5000) % messageCount];
    }
    tft.setTextDatum(TL_DATUM);
    tft.drawString(currentMessage, 10, 10, 2);
  }
}