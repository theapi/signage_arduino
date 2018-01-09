
/*
 * WebSocketClient.ino
 *
 */

#include <Arduino.h>

// Installed via the Arduino library manager.
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WebSocketsClient.h>


#include "config.h"

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

#define JSON_BUFFER_SIZE 1024

char configured = 0;
char pusher_ready = 0;


String pusherSubscribeJsonString(char * channel) {
  String json = "{\"event\":\"pusher:subscribe\",\"data\": {\"channel\": \"_C_\"}}";
  json.replace(String("_C_"), String(channel));
  return json;
}
void onPusherConnectionEstablished(const char* data) {
  StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
  JsonObject& jsonObj = jsonBuffer.parseObject(data);

  if (jsonObj.success()) {
    const char* activity_timeout = jsonObj["activity_timeout"];
    webSocket.setReconnectInterval(atoi(activity_timeout) * 1000);

    pusher_ready = 1;
  }
}

void onMessage(const char* data) {
  StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
  JsonObject& jsonObj = jsonBuffer.parseObject(data);

  if (jsonObj.success()) {
    const char* title = jsonObj["title"];
    Serial.print("title: "); Serial.println(title);
  }
}

void onMyEvent(const char* data) {
  StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
  JsonObject& jsonObj = jsonBuffer.parseObject(data);

  if (jsonObj.success()) {
    const char* name = jsonObj["name"];
    Serial.print("name: "); Serial.println(name);
  }
}

void handlePusherEvent(uint8_t * payload) {

  // Create the temporary buffer for parsing the json.
  StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
  // Parse the json string.
  JsonObject& jsonObj = jsonBuffer.parseObject(payload);
  if (!jsonObj.success()) {
    return; 
  }
  
  // Get the pusher event name.
  const char* event = jsonObj["event"];
  const char* data = jsonObj["data"];
  Serial.print("Event: "); Serial.println(event);

  // Event subscribers (poor man's event dispatcher).
  if (strcmp(event, "pusher:connection_established") == 0) {
    onPusherConnectionEstablished(data);
  }
  else if (strcmp(event, "message") == 0) {
    onMessage(data);
  }
  else if (strcmp(event, "my-event") == 0) {
    onMyEvent(data);
  }
  // 
    
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] Disconnected!\n");
      break;
    case WStype_CONNECTED:
      Serial.printf("[WSc] Connected to url: %s\n", payload); 
      break;
    case WStype_TEXT:
      Serial.printf("[WSc] got text: %s\n", payload);
      handlePusherEvent(payload);
      break;
  }
}

void signageConfigure() {
  // Get the config json from SIGNAGE_CONFIG_URL

  String payload;

  HTTPClient http;
  Serial.print("[HTTP] begin...\n");
  // configure traged server and url
  http.begin(SIGNAGE_CONFIG_URL);

  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();

  // httpCode will be negative on error
  if(httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if(httpCode == HTTP_CODE_OK) {
          payload = http.getString();
          Serial.println(payload);
      }
  } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  

  // Subscribe to the Pusher channels listed in the config.
  StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
  JsonObject& jsonObj = jsonBuffer.parseObject(payload);

  if (jsonObj.success()) {
    const char* channel_channel = jsonObj["channels"][0]["channel_name"];
    Serial.print("channel_channel: "); Serial.println(channel_channel);
    const char* channel_control = jsonObj["channels"][1]["channel_name"];
    Serial.print("channel_control: "); Serial.println(channel_control);

    // Subscribe to the 2 channels.
    String control = pusherSubscribeJsonString("channel_control");
    webSocket.sendTXT(control);

    String channel = pusherSubscribeJsonString("channel_channel");
    webSocket.sendTXT(channel);

    configured = 1;
  }
  
}

void setup() {
  Serial.begin(115200);

  Serial.setDebugOutput(true);

  Serial.println();

  WiFiMulti.addAP(WIFI_SID, WIFI_PASWORD);
  while(WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(200);
  }

  // server address, port and URL
  webSocket.begin(PUSHER_HOST, 80, PUSHER_PATH);

  // event handler
  webSocket.onEvent(webSocketEvent);

  // try ever 5000 again if connection has failed
  webSocket.setReconnectInterval(5000);

  // Config the device according to the dennis signage config page.
  //signageConfigure();

}

void loop() {
  webSocket.loop();

  if (!configured && pusher_ready) {
    // Config the device according to the dennis signage config page.
    signageConfigure();
  }
  
}


