#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

struct WifiCredentials {
  char ssid[33];
  char password[33];
};
const int MAX_NETWORKS = 30;
WifiCredentials networks[MAX_NETWORKS];

int counter = 0;
int currentNetworkIndex = 0;  

ESP8266WebServer server(80);
WiFiClientSecure client;
HTTPClient http;

const char* ssid = "NodeMCU"; 
const char* password = "12345678";
String target_ssid = "";
String target_password = "";

const int amount_of_currencies = 7;
String currencies[amount_of_currencies] = {"dogecoin", "shiba-inu", "bitcoin", "ethereum", "binance-coin", "xrp", "solana"};

void clearEEPROM() {
  EEPROM.begin(512);
  for (int i = 0 ; i < 512 ; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}

void saveWiFiCredentials(const String& ssid, const String& password, int networkIndex) {
  EEPROM.begin(512); 
  ssid.toCharArray(networks[networkIndex].ssid, 33);
  password.toCharArray(networks[networkIndex].password, 33);
  EEPROM.put(networkIndex * sizeof(WifiCredentials), networks[networkIndex]);
  EEPROM.put(500, networkIndex);
  EEPROM.commit(); 
}

void readWiFiCredentials() {

  EEPROM.begin(512);
  for (int i = 0; i < MAX_NETWORKS; ++i) {
    EEPROM.get(i * sizeof(WifiCredentials), networks[i]);
  }

  for (int i = 0; i < MAX_NETWORKS; ++i) {
  if (strlen(networks[i].ssid) > 0) {
    WiFi.begin(networks[i].ssid, networks[i].password);
    if (WiFi.waitForConnectResult() == WL_CONNECTED) {
      break;
      }
    }
  }

}

void handleRoot() {
  String html = "<html><body>";
  html += "<h1>Connecting cryptodisplay to WiFi</h1>";
  html += "<form action=\"/submit\" method=\"get\">";
  html += "SSID: <input type=\"text\" name=\"ssid\"><br>";
  html += "Password: <input type=\"password\" name=\"password\"><br>";
  html += "<input type=\"submit\" value=\"Connect\">";
  html += "</form>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleSubmit() {
  target_ssid = server.arg("ssid");
  target_password = server.arg("password");

  WiFi.begin(target_ssid.c_str(), target_password.c_str());

  if (WiFi.waitForConnectResult() == WL_CONNECTED) {
    if (currentNetworkIndex < MAX_NETWORKS - 1) { 
      currentNetworkIndex++;
    } else {
      currentNetworkIndex = 0;  
    }
    saveWiFiCredentials(target_ssid, target_password, currentNetworkIndex);
    server.send(200, "text/plain", "Connected!");
  } else {
    server.send(500, "text/plain", "Failed to connect to WiFi. Please try again.");
  }
}

void startServer()
{
    clearEEPROM();
    WiFi.softAP(ssid, password);
    server.on("/", handleRoot);
    server.on("/submit", handleSubmit);
    server.begin();
    while (true)
      server.handleClient();
}

void checkWiFiStatus()
{
  if (WiFi.status() != WL_CONNECTED) {
      WiFi.softAP(ssid, password);
      server.on("/", handleRoot);
      server.on("/submit", handleSubmit);
      server.begin();
      while (WiFi.status() != WL_CONNECTED) server.handleClient();
      server.send(200, "text/plain", "Connected to WiFi!");
    }
}

void reconnect()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Error");
  readWiFiCredentials();
  server.handleClient(); 
}

void startPicture()
{
  EEPROM.begin(512);
  EEPROM.get(500, currentNetworkIndex); 
  lcd.init();
  lcd.backlight();
  if (currentNetworkIndex == 0)
    lcd.print("First Start");
  else
    lcd.print("Upload");
}

String makeRequest(String currency)
{
  client.setInsecure();

  http.begin(client, "https://api.coincap.io/v2/assets/" + currency);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    http.end();
    return doc["data"]["priceUsd"].as<String>();

  } else {
    return "error";
  }
}

void show_currency()
{
  String coin = currencies[counter++ % amount_of_currencies];
  String result = makeRequest(coin);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(coin);
  lcd.setCursor(0, 1);
  lcd.print("Price: ");
  lcd.print(result.substring(0, 10));
}

void setup() {
  //clearEEPROM();
  startPicture();
  readWiFiCredentials();
  checkWiFiStatus();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
  reconnect();
  } else {
  show_currency();
  delay(5000);
  }
}
