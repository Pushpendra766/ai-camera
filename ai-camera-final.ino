#include <WiFi.h>
#include <HTTPClient.h>
#include <Base64.h>
#include "esp_camera.h"
#include <Adafruit_GFX.h>  
#include <Wire.h>
#include <Adafruit_SSD1306.h> 
#include <ArduinoJson.h>
#include <U8g2lib.h>

const char* ssid = "<WIFI-SSID>";
const char* password = "<WIFI-PASSWORD>";
const String apiKey = "<OPENAI-API-KEY>";


// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SCL 14
#define OLED_SDA 15
#define OLED_RESET -1

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ OLED_SCL, /* data=*/ OLED_SDA);

// Pin definitions for ESP32-CAM AI-Thinker module
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Input pins
#define SUMMARIZE_BUTTON_PIN 13
#define TRANSLATE_BUTTON_PIN 1
#define SOLVE_BUTTON_PIN 3
#define BUZZER_PIN 2

void showText(const String& text, int textSize = 1) {
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tf); // Choose a suitable font

  int maxLineLength = 16;  // Assuming 16 characters fit per line at textSize 1
  String lineBuffer = "";
  String wordBuffer = "";
  int lineHeight = 10; // Adjust based on font size

  // Calculate the total number of lines needed
  int lineCount = 0;
  for (size_t i = 0; i <= text.length(); i++) {
    char c = text.charAt(i);
    if (c == ' ' || c == '\n' || c == '\0') {
      if (lineBuffer.length() + wordBuffer.length() > maxLineLength) {
        lineCount++;
        lineBuffer = wordBuffer;
      } else {
        lineBuffer += (lineBuffer.isEmpty() ? "" : " ") + wordBuffer;
      }
      wordBuffer = "";

      if (c == '\n') {
        lineCount++;
        lineBuffer = "";
      }
    } else {
      wordBuffer += c;
    }
  }
  if (!lineBuffer.isEmpty()) lineCount++;  // Count the last line

  // Calculate the vertical offset to center the block of text
  int totalTextHeight = lineCount * lineHeight;
  int yOffset = (SCREEN_HEIGHT - totalTextHeight) / 2;

  // Render the text line by line, vertically centered
  int yPos = yOffset;
  lineBuffer = "";
  wordBuffer = "";
  for (size_t i = 0; i <= text.length(); i++) {
    char c = text.charAt(i);
    if (c == ' ' || c == '\n' || c == '\0') {
      if (lineBuffer.length() + wordBuffer.length() > maxLineLength) {
        // Render the current line
        display.drawStr((SCREEN_WIDTH - lineBuffer.length() * 6) / 2, yPos, lineBuffer.c_str());
        yPos += lineHeight;
        lineBuffer = wordBuffer;
      } else {
        lineBuffer += (lineBuffer.isEmpty() ? "" : " ") + wordBuffer;
      }
      wordBuffer = "";

      if (c == '\n' || c == '\0') {
        display.drawStr((SCREEN_WIDTH - lineBuffer.length() * 6) / 2, yPos, lineBuffer.c_str());
        yPos += lineHeight;
        lineBuffer = "";
      }
    } else {
      wordBuffer += c;
    }
  }

  display.sendBuffer();
}

void playSound() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(300);
  digitalWrite(BUZZER_PIN, LOW);
}

void initializeCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;  // Fix: Change SIOD to sccb_sda
  config.pin_sscb_scl = SIOC_GPIO_NUM;  // Fix: Change SIOC to sccb_scl
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;  // Start with higher resolution
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  // Initialize camera with error checking
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    showText("Camera Init Failed");
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
  s->set_framesize(s, FRAMESIZE_QVGA);
}

void initializeSystem() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);  // Add debug output
  WiFi.begin(ssid, password);

  pinMode(SUMMARIZE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(TRANSLATE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SOLVE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);  // Set Buzzer pin as output

  display.begin();
  showText("AI Camera");
  delay(3000);  // Hold the title screen for 3 seconds

  showText("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
  }

  showText("WiFi Connected!");
  delay(2000);

  initializeCamera();
  showText("Camera Ready");
  delay(2000);
  showText("Press button");
}

camera_fb_t* captureImage() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Capture failed");
    showText("Capture Failed");
    return nullptr;
  }
  return fb;
}

void processImage(const String& prompt, const String& statusText) {
  Serial.println("Capturing...");

  // Clear previous capture
  camera_fb_t* fb = captureImage();
  if (!fb) return;
  esp_camera_fb_return(fb);

  // Capture new image
  fb = captureImage();
  if (!fb) return;

  String base64Image = base64::encode(fb->buf, fb->len);
  playSound();
  esp_camera_fb_return(fb);

  if (base64Image.isEmpty()) {
    Serial.println("Encoding failed");
    showText("Encode Failed");
    return;
  }

  analyzeWithAI(base64Image, prompt, statusText);
}

void analyzeWithAI(const String& base64Image, const String& prompt, const String& statusText) {
  Serial.println("Sending image for analysis...");
  showText(statusText);  // Display the appropriate text

  String result;

  // Prepare the payload for the OpenAI API
  String url = "data:image/jpeg;base64," + base64Image;
  Serial.println(url);

  display.clearBuffer();
  showText("URL Generated");
  display.sendBuffer();

  DynamicJsonDocument doc(4096);
  doc["model"] = "gpt-4o";
  JsonArray messages = doc.createNestedArray("messages");
  JsonObject message = messages.createNestedObject();
  message["role"] = "user";
  JsonArray content = message.createNestedArray("content");
  JsonObject textContent = content.createNestedObject();
  textContent["type"] = "text";
  textContent["text"] = prompt;

  JsonObject imageContent = content.createNestedObject();
  imageContent["type"] = "image_url";
  JsonObject imageUrlObject = imageContent.createNestedObject("image_url");
  imageUrlObject["url"] = url;
  imageContent["image_url"]["detail"] = "auto";

  doc["max_tokens"] = 400;

  String jsonPayload;
  serializeJson(doc, jsonPayload);
  
  display.clearBuffer();
  showText("Calling API");
  display.sendBuffer();

  // Send request and validate response
  if (makeAPIRequest(jsonPayload, result)) {
  
    display.clearBuffer();
    showText("Got response from API");
    display.sendBuffer();

    Serial.print("[ChatGPT] Response: ");
    Serial.println(result);

    // Clear the display before showing the new response
    display.clearBuffer();
    display.sendBuffer();

    DynamicJsonDocument responseDoc(4096);
    deserializeJson(responseDoc, result);

    String responseContent = responseDoc["choices"][0]["message"]["content"].as<String>();
    Serial.println("[ChatGPT] Parsed response: " + responseContent);

    // Smooth scrolling and proper word wrapping
    display.clearBuffer();
    int lineHeight = 8;     // Height of each line in pixels
    int maxLineChars = 20;  // Approx. max characters per line
    int visibleLines = 8;
    int scrollDelay = 2000;  // Delay for scrolling in milliseconds

    std::vector<String> lines;  // Store formatted lines for display
    display.clearBuffer();
    showText("Started printing the response");
    display.sendBuffer();
    // Split responseContent into words for word wrapping
    String word = "";
    String currentLine = "";

    for (int i = 0; i < responseContent.length(); i++) {
      char c = responseContent.charAt(i);
      if (c == ' ' || c == '\n') {
        if (currentLine.length() + word.length() <= maxLineChars) {
          currentLine += (currentLine.isEmpty() ? "" : " ") + word;
        } else {
          lines.push_back(currentLine);
          currentLine = word;
        }
        word = "";
      } else {
        word += c;
      }
    }
    if (!currentLine.isEmpty()) lines.push_back(currentLine);
    if (!word.isEmpty()) lines.push_back(word);

    // Display lines with scrolling effect
    for (size_t i = 0; i < lines.size(); i++) {
      display.clearBuffer();
      for (size_t j = 0; j < visibleLines && (i + j) < lines.size(); j++) {
        display.drawStr(0, (j * lineHeight) + 10, lines[i + j].c_str());  // Added +10 pixels offset
      }
      display.sendBuffer();
      delay(scrollDelay);
    }
    display.clearBuffer();
    showText("Printing ended");
    display.sendBuffer();
    // Clear display after the response
    display.clearBuffer();
    display.sendBuffer();

    showText("Press button to capture");
  } else {

     display.clearBuffer();
    showText("CHATGPT Error");
    display.sendBuffer();

    Serial.print("[ChatGPT] Error: ");
    Serial.println(result);
    display.clearBuffer();
     showText("API Error");
    display.sendBuffer();
  }
}

bool makeAPIRequest(const String& payload, String& result) {
  HTTPClient http;
  http.begin("https://api.openai.com/v1/chat/completions");

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + apiKey);
  http.setTimeout(20000);

  Serial.print("Payload size: ");
  Serial.println(payload.length());

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    result = http.getString();
    Serial.println("HTTP Response Code: " + String(httpResponseCode));
    Serial.println("Response Body: " + result);
    http.end();
    return true;
  } else {
    result = "HTTP request failed, response code: " + String(httpResponseCode);
    Serial.println("Error Code: " + String(httpResponseCode));
    Serial.println("Error Message: " + http.errorToString(httpResponseCode));
    http.end();
    return false;
  }
}

void handleButtons() {
  if (digitalRead(SUMMARIZE_BUTTON_PIN) == LOW) {
    processImage("Summarize the image", "Summarizing...");
  } else if (digitalRead(TRANSLATE_BUTTON_PIN) == LOW) {
    processImage("Translate the text to English", "Translating...");
  } else if (digitalRead(SOLVE_BUTTON_PIN) == LOW) {
    processImage("Describe the weather", "Analyzing...");
  }
  delay(1000); // Debounce
}

void setup() {
  initializeSystem();
}

void loop() {
  handleButtons();
}
