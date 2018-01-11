
/*
 * Pusher client for the Dennis Signage application
 *
 */

#include <Arduino.h>

// Installed via the Arduino library manager.
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WebSocketsClient.h>

// Rename example_config.h to config.h
#include "config.h"

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

#define JSON_BUFFER_SIZE 1024

char configured = 0;
char pusher_ready = 0;
char subscribed_channel[32];
char subscribed_control[32];

String pusherSubscribeJsonString(char * channel) {
  String json = "{\"event\":\"pusher:subscribe\",\"data\": {\"channel\": \"_C_\"}}";
  json.replace(String("_C_"), String(channel));
  return json;
}

String pusherUnSubscribeJsonString(char * channel) {
  String json = "{\"event\":\"pusher:unsubscribe\",\"data\": {\"channel\": \"_C_\"}}";
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
    const char* notification_type = jsonObj["notification_type"];
    Serial.print("notification_type: "); Serial.println(notification_type);
  }
}

void onRemoteControl(const char* data) {
  String payload = String(data);
  signageProcessConfigPayload(payload);
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
  // Message event.
  else if (strcmp(event, "message") == 0) {
    onMessage(data);
  }
  // Change channel event.
  else if (strcmp(event, "remote-control") == 0) {
    onRemoteControl(data);
  }
  // Just for simple testing.
  else if (strcmp(event, "my-event") == 0) {
    onMyEvent(data);
  }
    
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

/**
 * Subscribe to the Pusher channels listed in the config json payload.
 */
void signageProcessConfigPayload(String payload) {
  StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
  JsonObject& jsonObj = jsonBuffer.parseObject(payload);

  if (jsonObj.success()) {
    if (configured) {
      // Unsubscribe from the existing channels.
      String unsub_control = pusherSubscribeJsonString(subscribed_control);
      webSocket.sendTXT(unsub_control);
      String unsub_channel = pusherSubscribeJsonString(subscribed_channel);
      webSocket.sendTXT(unsub_channel);
    }
    
    const char* channel_channel = jsonObj["channels"][0]["channel_name"];
    strncpy(subscribed_channel, channel_channel, 32);
    Serial.print("channel_channel: "); Serial.println(channel_channel);
    const char* channel_control = jsonObj["channels"][1]["channel_name"];
    strncpy(subscribed_control, channel_control, 32);
    Serial.print("channel_control: "); Serial.println(channel_control);

    // Subscribe to the 2 channels.
    String control = pusherSubscribeJsonString("channel_control");
    webSocket.sendTXT(control);

    String channel = pusherSubscribeJsonString("channel_channel");
    webSocket.sendTXT(channel);

    configured = 1;
  }
}

/**
 * Get the config json from SIGNAGE_CONFIG_URL
 */
String signageGetConfigPayload() {
  String payload;

  HTTPClient http;
  Serial.print("[HTTP] begin...\n");
  // configure traged server and url
  http.begin(SIGNAGE_CONFIG_URL);

  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();

  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
        payload = http.getString();
        Serial.println(payload);
    }
  } else {
    //@todo handle error
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();

  return payload;
}

/**
 * Config the device according to the dennis signage config page.
 */
void signageConfigure() {
  signageProcessConfigPayload(signageGetConfigPayload());

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

  // try every so often if connection has failed
  webSocket.setReconnectInterval(120000);
}

void loop() {
  webSocket.loop();

  if (!configured && pusher_ready) {
    // Subscribe to the required Pusher channels.
    signageConfigure();
  }
  
}


