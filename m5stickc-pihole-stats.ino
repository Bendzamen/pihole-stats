#include "M5StickC.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> // JSON decoding library
#include "SimpleDebouncer.h"  // button debouncer library https://github.com/Bendzamen/SimpleDebouncer

const char WIFI_SSID[] = "your-ssid";
const char WIFI_PASSWORD[] = "your-password";

const int disable_sec = 599;  //disable for x seconds
String api_key = "your-pihole-api-key";  // PiHole API key

String summary_reqst = "http://pi.hole/admin/api.php?summary&auth=" + api_key;  // summary http request
String tmp_disable = "http://pi.hole/admin/api.php?disable=" + String(disable_sec) + "&auth=" + api_key;  //  disable for x sec PiHole http request
String disable = "http://pi.hole/admin/api.php?disable&auth=" + api_key;  // disable PiHole http request
String enable = "http://pi.hole/admin/api.php?enable&auth=" + api_key;  // enable PiHole http request

unsigned long stats_last_update = 0;  // last time when stats were updated
unsigned long stats_update_delay = 5000;  // delay between stats updates
unsigned long disabled_timer = 0;  // timer of disabled PiHole sec left 
unsigned long timer_last_update = 0;  // last time when timer was updated

const int main_button = 37;
const int side_button = 39;

SimpleDebouncer MainButton(main_button);
SimpleDebouncer SideButton(side_button);

bool enabled = true;


void setup() {
  Serial.begin(115200);
  Serial.println("Starting");
  M5.begin(); // initialize the M5StickC
  M5.Axp.ScreenBreath(10);
  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("PiHole api address ");
  Serial.println(summary_reqst);
  updateStats();
  stats_last_update = millis();
  MainButton.setup();
  SideButton.setup();
}


void loop() {
  MainButton.process();
  SideButton.process();
  // main button press
  if (MainButton.pressed()){
    Serial.println("main pressed");
    changeStatusPiHole(tmp_disable);
    disabled_timer = disable_sec * 1000;
    countdown();
    enabled = false;
  }

  // side button press
  if (SideButton.pressed()){
    Serial.println("side pressed");
    if (enabled){
      changeStatusPiHole(disable);
      disabled_timer = 0;
      enabled = false;
    }
    else{
      changeStatusPiHole(enable);
      disabled_timer = 0;
      enabled = true;
    }
  }

  // stats update every x milliseconds
  // prevents wild flickering and excessive amount of web requests
  if ((millis() - stats_last_update) > stats_update_delay) {
    updateStats();
    stats_last_update = millis();
  }

  if (disabled_timer > 0){
    countdown();
  }
}


void changeStatusPiHole(String resorc) {
  HTTPClient http;
  Serial.println(resorc);
  http.begin(resorc); // send request
  int httpCode = http.GET();
  Serial.println(httpCode);
  http.end();
  updateStats();  // update statistics
  stats_last_update = millis();
}


void updateStats(){
  HTTPClient http;
  http.begin(summary_reqst); //HTTP
  int httpCode = http.GET();
  // httpCode will be negative on error
  if(httpCode > 0) {
    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString(); //Get the request response payload
      DynamicJsonDocument root(2048);
      Serial.println(payload);
      auto error = deserializeJson(root, payload);
      if (error) {
        Serial.println("Parsing failed!");
        return;
      }

      // extract all the necessary data
      String dns_queries_all = root["dns_queries_all_replies"];
      String dns_queries_today = root["dns_queries_today"];
      String ads_blocked_today = root["ads_blocked_today"];
      String ads_percent_today = root["ads_percentage_today"];
      String status = root["status"];

      Serial.println(dns_queries_all);
      Serial.println(dns_queries_today);
      Serial.println(ads_blocked_today);
      Serial.println(ads_percent_today);
      Serial.println(status);

      // screen update
      if (status == "enabled"){
        M5.Lcd.fillScreen(GREEN);
        enabled = true;
      }
      else{
        M5.Lcd.fillScreen(RED);
        enabled = false;
      }
      M5.Lcd.setTextColor(BLACK);
      M5.Lcd.setTextSize(2);
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.print("Total ");
      M5.Lcd.println(dns_queries_all);
      M5.Lcd.setCursor(0, 20);
      M5.Lcd.print("Block ");
      M5.Lcd.println(ads_blocked_today);
      M5.Lcd.setCursor(0, 40);
      M5.Lcd.print("Ads ");
      M5.Lcd.print(ads_percent_today);
      M5.Lcd.println("%");
      M5.Lcd.setCursor(0, 60);
      M5.Lcd.println(status);

    } else {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    M5.Lcd.fillScreen(RED);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(30, 2);
    M5.Lcd.print("HTTP");
    M5.Lcd.setCursor(30, 25);
    M5.Lcd.print("error");
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(5, 60);
    M5.Lcd.print("Check PiHole");
  }
  http.end();
}

void countdown(){
  if ((millis()-timer_last_update)>1000){
    M5.Lcd.setCursor(110, 60);
    M5.Lcd.fillRect(100, 60, 60, 20, RED);
    int min = (disabled_timer/1000) / 60;
    int sec = (disabled_timer/1000) % 60;
    M5.Lcd.print(min);
    M5.Lcd.print(":");
    if (sec < 10){
      M5.Lcd.print("0");
    }
    M5.Lcd.println(sec);
    timer_last_update = millis();
    disabled_timer = disabled_timer - 1000;
  }
}