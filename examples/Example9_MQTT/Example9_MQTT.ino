// Copyright (c) 2024 Daniel Wallner

// ESP8266/ESP32 MQTT client.
// Connect to temporary AP and navigate to http://192.168.4.1 to configure.

// Supports interconnect protocols with Inseparates
// Supports IR protocols with IRremoteESP8266
// Partially compatible with OpenMQTTGateway's regular JSON message structure.

// Topics:
// inseparates/status/{id} -- Active instances
// inseparates/status/{id}/log -- Instance log
// inseparates/commands/IRtoMQTT -- Received messages
// inseparates/commands/MQTTtoIR -- Send message

// MQTT messages can be multiple JSON lines or just concatenated JSON.
// Also accepts JSON on the serial port or a WebSocket connection.

// Sending "{wipe}" or "{config}" on serial restarts and activates the config AP.

// Extensions compared to IRremoteESP8266:
// bus - integer - when absent or zero send IR.
// client_id - string - when absent all clients send.

// Example messages:
// {"bus":2,"value":"1","protocol_name":"SWITCH"} - bus is bit number to set
// {"bus":2,"protocol_name":"SWITCH_TOGGLE"} - bus == bit number to toggle
// {"bus":0,"value":200,"protocol_name":"TRIGGER"} - bus is bit number to activate for value milliseconds -- BUG not more than a few seconds or will stick!
// {"repeat":4,"hex":"0x340D","protocol_name":"RC5"} - Send Philips amplifier mute toggle as IR
// {"repeat":4,"bus":1,"protocol":1,"hex":"0x340D"} - Send Philips amplifier mute toggle on RC5 bus
// {"bits":36,"hex":"0x210401200","protocol_name":"ESI"} - Send Philips amplifier select tape on ESI bus
// {"bus":1,"hex":"0x54","bits":12,"protocol_name":"SONY"} - Send Sony message on SR/CONTROL S bus
// {"repeat":3,"bus":1,"hex":"0xA3","bits":12,"protocol_name":"NEC"} - Send NEC message on SR/CONTROL S bus
// {"repeat":3,"bus":1,"hex":"0xA3","bits":12,"protocol_name":"NEC2"} - Send NEC message on SR/CONTROL S bus with full message as repeat message
// {"bus":1,"hex":"0x00","bits":12,"protocol_name":"NEC"} - Send NEC repeat message on SR/CONTROL S bus
// {"bits":17,"hex":"0x340D","protocol_name":"BANG_OLUFSEN"} - Send B&O 455 kHz IR message -- BUG
// {"bus":1,"bits":17,"hex":"0x340D","protocol_name":"DATALINK86"} - Send B&O Datalink 86 message
// {"bus":2,"bits":17,"hex":"0x340D","protocol_name":"DATALINK80"} - Send B&O Datalink 80 message to tape 2
// {"hex":"0x00940001","protocol_name":"TECHNICS_SC"} - Send Technics System Control message select CD

// BEO36 can only be received!

// Edit IC.h to select which protocols should be active.

// Default pin mapping based on WeMos D1 R2 and WeMos D1 32
// Uno ESP8266 ESP32             Function      Notes ESP8266                   Notes ESP32
// 12  GPIO12 GPIO19             ESI
// 11  GPIO13 GPIO23             SR/FLSH
// 10  GPIO15 GPIO5  ESI/DL      ESI/-         Pull-down, must be low at boot  Must be high at boot, outputs PWM at boot
//  9  GPIO13 GPIO13 FLSH/DL80   IR EMIT
//  8  GPIO12 GPIO12 RC5/DL80/SR =/-                                           Must be low at boot
//  7  GPIO14 GPIO14 IR SR/SENS  =/DL                                          Outputs PWM at boot
//  6  GPIO2  GPIO27 IR REC1     SC CLK/DL80   Pull-up, must be high at boot
//  5  GPIO0  GPIO16 SC DATA     =/DL80        Pull-up, must be high at boot
//  4  GPIO4  GPIO17 SC CLK.     IR REC1/SR
//  3  GPIO5  GPIO25 IR EMIT0    IR REC0
//  2  GPIO16 GPIO26 IR REC0     SR/FLSH/REC1  No interrupts, no PWM, high at boot

#define ECHO 0
#define INS_DEBUGGING 0
#if defined(ESP32)
#define REV_A 1
#else
#define REV_A 0
#endif

#define IR_SEND_ACTIVE LOW

#define ENABLE_IRREMOTE 1
#define ENABLE_WEBSERVER 1

#define SERIAL_BUFFER_SIZE 256

#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Preferences.h>

#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>

#define PREFERENCES_NAMESPACE "InsEx9MQTT"
#define DEFAULT_DESCRIPTION "Living Room"
#define DEFAULT_ROOT_TOPIC "inseparates"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

String clientId = "Ins-";
String rootTopic;
String statusTopic;
String rxTopic;
String txTopic;

void logLine(const String &string);
void messageCallback(const String &payload);

Preferences preferences;

#include "IC.h"
#if ENABLE_WEBSERVER
#include "Web.h"
#endif

char serialBuffer[SERIAL_BUFFER_SIZE];
uint8_t serialBufferLength;
Timekeeper mqttTimekeeper;
uint32_t mqttTimeOut;
bool forceConfig;

WiFiManagerParameter hostname_param("hostname", "Host Name");
WiFiManagerParameter description_param("description", "Description");
WiFiManagerParameter mqtt_server_param("mqtt_server", "MQTT Server");
WiFiManagerParameter mqtt_port_param("mqtt_port", "MQTT Port");
WiFiManagerParameter mqtt_user_param("mqtt_user", "MQTT Username");
WiFiManagerParameter mqtt_password_param("mqtt_password", "MQTT Password");
WiFiManagerParameter mqtt_root_param("mqtt_root_topic", "MQTT Root Topic");

void saveSetting(const char *key, const String &value)
{
  preferences.putString(key, value);
  Serial.println("Saved setting: " + String(key));
}

String loadSetting(const char *key, const char *defaultValue = "")
{
  String value = preferences.getString(key, defaultValue);
  Serial.println("Loaded setting: " + String(key));
  return value;
}

void mqttMessageCallback(char *topic, byte *payload, unsigned length)
{
  String payloadString;
  for (unsigned i = 0; i < length; ++i)
  {
    payloadString += char(payload[i]);
  }
  messageCallback(payloadString);
}

void config(bool reset = false)
{
  if (reset)
  {
    Serial.println("Resetting parameters!\nRestarting!");
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.clear();
    preferences.putBool("force_reset", true);
    preferences.end();
  }
  else
  {
    Serial.println("Force config!\nRestarting!");
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putBool("force_config", true);
    preferences.end();
  }

  delay(500);
  ESP.restart();
}

void setup()
{
  Serial.begin(115200);

  Serial.println("");

  preferences.begin(PREFERENCES_NAMESPACE, false);
  String hostname = loadSetting("hostname", PREFERENCES_NAMESPACE);
  String description = loadSetting("description", DEFAULT_DESCRIPTION);
  String mqtt_server = loadSetting("mqtt_server");
  String mqtt_port = loadSetting("mqtt_port", "1883");
  String mqtt_user = loadSetting("mqtt_user");
  String mqtt_password = loadSetting("mqtt_password");
  String mqtt_root = loadSetting("mqtt_root_topic", DEFAULT_ROOT_TOPIC);
  bool forceReset = preferences.getBool("force_reset", false);
  forceConfig = preferences.getBool("force_config", false);
  preferences.end();

  statusTopic = mqtt_root + "/status/";
  clientId += String(random(0xffff), HEX);
  statusTopic += clientId;
  rxTopic = mqtt_root + "/IRtoMQTT";
  txTopic = mqtt_root + "/commands/MQTTtoIR";

  hostname_param.setValue(hostname.c_str(), 40);
  description_param.setValue(description.c_str(), 40);
  mqtt_server_param.setValue(mqtt_server.c_str(), 40);
  mqtt_port_param.setValue(mqtt_port.c_str(), 6);
  mqtt_user_param.setValue(mqtt_user.c_str(), 40);
  mqtt_password_param.setValue(mqtt_password.c_str(), 40);
  mqtt_root_param.setValue(mqtt_root.c_str(), 40);

  WiFiManager wifiManager;

  if (forceReset)
  {
    Serial.println("Force Reset!");
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putBool("force_reset", false);
    preferences.end();
    wifiManager.resetSettings();
  }

  wifiManager.setHostname(hostname);

  wifiManager.addParameter(&hostname_param);
  wifiManager.addParameter(&description_param);
  wifiManager.addParameter(&mqtt_server_param);
  wifiManager.addParameter(&mqtt_port_param);
  wifiManager.addParameter(&mqtt_user_param);
  wifiManager.addParameter(&mqtt_password_param);
  wifiManager.addParameter(&mqtt_root_param);

  wifiManager.setSaveParamsCallback(saveParamsCallback);

  String apName(hostname);
  apName += "_Config";

  if (forceConfig)
  {
    Serial.println("Force Config!");
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putBool("force_config", false);
    preferences.end();
    wifiManager.startConfigPortal(apName.c_str());
  }
  else if (!wifiManager.autoConnect(apName.c_str()))
  {
    Serial.println("ERROR: Failed to connect! Restarting!");
    delay(500);
    ESP.restart();
  }

#if ENABLE_IRREMOTE
  setupIR();
#endif
#if ENABLE_WEBSERVER
  setupWebServer();
#endif
  setupInseparates();

  if (!mqtt_server_param.getValue()[0])
  {
    Serial.print("MQTT server not set! Disabling MQTT!");
    return;
  }

  uint16_t mqtt_port_value = atoi(mqtt_port.c_str());
  mqttClient.setServer(mqtt_server_param.getValue(), mqtt_port_value);
  mqttClient.setCallback(mqttMessageCallback);
}

void loop()
{
  if (mqtt_server_param.getValue()[0])
  {
    if (!mqttClient.connected())
    {
      reconnect();
    }
    mqttClient.loop();
  }

#if ENABLE_IRREMOTE
  loopIR();
#endif
#if ENABLE_WEBSERVER
  loopInseparates();
#endif

  if (Serial.available() > 0)
  {
    uint8_t data = Serial.read();
    if (!serialBufferLength)
    {
      if (data != '{')
      {
        return;
      }
      serialBuffer[serialBufferLength++] = data;
    }
    else if (serialBufferLength + 1 < SERIAL_BUFFER_SIZE)
    {
      serialBuffer[serialBufferLength++] = data;
    }
    else
    {
      serialBufferLength = 0;
      logLine("Serial overflow");
    }

    if (serialBufferLength == 6 && !strncmp("{wipe}", (char*)serialBuffer, 6))
    {
      config(true);
      serialBufferLength = 0;
    }

    if (serialBufferLength == 8 && !strncmp("{config}", (char*)serialBuffer, 8))
    {
      config(false);
      serialBufferLength = 0;
    }

    if (serialBufferLength && serialBuffer[serialBufferLength - 1] == '}')
    {
      serialBuffer[serialBufferLength] = 0;

      String logString = "Serial: ";
      logString += serialBuffer;
      logLine(logString);

      messageCallback(serialBuffer);

      serialBufferLength = 0;
    }
  }
}

void saveParamsCallback()
{
  preferences.begin(PREFERENCES_NAMESPACE, false);
  saveSetting("hostname", hostname_param.getValue());
  saveSetting("description", description_param.getValue());
  saveSetting("mqtt_server", mqtt_server_param.getValue());
  saveSetting("mqtt_port", mqtt_port_param.getValue());
  saveSetting("mqtt_user", mqtt_user_param.getValue());
  saveSetting("mqtt_password", mqtt_password_param.getValue());
  saveSetting("mqtt_root_topic", mqtt_root_param.getValue());
  preferences.end();
  if (forceConfig)
  {
    delay(500);
    ESP.restart();
  }
}

void reconnect()
{
  if (mqttTimekeeper.microsSinceReset() <= mqttTimeOut)
  {
    return;
  }

  Serial.print("Connecting to ");
  Serial.print(mqtt_server_param.getValue());
  Serial.print(" ...");

  const char *mqtt_user = mqtt_user_param.getValueLength() ? mqtt_user_param.getValue() : nullptr;
  const char *mqtt_password =  mqtt_password_param.getValueLength() ? mqtt_password_param.getValue() : nullptr;
  if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password, statusTopic.c_str(), 1, true, ""))
  {
    Serial.println(" connected");
    mqttClient.subscribe(txTopic.c_str());

    mqttTimeOut = 0;

    preferences.begin(PREFERENCES_NAMESPACE, true);
    String description = preferences.getString("description");
    preferences.end();

    StaticJsonDocument<200> doc;
    doc["description"] = description;
    String jsonString;
    serializeJson(doc, jsonString);

    mqttClient.publish(statusTopic.c_str(), jsonString.c_str(), true);
  }
  else
  {
    Serial.print(" failed, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" retrying in 5 seconds");
    mqttTimeOut = 5000000;
    mqttTimekeeper.reset();
  }
}

void logLine(const String &string)
{
  String logTopic = statusTopic + "/log";
  if (mqttClient.connected())
    mqttClient.publish(logTopic.c_str(), string.c_str(), false);
  Serial.println(string);
#if ENABLE_WEBSERVER
  websocketLogMessage(string);
#endif
}

void hex64(String &hexStr, uint64_t value)
{
  hexStr = "0x";
  for (int8_t i = 60; i >= 0; i -= 4)
  {
    uint8_t nibble = (value >> i) & 0xF;
    if (!(hexStr.length() == 2 && nibble == 0))
    {
      hexStr += nibble < 10 ? char(nibble + '0') : char(nibble + 'A' - 10);
    }
  }
  if (hexStr.length() == 2)
    hexStr += '0';
}

void publish(Message &message)
{
  StaticJsonDocument<256> doc;
  String hexStr;
  hex64(hexStr, message.value);

  doc["value"] = message.value;
  doc["hex"] = hexStr;
  if (message.protocol_name)
    doc["protocol_name"] = message.protocol_name;
  doc["protocol"] = message.protocol;
  if (message.repeat)
    doc["repeat"] = message.repeat;
  doc["bits"] = message.bits;
  if (message.bus)
    doc["bus"] = message.bus;
  doc["client_id"] = clientId;

  String json;
  serializeJson(doc, json);

  mqttClient.publish(rxTopic.c_str(), json.c_str(), false);
#if ENABLE_WEBSERVER
  websocketMessage(json.c_str());
#endif
}

void messageCallback(const String &payload)
{
  size_t startPos = 0;
  for (size_t i = 0; i < payload.length(); ++i)
  {
    if (payload[i] == '}')
    {
      String jsonSegment = payload.substring(startPos, i + 1);
      handleJSON(jsonSegment.c_str());
#if ECHO
      logLine(jsonSegment.c_str());
#endif
      startPos = i + 1;
    }
  }
}

void handleJSON(const char* string)
{
  Message message;
  message.value = 0;
  message.protocol_name = nullptr;
  message.protocol = 0;
  message.repeat = 0;
  message.bits = 0;
  message.bus = 0;

  StaticJsonDocument<256> doc;

  DeserializationError error = deserializeJson(doc, string);

  String errorString;

  if (error)
  {
    errorString = "JSON parse error: ";
    errorString += error.f_str();
    errorString = "\nInput: ";
    errorString += string;
    logLine(errorString);
    return;
  }

  for (JsonPair kv : doc.as<JsonObject>())
  {
    if (kv.key() == "client_id")
    {
      const char* client_id = kv.value().as<const char*>();
      if (strcmp(client_id, clientId.c_str()))
      {
        String logString = "Ignoring message to ";
        logString += client_id;
        logLine(logString);
        return;
      }
    }
    else if (kv.key() == "bus")
    {
      message.bus = kv.value().as<uint8_t>();
    }
    else if (kv.key() == "value")
    {
      message.value = kv.value().as<unsigned long long>();
    }
    else if (kv.key() == "hex")
    {
      const char* hex = kv.value().as<const char*>();
      uint64_t value = std::stoull(hex, nullptr, 16);
      message.value = value;
    }
    else if (kv.key() == "repeat")
    {
      message.repeat = kv.value().as<uint8_t>();
    }
    else if (kv.key() == "bits")
    {
      message.bits = kv.value().as<uint8_t>();
    }
    else if (kv.key() == "protocol")
    {
      int16_t protocol = kv.value().as<signed short>();
      if (message.protocol && message.protocol != protocol)
      {
        logLine("Protocol mismatch");
        return;
      }
      message.protocol = protocol;
    }
    else if (kv.key() == "protocol_name")
    {
      message.protocol_name = kv.value().as<const char*>();
      int16_t protocol = strToDecodeType(message.protocol_name);
      if (message.protocol && message.protocol != protocol)
      {
        logLine("Protocol mismatch");
        return;
      }
      message.protocol = protocol;
    }
    else
    {
      if (errorString.length())
        errorString += "\n";
      errorString += "Unknown key:";
      errorString += kv.key().c_str();
      errorString += ": ";
      if (kv.value().is<const char*>())
      {
        errorString += ": ";
        errorString += kv.value().as<const char*>();
      }
      else if (kv.value().is<int>())
      {
        errorString += kv.value().as<int>();
      }
    }
  }

  if (errorString.length())
    logLine(errorString);

  dispatch(message);
}
