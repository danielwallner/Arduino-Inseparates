// Copyright (c) 2024 Daniel Wallner

// ESP8266/ESP32 MQTT client and HTTP/Websocket server.
// Connect to temporary AP and navigate to http://192.168.4.1 to configure.

// Tested on ESP8266, ESP32 and ESP32-C3
// For best reliability use ESP32 with two cores

// Supports interconnect protocols with Inseparates
// Supports IR protocols with IRremoteESP8266 or Inseparates
// Partially compatible with OpenMQTTGateway's regular JSON message structure.

// MQTT Topics:
// inseparates/status/{id} -- Active instances
// inseparates/status/{id}/log -- Instance log
// inseparates/commands/IRtoMQTT -- Received messages
// inseparates/commands/MQTTtoIR -- Send message

// Messages can be multiple JSON lines or just concatenated JSON.

// Supports the following communication modes:

// MQTT
// Serial
// WebSocket on /ws
// HTTP POST send on /send
// Web

// Sending {"wipe":""} or "{"config":""} with any value restarts and activates the config AP.

// Extensions compared to OpenMQTTGateway regular JSON messages:
// address - integer/hex - used instead of value - not supported by all protocols.
// command - integer/hex - used instead of value - not supported by all protocols.
// extended - integer/hex - used instead of value - either extended data or a signal use an extended protocol - not supported by all protocols.
// repeat - integer - when set to -1 results in indefinite repeat - stop by sending message with no value or different valid message on same bus and protocol.
// bus - integer - when absent or zero send IR.
// instance - string - when absent all clients send.

// Example messages:
// {"bus":2,"value":"1","protocol_name":"SWITCH"} - bus is bit number to set
// {"bus":2,"protocol_name":"SWITCH_TOGGLE"} - bus == bit number to toggle
// {"bus":0,"value":200,"protocol_name":"PULSE"} - bus is bit number to activate for value milliseconds
// {"repeat":4,"address":16,"command":13,"protocol_name":"RC5"} - Send Philips amplifier mute toggle as IR
// {"repeat":4,"bus":1,"protocol":1,"bits":12,"hex":"0x340D"} - Send Philips amplifier mute toggle on RC5 bus
// {"bits":36,"hex":"0x210401200","protocol_name":"ESI"} - Send Philips amplifier select tape on ESI bus
// {"hex":"0x54","bits":12,"protocol_name":"SONY"} - Send Sony IR message
// {"bus":1,"hex":"0x54","bits":12,"protocol_name":"SONY"} - Send NEC message on SR/CONTROL S bus
// {"repeat":3,"bus":1,"bits":32,"hex":"0xA3","protocol_name":"NEC2"} - Send NEC message on SR/CONTROL S bus with full message as repeat message
// {"bus":1,"bits":32,"hex":"0xa5a5a5a5","protocol_name":"NEC"} - Send NEC repeat message on SR/CONTROL S bus
// {"bits":16,"hex":"0x10C","protocol_name":"DATALINK86"} - Send B&O 455 kHz IR message
// {"bus":1,"bits":16,"hex":"0x10C","protocol_name":"DATALINK86"} - Send B&O Datalink 86 message
// {"bus":2,"hex":"0x55","protocol_name":"DATALINK80"} - Send B&O Datalink 80 message to tape 2
// {"hex":"0x00940001","protocol_name":"TECHNICS_SC"} - Send Technics System Control message select CD

#define DEBUG_RECEIVE 0

#define ECHO 0
#define INS_DEBUGGING 0
#define SERIAL_DEBUGGING 0

#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define IR_SEND_ACTIVE HIGH
#else
#define IR_SEND_ACTIVE LOW
#endif
#define SWITCH_ACTIVE_LOW 1

#define ENABLE_INSEPARATES 1
#define ENABLE_MQTT 1
#define ASYNC_MQTT 1
#define ENABLE_IRREMOTE 0
#define ENABLE_WEBSERVER 1
#if defined(ESP32) && !defined(CONFIG_IDF_TARGET_ESP32C3)
#define ENABLE_MULTICORE 1
#else
#define ENABLE_MULTICORE 0 // See notes in IC.h
#endif

#define ENABLE_SWITCH 0
#define ENABLE_PULSE 0
// 455 kHz IR send is only enabled when ENABLE_IRREMOTE = 0
#define ENABLE_IR_455 1
#define ENABLE_BEO36 0 // Can only be received!
#define ENABLE_ESI 1
#define ENABLE_SECOND_SR 0
#define ENABLE_BEO_IC 0
#define ENABLE_TECHNICS_SC 1

#define SERIAL_BUFFER_SIZE 256
#define TMP_STRING_SIZE 256
#define TMP_JSON_SIZE 256

#include <WiFiManager.h>
#if ENABLE_MQTT
#if ASYNC_MQTT
#include <AsyncMqttClient.h>
#else
// PubSubClient is blocking and not ideal for this use case.
#include <PubSubClient.h>
#endif
#endif
#include <Preferences.h>
#include <FastTime.h>

#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>

using namespace inseparates;

#define PREFERENCES_NAMESPACE "InsEx9MQTT"
#define DEFAULT_DESCRIPTION "Living Room"
#define DEFAULT_ROOT_TOPIC "inseparates"

WiFiClient espClient;
#if ENABLE_MQTT
#if ASYNC_MQTT
AsyncMqttClient mqttClient;
#else
PubSubClient mqttClient(espClient);
#endif
#endif

String instanceId = "Ins-";
#if ENABLE_MQTT
String rootTopic;
String statusTopic;
String rxTopic;
String txTopic;
#endif

enum ins_log_target_t {
  ILT_NONE = 0,
  ILT_ALL,
  ILT_SERIAL,
  ILT_MQTT,
  ILT_WEBSOCKET,
};

size_t connectMessage(char *destination, size_t d_size);
void logLine(const char *message, size_t length, ins_log_target_t target = ILT_NONE);
const char* getLogLine();
void messageCallback(const byte *payload, size_t length, ins_log_target_t target);
size_t strnnncat(char *destination, size_t d_pos, size_t d_size, const char *source, size_t s_size = 0);
size_t strnnncat(char *destination, size_t d_pos, size_t d_size, const __FlashStringHelper *source);

#include "IC.h"
#if ENABLE_WEBSERVER
#include "Web.h"
#endif

#if ENABLE_MULTICORE
TaskHandle_t task;
inseparates::LockFreeFIFO<Message, 16> outFIFO;
#endif

Preferences preferences;
char serialBuffer[SERIAL_BUFFER_SIZE];
uint8_t serialBufferLength;
Timekeeper mqttTimekeeper;
uint32_t mqttTimeOut;
char lastLogLine[TMP_STRING_SIZE];
bool forceConfig;

WiFiManagerParameter hostname_param("hostname", "Host Name");
WiFiManagerParameter description_param("description", "Description");
WiFiManagerParameter mqtt_server_param("mqtt_server", "MQTT Server");
WiFiManagerParameter mqtt_port_param("mqtt_port", "MQTT Port");
WiFiManagerParameter mqtt_user_param("mqtt_user", "MQTT Username");
WiFiManagerParameter mqtt_password_param("mqtt_password", "MQTT Password");
WiFiManagerParameter mqtt_root_param("mqtt_root_topic", "MQTT Root Topic");

class ArrayPrinter : public Print
{
  char *_buffer;
  size_t _size;
  size_t _pos;
public:
  ArrayPrinter(char *buffer, size_t size) : _buffer(buffer), _size(size), _pos(0) {}
  ~ArrayPrinter() { write('\0'); }
  size_t write(uint8_t v) override { if (_pos >= _size) return 0; _buffer[_pos] = v; ++_pos; return 1; }
  size_t write(const uint8_t *buffer, size_t size) override { size_t i = 0; for(; i < size && _pos < _size; ++i, ++_pos) _buffer[_pos] = buffer[i]; return i; }
  int availableForWrite() override { return _size - _pos; }
};

size_t strnnncat(char *destination, size_t d_pos, size_t d_size, const char *source, size_t s_size)
{
  for (; d_pos < d_size && destination[d_pos]; ++d_pos);
  size_t s_pos = 0;
  for (; d_pos < d_size && source[s_pos] && (!s_size || s_pos < s_size); ++d_pos, ++s_pos)
  {
    destination[d_pos] = source[s_pos];
  }
  if (d_pos >= d_size)
    --d_pos;
  destination[d_pos] = '\0';
  return d_pos;
}

size_t strnnncat(char *destination, size_t d_pos, size_t d_size, const __FlashStringHelper *source)
{
  for (; d_pos < d_size && destination[d_pos]; ++d_pos);
  size_t s_pos = 0;
  for (; d_pos < d_size && pgm_read_byte((const char*)source + s_pos); ++d_pos, ++s_pos)
  {
    destination[d_pos] = pgm_read_byte((const char*)source + s_pos);
  }
  if (d_pos >= d_size)
    --d_pos;
  destination[d_pos] = '\0';
  return d_pos;
}

// Does not check length so make sure hexStr is at least 11 bytes!
void hex64(char *hexStr, uint64_t value)
{
  hexStr[0] = '0';
  hexStr[1] = 'x';
  uint8_t pos = 2;
  for (int8_t i = 60; i >= 0; i -= 4)
  {
    uint8_t nibble = (value >> i) & 0xF;
    if (!(pos == 2 && nibble == 0))
    {
      hexStr[pos] = nibble < 10 ? char(nibble + '0') : char(nibble + 'A' - 10);
      ++pos;
    }
  }
  if (pos == 2)
  {
    hexStr[pos] = '0';
    ++pos;
  }
  hexStr[pos] = '\0';
}

void saveSetting(const char *key, const char *value)
{
  preferences.putString(key, value);
  Serial.print("Saved setting: ");
  Serial.println(key);
}

String loadSetting(const char *key, const char *defaultValue = "")
{
  String value = preferences.getString(key, defaultValue);
  Serial.print("Loaded setting: ");
  Serial.println(key);
  return value;
}

size_t connectMessage(char *destination, size_t d_size)
{
  StaticJsonDocument<TMP_JSON_SIZE> doc;
  doc["instance"] = instanceId;
  doc["hostname"] = hostname_param.getValue();
  doc["description"] = description_param.getValue();
#if ENABLE_WEBSERVER
  char url[64];
  {
    ArrayPrinter p(url, 64);
    p.print("http://");
    WiFi.localIP().printTo(p);
  }
  doc["url"] = url;
#endif

  size_t ret = serializeJson(doc, destination, d_size);
  if (ret >= d_size)
    ret = d_size - 1;
  destination[ret] = '\0';
  return ret;
}

#if ENABLE_MQTT
void mqttConnectCallback(bool sessionPresent)
{
  Serial.print("Connected to ");
  Serial.println(mqtt_server_param.getValue());
  mqttClient.subscribe(txTopic.c_str(), 0);

  mqttTimeOut = 0;

  char jsonString[TMP_JSON_SIZE];
  size_t length = connectMessage(jsonString, TMP_JSON_SIZE);
#if ASYNC_MQTT
    mqttClient.publish(statusTopic.c_str(), 1, true, jsonString, length);
#else
  mqttClient.publish(statusTopic.c_str(), (const uint8_t *)jsonString, length, true);
#endif
}

#if ASYNC_MQTT
void mqttDisconnectCallback(AsyncMqttClientDisconnectReason reason)
{
  mqttTimeOut = 1000000;
  mqttTimekeeper.reset();
}
void mqttMessageCallback(char* /*topic*/, char* payload, AsyncMqttClientMessageProperties /*properties*/, size_t length, size_t /*index*/, size_t /*total*/)
#else
void mqttMessageCallback(char */*topic*/, byte *payload, unsigned length)
#endif
{
  messageCallback((const byte*)payload, length, ILT_MQTT);
}
#endif

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

  delay(500);

  Serial.println("\nStarting...");

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

  instanceId += String(random(0xffff), HEX);
#if ENABLE_MQTT
  statusTopic = mqtt_root + "/status/";
  statusTopic += instanceId;
  rxTopic = mqtt_root + "/IRtoMQTT";
  txTopic = mqtt_root + "/commands/MQTTtoIR";
#endif

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

  Serial.print("IR receiver active on pin ");
  Serial.println(kIRReceivePin);

  Serial.print("IR transmitter active on pin ");
  Serial.println(kIRSendPin);

#if ENABLE_IRREMOTE
  setupIR();
#endif
#if ENABLE_WEBSERVER
  setupWebServer();
#endif
  setupInseparates();

#if ENABLE_MQTT
  if (!mqtt_server_param.getValue()[0])
  {
    Serial.print("MQTT server not set! Disabling MQTT!");
    return;
  }

  uint16_t mqtt_port_value = atoi(mqtt_port.c_str());
  mqttClient.setServer(mqtt_server_param.getValue(), mqtt_port_value);
#if ASYNC_MQTT
  mqttClient.onConnect(mqttConnectCallback);
  mqttClient.onDisconnect(mqttDisconnectCallback);
  mqttClient.onMessage(mqttMessageCallback);
#else
  mqttClient.setCallback(mqttMessageCallback);
#endif
#endif

#if ENABLE_MULTICORE
  xTaskCreatePinnedToCore(loop2Code, "loop2", 4096, NULL, 18, &task, 1 - xPortGetCoreID());
  String coreMessage("setup() running on core ");
  coreMessage += xPortGetCoreID();
  coreMessage += '\n';
  Serial.print(coreMessage);
#else
  setupInseparates2();
#endif
}

void loop()
{
#if ENABLE_MQTT
  if (mqtt_server_param.getValue()[0] && WiFi.isConnected())
  {
    if (!mqttClient.connected() && mqttTimekeeper.microsSinceReset() > mqttTimeOut)
    {
      mqttTimeOut = 60000000;
      mqttTimekeeper.reset();
      reconnect();
    }
#if !ASYNC_MQTT
    mqttClient.loop();
#endif
  }
#endif

#if !ENABLE_MULTICORE
  loop2();
#endif

#if ENABLE_WEBSERVER
  loopWebServer();
#endif

#if ENABLE_MULTICORE
  if (!outFIFO.empty())
  {
    doPublish(outFIFO.readRef());
    outFIFO.pop();
  }
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
      Serial.println("Serial overflow");
    }

    if (serialBufferLength && serialBuffer[serialBufferLength - 1] == '}')
    {
      serialBuffer[serialBufferLength] = 0;
#if ECHO && SERIAL_DEBUGGING
      Serial.print("Serial: ");
      Serial.println(serialBuffer);
#endif
      messageCallback((const byte*)serialBuffer, serialBufferLength, ILT_SERIAL);

      serialBufferLength = 0;
    }
  }
}

void loop2()
{
#if ENABLE_IRREMOTE
  loopIR();
#endif
#if ENABLE_INSEPARATES
  loopInseparates();
#endif
}

#if ENABLE_MULTICORE
void loop2Code(void* pvParameters)
{
  Timekeeper timekeeper;
  {
    String coreMessage("loop2() running on core ");
    coreMessage += xPortGetCoreID();
    coreMessage += '\n';
    Serial.print(coreMessage);
  }
  setupInseparates2();
  for (;;)
  {
    loop2();
    // The following is not entirely ideal.
    // It would be better to make loop2() return if it was idle and only do the following then.
    if (timekeeper.microsSinceReset() < 4000000L)
    {
      continue;
    }
    timekeeper.reset();
    vTaskDelay(10);
  }
}
#endif

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

#if ENABLE_MQTT
void reconnect()
{
  Serial.print("Connecting to ");
  Serial.print(mqtt_server_param.getValue());
  Serial.println(" ...");

  const char *mqtt_user = mqtt_user_param.getValueLength() ? mqtt_user_param.getValue() : nullptr;
  const char *mqtt_password =  mqtt_password_param.getValueLength() ? mqtt_password_param.getValue() : nullptr;
#if ASYNC_MQTT
  mqttClient.setClientId(instanceId.c_str());
  mqttClient.setCredentials(mqtt_user, mqtt_password);
  mqttClient.setWill(statusTopic.c_str(), 1, true, "");
  mqttClient.connect();
#else
  if (mqttClient.connect(instanceId.c_str(), mqtt_user, mqtt_password, statusTopic.c_str(), 1, true, ""))
  {
    mqttConnectCallback(false);
  }
  else
  {
    Serial.print("Connect failed, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" retrying in 5 seconds");
    mqttTimeOut = 5000000;
    mqttTimekeeper.reset();
  }
#endif
}
#endif

void logLine(const char *message, size_t length, ins_log_target_t send)
{
  lastLogLine[0] = '\0';
  strnnncat(lastLogLine, 0, TMP_STRING_SIZE, message, length);
#if ENABLE_MQTT
  if ((send == ILT_ALL || send == ILT_MQTT) && mqttClient.connected())
  {
    char logTopic[128];
    logTopic[0] = '\0';
    size_t pos = strnnncat(logTopic, 0, 128, statusTopic.c_str());
    pos = strnnncat(logTopic, pos, 128, "/log");
#if ASYNC_MQTT
    mqttClient.publish(logTopic, 1, false, message, length);
#else
    mqttClient.publish(logTopic, (const uint8_t *)lastLogLine, length, false);
#endif
  }
#endif
  if (send == ILT_ALL || send == ILT_SERIAL)
  {
    Serial.println(lastLogLine);
  }
#if ENABLE_WEBSERVER
  if (send == ILT_ALL || send == ILT_WEBSOCKET)
  {
    websocketLogMessage(lastLogLine);
  }
#endif
}

const char* getLogLine()
{
  return lastLogLine;
}

void publish(Message &message)
{
#if ENABLE_MULTICORE
  // Note that message.protocol_name can only point to global const data or it will not survive to the other side!
  outFIFO.writeRef() = message;
  outFIFO.push();
#else
  doPublish(message);
#endif
}

void doPublish(const Message &message)
{
  StaticJsonDocument<TMP_JSON_SIZE> doc;
  char hexStr[12];
  hex64(hexStr, message.value);

  doc["value"] = message.value;
  doc["hex"] = hexStr;
  if (message.protocol_name)
  {
    doc["protocol_name"] = message.protocol_name;
  }
  else if (message.protocol > 0 && message.protocol < NEC2)
  {
    doc["protocol_name"] = typeToString((decode_type_t)message.protocol, message.repeat);
  }
  doc["protocol"] = message.protocol;
  if (message.repeat)
    doc["repeat"] = message.repeat;
  doc["bits"] = message.bits;
  if (message.bus)
    doc["bus"] = message.bus;
  doc["instance"] = instanceId;

  char json[TMP_JSON_SIZE];
  size_t ret = serializeJson(doc, json, TMP_JSON_SIZE);
  if (ret >= TMP_JSON_SIZE)
  {
    ret = TMP_JSON_SIZE - 1;
    json[ret] = '\0';
  }

#if ENABLE_MQTT
#if ASYNC_MQTT
  mqttClient.publish(rxTopic.c_str(), 1, false, json, ret);
#else
  mqttClient.publish(rxTopic.c_str(), (const uint8_t *)json, ret, false);
#endif
#endif
#if ENABLE_WEBSERVER
  websocketMessage(json);
#endif
#if SERIAL_DEBUGGING
  Serial.println(json);
#endif
}

void messageCallback(const byte *payload, size_t length, ins_log_target_t target)
{
  lastLogLine[0] = '\0';
  size_t startPos = 0;
  for (size_t i = 0; i < length; ++i)
  {
    if (payload[i] == '}')
    {
      handleJSON((const char*)payload + startPos, i + 1 - startPos, target);
#if ECHO
      logLine((const char*)payload + startPos, i + 1 - startPos, target);
#endif
      startPos = i + 1;
    }
  }
}

void handleJSON(const char* string, size_t length, ins_log_target_t target)
{
  Message message = {};
  message.logTarget = target;

  StaticJsonDocument<256> doc;

  DeserializationError error = deserializeJson(doc, string, length);

  if (error)
  {
    char errorString[TMP_STRING_SIZE];
    errorString[0] = '\0';
    size_t pos = strnnncat(errorString, 0, TMP_STRING_SIZE, F("JSON parse error: "));
    pos = strnnncat(errorString, pos, TMP_STRING_SIZE, error.f_str());
    pos = strnnncat(errorString, pos, TMP_STRING_SIZE, string, length);
    logLine(errorString, pos, target);
    return;
  }

  {
    const char* instance = doc["instance"];
    if (instance)
    {
      if (strcmp(instance, instanceId.c_str()))
      {
#if INS_DEBUGGING
        char logString[TMP_STRING_SIZE];
        logString[0] = '\0';
        size_t pos = strnnncat(logString, 0, TMP_STRING_SIZE, F("Ignoring message to "));
        pos = strnnncat(logString, pos, TMP_STRING_SIZE, strVal);
        logLine(logString, pos, target);
#endif
        return;
      }
    }
  }

  for (JsonPair kv : doc.as<JsonObject>())
  {
    const __FlashStringHelper *errorMsg = nullptr;
    auto decode = [&message, &kv, &errorMsg, target]()
    {
      const char *strVal = nullptr;
      int64_t intVal = 0;
      bool isNum = false;
      bool mustBeNum = true;

      if (kv.value().is<const char*>())
      {
        strVal = kv.value().as<const char*>();
        if (!strcmp(strVal, "0x"))
        {
          size_t pos = 0;
          intVal = std::stoull(strVal, &pos, 0);
          if (strVal[pos] == '\0')
          {
            isNum = true;
          }
        }
        else if (*strVal == '-' || isdigit(*strVal))
        {
          size_t pos = 0;
          intVal = std::stoll(strVal, &pos, 0);
          if (strVal[pos] == '\0')
          {
            isNum = true;
          }
        }
      }
      else if (kv.value().is<long long>())
      {
        isNum = true;
        intVal = kv.value().as<long long>();
      }
      else if (kv.value().is<unsigned long long>())
      {
        isNum = true;
        intVal = kv.value().as<unsigned long long>();
      }

      if (kv.key() == "config")
      {
        config(false);
      }
      else if (kv.key() == "wipe")
      {
        config(true);
      }
      else if (kv.key() == "bus")
      {
        if (unsigned(intVal) > 0xff)
          return 1;
        message.bus = intVal;
      }
      else if (kv.key() == "address")
      {
        message.setAddress(intVal);
      }
      else if (kv.key() == "command")
      {
        message.setCommand(intVal);
      }
      else if (kv.key() == "extended")
      {
        message.setExtended(intVal);
      }
      else if (kv.key() == "value")
      {
        // Hex is accepted here too!
        message.setValue(intVal);
      }
      else if (kv.key() == "hex")
      {
        message.setValue(intVal);
      }
      else if (kv.key() == "repeat")
      {
        if (intVal < -1)
          return 2;
        message.repeat = intVal;
      }
      else if (kv.key() == "bits")
      {
        if (intVal < 0)
          return 3;
        message.bits = intVal;
      }
      else if (kv.key() == "protocol")
      {
        if (intVal < 0)
          return 4;
        if (message.protocol && message.protocol != intVal)
        {
          errorMsg = F("Protocol mismatch: ");
          return 5;
        }
        message.protocol = intVal;
      }
      else if (strVal && kv.key() == "protocol_name")
      {
        mustBeNum = false;
        message.protocol_name = strVal;
        int16_t protocol = strToDecodeType(message.protocol_name);
        if (protocol == UNKNOWN)
        {
          protocol = strToDecodeTypeEx(message.protocol_name);
        }
        if (message.protocol && message.protocol != protocol)
        {
          errorMsg = F("Protocol error: ");
          return 6;
        }
        message.protocol = protocol;
      }
      else
      {
        errorMsg = F("Unknown key: ");
        return 7;
      }

      if (mustBeNum && !isNum)
      {
        errorMsg = F("Value is not a number: ");
        return 8;
      }
      return 0;
    };
    int result = decode();
    if (result)
    {
      char errorString[TMP_STRING_SIZE];
      errorString[0] = '\0';
      size_t pos = strnnncat(errorString, 0, TMP_STRING_SIZE, errorMsg ? errorMsg : F("Invalid value: "));
      pos = strnnncat(errorString, pos, TMP_STRING_SIZE, kv.key().c_str());
      pos = strnnncat(errorString, pos, TMP_STRING_SIZE, ": ");
      if (kv.value().is<const char*>())
      {
        pos = strnnncat(errorString, pos, TMP_STRING_SIZE, kv.value().as<const char*>());
      }
      else if (kv.value().is<int>())
      {
        pos += snprintf(errorString + pos, TMP_STRING_SIZE - pos, "%d", kv.value().as<int>());
        if (pos >= TMP_STRING_SIZE)
          pos = TMP_STRING_SIZE - 1;
      }
      logLine(errorString, pos, target);
      return;
    }
  }

  dispatch(message);
}
