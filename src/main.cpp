#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <LiquidCrystal.h>
#include <DS18B20.h>
// #include <EasyButton.h>

#include <WiFiManager.h>
WiFiManager wifiManager;

#define WEBSERVER_H
#include <ESPAsyncWebServer.h>

#include <DNSServer.h>

#ifdef ESP32
  #include <WiFi.h>
  #include <AsyncTCP.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#endif

#define LCD_rs        D0
#define LCD_d4        D1
#define LCD_d5        D2
#define LCD_d6        D3
#define LCD_d7        D4
#define LCD_en        D5
#define PELTIER_EN    D6
#define TEMP_SENSOR   D7
#define PELTIER       D8
#define RESET         A0
#define INTERVAL      1000
#define RESET_DURATION  1000

// Default Threshold Temperature Value
String inputMessage = "25.0";
String lastTemperature;
String enableArmChecked = "checked";
String inputMessage2 = "true";


// HTML web page to handle 2 input fields (threshold_input, enable_arm_input)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Temperature Threshold PELTIER Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <h2>DS18B20 Temperature</h2> 
  <h3>%TEMPERATURE% &deg;C</h3>
  <h2>ESP Arm Trigger</h2>
  <form action="/get">
    Temperature Threshold <input type="number" step="0.1" name="threshold_input" value="%THRESHOLD%" required><br>
    Arm Trigger <input type="checkbox" name="enable_arm_input" value="true" %ENABLE_ARM_INPUT%><br><br>
    <input type="submit" value="Submit">
  </form>
</body></html>)rawliteral";

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

AsyncWebServer server(80);
// EasyButton button(RESET);

// Replaces placeholder with DS18B20 values
String processor(const String& var){
  //Serial.println(var);
  if(var == "TEMPERATURE"){
    return lastTemperature;
  }
  else if(var == "THRESHOLD"){
    return inputMessage;
  }
  else if(var == "ENABLE_ARM_INPUT"){
    return enableArmChecked;
  }
  return String();
}

// Flag variable to keep track if triggers was activated or not
bool triggerActive = false;

const char* PARAM_INPUT_1 = "threshold_input";
const char* PARAM_INPUT_2 = "enable_arm_input";

unsigned long previousMillis = 0;

OneWire oneWire(TEMP_SENSOR);
DS18B20 tempSensor(&oneWire);
LiquidCrystal lcd(LCD_rs, LCD_en, LCD_d4, LCD_d5, LCD_d6, LCD_d7);
float temperature;
// todo
// need a reset button to erase eeprom

String header;



// void onPressedForDuration() {
//   wifiManager.resetSettings();
//   lcd.setCursor(0,0);
//   lcd.print("Setup Mode");
//   delay(1000);
//   lcd.setCursor(0,1);
//   lcd.print("Release Button");
//   ESP.restart();
// }

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  // push button on booting time for reset
  pinMode(RESET, INPUT);
  digitalWrite(RESET, HIGH);

  // if(digitalRead(RESET) == LOW) {

  // }

  // set custom ip for portal
  //wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  // fetches ssid and pass from eeprom and tries to connect
  // if it does not connect it starts an access point with the specified name
  // here  "AutoConnectAP"
  // and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("Peltier Controller");
  // or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();
  
  // if you get here you have connected to the WiFi

  lcd.setCursor(0,0);
  lcd.print(WiFi.localIP());
  tempSensor.begin();
  tempSensor.setResolution(12);

  Serial.println("Connected.");
  
  pinMode (PELTIER, OUTPUT);
  digitalWrite (PELTIER, LOW);
  
  pinMode (PELTIER_EN, OUTPUT);
  digitalWrite (PELTIER_EN, LOW);
  
  // Send web page to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Receive an HTTP GET request at <ESP_IP>/get?threshold_input=<inputMessage>&enable_arm_input=<inputMessage2>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    // GET threshold_input value on <ESP_IP>/get?threshold_input=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      // GET enable_arm_input value on <ESP_IP>/get?enable_arm_input=<inputMessage2>
      if (request->hasParam(PARAM_INPUT_2)) {
        inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
        enableArmChecked = "checked";
      }
      else {
        inputMessage2 = "false";
        enableArmChecked = "";
      }
    }
    Serial.println(inputMessage);
    Serial.println(inputMessage2);
    request->send(200, "text/html", "New Setting Received.<br><a href=\"/\">Returning to Home Page</a><script>window.setTimeout(function(){window.location.href = '/';}, 1000);</script>");
  });
  server.onNotFound(notFound);
  server.begin();

  // button.onPressedFor(RESET_DURATION, onPressedForDuration);
}


void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= INTERVAL) {
    previousMillis = currentMillis;
    tempSensor.requestTemperatures();
    temperature = tempSensor.getTempC();
    // set the cursor to column 0, line 1
    // (note: line 1 is the second row, since counting begins with 0):
    lcd.setCursor(0, 1);
    // print the number of seconds since reset:
    lcd.print(temperature);
    lcd.print("/");
    lcd.print(inputMessage.toFloat());
    lcd.print(" ");

    if(triggerActive == true){
      lcd.print("COOL");
    }else{
      lcd.print("HEAT");
    }  

    lastTemperature = String(temperature);
    
    // Check if temperature is above threshold and if it needs to trigger PELTIER
    if(temperature > inputMessage.toFloat() && inputMessage2 == "true" && !triggerActive){
      String message = String("Temperature above threshold. Current temperature: ") + 
                            String(temperature) + String("C");
      Serial.println(message);
      triggerActive = true;
      digitalWrite(PELTIER, HIGH);
    }
    // Check if temperature is below threshold and if it needs to trigger PELTIER
    else if((temperature < inputMessage.toFloat()) && inputMessage2 == "true" && triggerActive) {
      String message = String("Temperature below threshold. Current temperature: ") + 
                            String(temperature) + String(" C");
      Serial.println(message);
      triggerActive = false;
      digitalWrite(PELTIER, LOW);
    }
  }

}


