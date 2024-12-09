
// Copyright (c) 2024 Daniel Wallner

#include <pgmspace.h>

#define WEBSERVER_H // https://stackoverflow.com/questions/75043892/i-am-facing-http-get-conflicts-with-a-previous-declaration-error-with-the-wifi
#include <ESPAsyncWebServer.h>

#ifdef ESP8266
#include <ESPAsyncTCP.h>
#else
#include <AsyncTCP.h>
#endif

const char webInterfaceHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Inseparates WebSocket Messages</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  body {
    font-family: Arial, sans-serif;
    margin: 20px;
    line-height: 1.6;
  }
  textarea {
    width: 100%;
    margin: 10px 0;
    font-size: 16px;
    padding: 10px;
    box-sizing: border-box;
  }
  #log {
    width: 100%;
    height: 200px;
    overflow-y: scroll;
    border: 1px solid #ccc;
    background: #f9f9f9;
    padding: 10px;
  }
  table {
    width: 100%;
    border-collapse: collapse;
    margin-top: 20px;
  }
  table, th, td {
    border: 1px solid #ddd;
  }
  th, td {
    padding: 8px;
    text-align: left;
  }
  th {
    background-color: #f2f2f2;
  }
  button {
    padding: 10px 20px;
    margin-top: 10px;
    background-color: #4CAF50;
    color: white;
    border: none;
    cursor: pointer;
    font-size: 16px;
  }
  button:hover {
    background-color: #45a049;
  }
  .status {
    display: inline-block;
    margin-left: 10px;
    padding: 5px 10px;
    border-radius: 12px;
    font-size: 14px;
    color: white;
    font-weight: bold;
    text-align: center;
    border: 1px solid transparent;
    box-shadow: none;
  }
  .status.connected {
    background-color: #28a745;
    border-color: #218838;
  }
  .status.disconnected {
    background-color: #dc3545;
    border-color: #c82333;
  }
</style>
<script>
  var ws;
  function setupWebSocket() {
    ws = new WebSocket("ws://" + location.host + "/ws");

    ws.onopen = function() {
      updateStatus(true);
    };

    ws.onclose = function() {
      updateStatus(false);
      setTimeout(setupWebSocket, 3000);
    };

    ws.onmessage = function(event) {
      const data = event.data;

      // Handle CON message
      if (data.startsWith("CON:")) {
        try {
          const jsonData = JSON.parse(data.slice(4));
          if (jsonData.hostname && jsonData.description) {
            document.title = `${jsonData.hostname} - ${jsonData.description} - Messages`;
            document.getElementById("header").textContent = `${jsonData.hostname} - ${jsonData.description} - Messages`;
          }
        } catch (e) {
          console.error("Failed to parse CON message:", e);
        }
        return;
      }

      // Create a new row for the log
      const newRow = document.createElement("tr");
      const timestampCell = document.createElement("td");
      const messageCell = document.createElement("td");

      const now = new Date();
      const timestamp = `${now.toLocaleTimeString()}.${now.getMilliseconds().toString().padStart(3, '0')}`;
      timestampCell.textContent = timestamp;

      // Style messages based on type
      if (data.startsWith("ACK:")) {
        messageCell.style.color = "green";
      } else if (data.startsWith("LOG:")) {
        messageCell.style.color = "blue";
      }
      messageCell.textContent = data;

      // Add cells to the new row
      newRow.appendChild(timestampCell);
      newRow.appendChild(messageCell);

      // Add the new row to the top of the table
      const historyTable = document.getElementById("history");
      if (historyTable.firstChild) {
        historyTable.insertBefore(newRow, historyTable.firstChild);
      } else {
        historyTable.appendChild(newRow);
      }
    };
  }

  function updateStatus(isConnected) {
    const statusElement = document.getElementById('status');
    if (isConnected) {
      statusElement.textContent = "Connected";
      statusElement.className = "status connected";
    } else {
      statusElement.textContent = "Disconnected";
      statusElement.className = "status disconnected";
    }
  }

  function sendMessage() {
    const input = document.getElementById("input");
    const message = input.value.trim();
    if (message && ws.readyState === WebSocket.OPEN) {
      ws.send(message);
      input.focus();
    } else {
      alert("WebSocket is not connected.");
    }
  }

  window.onload = setupWebSocket;
</script>
</head>
<body>
<h1 id="header">Inseparates Messages</h1>
<textarea id="input" rows="6" placeholder='Enter JSON message'></textarea><br>
<button onclick="sendMessage()">Send</button>
<span id="status" class="status disconnected">Disconnected</span>

<h2>Log</h2>
<table>
  <thead>
    <tr>
      <th>Timestamp</th>
      <th>Message</th>
    </tr>
  </thead>
  <tbody id="history"></tbody>
</table>
</body>
</html>
)rawliteral";

AsyncWebServer server(80); // Web server on port 80.
AsyncWebSocket ws("/ws");  // WebSocket endpoint.

void websocketConnectionMessage(AsyncWebSocketClient *client)
{
  preferences.begin(PREFERENCES_NAMESPACE, true);
  const String hostname = preferences.getString("hostname");
  const String description = preferences.getString("description");
  preferences.end();

  StaticJsonDocument<256> doc;
  doc["hostname"] = hostname;
  doc["description"] = description;

  String message;
  serializeJson(doc, message);

  String conMessage = "CON:";
  conMessage += message;
  client->text(conMessage);
}

void websocketLogMessage(const String &message)
{
  String logMessage("LOG:");
  logMessage += message;
  ws.textAll(logMessage);
}

void websocketMessage(const String &message)
{
  ws.textAll(message);
}

void webSocketCallback(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if(type == WS_EVT_CONNECT)
  {
    Serial.printf("WebSocket [%s][%u] connected\n", server->url(), client->id());
    websocketConnectionMessage(client);
  }
  if(type != WS_EVT_DATA)
  {
    return;
  }
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if(!(info->final && info->index == 0 && info->len == len))
  {
    // Ignore multi frame messages.
    return;
  }

  String message;
  for(size_t i = 0; i < info->len; i++)
  {
    message += char(data[i]);
  }

  String response = "ACK:" + message;
  client->text(response);

  messageCallback(message);
}

void setupWebServer()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send_P(200, "text/html", webInterfaceHTML);
  });

  ws.onEvent(webSocketCallback);
  server.addHandler(&ws);

  server.begin();
}

void loopWebServer()
{
  ws.cleanupClients();
}
