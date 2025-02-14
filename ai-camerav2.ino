/*
  ESP32-CAM Image Analysis with OpenAI API and OLED Display

  This code captures an image using the ESP32-CAM module, processes it, 
  and sends it to OpenAI's GPT-4o API for analysis. The API's response is 
  displayed on an OLED screen. The code also provides audio feedback using 
  a buzzer and includes features like Wi-Fi connectivity, image encoding, 
  and scrolling text display.

  Tested with:
  - Arduino IDE version 2.3.2
  - ESP32 boards package version 3.0.0
  - Adafruit GFX library version 1.11.11
  - Adafruit SSD1306 library version 2.5.13
  - ArduinoJson library version 7.1.0
  - Base64 library (default version with ESP32 boards package)

  Make sure to install these libraries and configure your environment 
  as specified above before running the code.
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <Base64.h>
#include "esp_camera.h"
#include <Adafruit_GFX.h>  
#include <Wire.h>
#include <Adafruit_SSD1306.h> 
#include <ArduinoJson.h>
#include <U8g2lib.h>

const char* ssid = "wifi_name";
const char* password = "wifi_pass";

// OpenAI API key
const String apiKey = "openai_key";


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

#define SUMMARIZE_BUTTON_PIN 13
#define TRANSLATE_BUTTON_PIN 1
#define SOLVE_BUTTON_PIN 3
#define BUZZER_PIN 2  // Buzzer connected to GPIO2

void displayCenteredText(const String& text, int textSize = 1) {
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
// Function to encode image to Base64
String encodeImageToBase64(const uint8_t* imageData, size_t imageSize) {
  return base64::encode(imageData, imageSize);
}
void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  pinMode(SUMMARIZE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(TRANSLATE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SOLVE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);  // Set Buzzer pin as output

  display.begin();
  displayCenteredText("AI Camera", 1);
  delay(3000);  // Hold the title screen for 3 seconds

  displayCenteredText("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  displayCenteredText("WiFi Connected!");
  delay(2000);

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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    displayCenteredText("Camera Init Failed");
    return;
  }

  displayCenteredText("Camera Initialized");
  delay(2000);

  displayCenteredText("Press button to capture");
}


void captureAndAnalyzeImage(const String& prompt, const String& displayText) {
  Serial.println("Capturing image...");

  // Capture the image frame buffer
  camera_fb_t* fb = esp_camera_fb_get();  // Get the frame buffer
  if (!fb) {
    Serial.println("Camera capture failed");
    displayCenteredText("Capture Failed");
    return;
  }

  // After the new frame is obtained, ensure the buffer is returned (cleared)
  esp_camera_fb_return(fb);  // Release the frame buffer from the previous capture

  // Now, capture the new image
  fb = esp_camera_fb_get();  // Get the frame buffer again for the new image

  if (!fb) {
    Serial.println("Camera capture failed");
    displayCenteredText("Capture Failed");
    return;
  }

  Serial.println("Image captured");
  String base64Image = encodeImageToBase64(fb->buf, fb->len);

  beep();
  // Return the frame buffer after processing the image
  esp_camera_fb_return(fb);  // Return the frame buffer to free memory

  if (base64Image.isEmpty()) {
    Serial.println("Failed to encode the image!");
    displayCenteredText("Encode Failed");
    return;
  }
  // Send the image to OpenAI for analysis with the specified prompt
  AnalyzeImage(base64Image, prompt, displayText);

 
}

void AnalyzeImage(const String& base64Image, const String& prompt,  const String& displayText) {
  Serial.println("Sending image for analysis...");
   displayCenteredText(displayText);  // Display the appropriate text

  String result;

  // Prepare the payload for the OpenAI API
  String url = "data:image/jpeg;base64," + base64Image;
  Serial.println(url);

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

  // Send request and validate response
  if (sendPostRequest(jsonPayload, result)) {
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
    int maxLineChars = 21;  // Approx. max characters per line
    int visibleLines = 7;
    int scrollDelay = 2000;  // Delay for scrolling in milliseconds

    std::vector<String> lines;  // Store formatted lines for display

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
        display.drawStr(0, j * lineHeight, lines[i + j].c_str());
      }
      display.sendBuffer();
      delay(scrollDelay);
    }

    // Clear display after the response
    display.clearBuffer();
    display.sendBuffer();

    displayCenteredText("Press button to capture");
  } else {
    Serial.print("[ChatGPT] Error: ");
    Serial.println(result);
    display.clearBuffer();
    display.drawStr(0, 0, "API Error");
    display.sendBuffer();
  }
}

bool sendPostRequest(const String& payload, String& result) {
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


// Remaining code remains unchanged...

void loop() {
  if (digitalRead(SUMMARIZE_BUTTON_PIN) == LOW) {
    Serial.println("Summarize button pressed! Capturing image...");
    captureAndAnalyzeImage("Summarize the image", "Summarizing...");
    delay(1000);  // Small delay to debounce button press
  } else if (digitalRead(TRANSLATE_BUTTON_PIN) == LOW) {
    Serial.println("Translate button pressed! Capturing image...");
    captureAndAnalyzeImage("Translate the text in the image to English. Display only the translated text in the output.", "Translating...");
    delay(1000);  // Small delay to debounce button press
  } else if (digitalRead(SOLVE_BUTTON_PIN) == LOW) {
    Serial.println("Solve button pressed! Capturing image...");
    captureAndAnalyzeImage("Solve the math problem in the image. In the output show very very very short solution or just the answer.", "Solving...");
    delay(1000);  // Small delay to debounce button press
  }
}
void beep(){
  digitalWrite(2,HIGH);
  delay(300);
  digitalWrite(2,LOW);
  
}
