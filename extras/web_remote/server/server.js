const express = require('express');
const WebSocket = require('ws');
const mqtt = require('mqtt');
const path = require('path');
const fs = require('fs');
const bodyParser = require('body-parser');

// Load configuration
const config = require('./config/config.json');

// Express app
const app = express();
const port = config.server.port || 3000;

// WebSocket server
const wss = new WebSocket.Server({ noServer: true });
let frontends = []; // WebSocket connected frontends

// Connected instances
// If a single instance is connected through both protocols there will be double receive messages!
const activeWSInstances = new Map();
const activeMQTTInstances = new Map();

// MQTT client
let mqttClient = null;
if (config.mqtt.url && config.mqtt.enabled) {
  mqttClient = mqtt.connect(config.mqtt.url);
}

if (config.websocket && config.websocket.enabled) {
  config.websocket.urls.forEach(url => {
    connectToWebSocketInstance(url);
  });
}

// --- WebSocket handling for frontends ---
wss.on('connection', (ws) => {
  console.log('Frontend connected (ws)');
  frontends.push(ws);

  ws.send(generateInstancesMessage());

  ws.on('message', (event) => {
    const messageStr = event.toString();

    console.log('Received message from frontend:', messageStr);
    handleFrontendMessage(messageStr);
  });

  ws.on('close', () => {
    console.log('Frontend disconnected');
    removeFrontend(ws);
  });

  broadcastActiveInstances();
});

function handleFrontendMessage(message) {
  try {
    const parsedMessage = JSON.parse(message);
    //console.log('Parsed JSON message:', parsedMessage);

    sendToInstances(message);
  } catch (err) {
    console.error('Failed to parse message as JSON:', err);
  }
}

function removeFrontend(ws) {
  frontends = frontends.filter(client => client !== ws);
}

if (mqttClient) {
  mqttClient.on('connect', () => {
    console.log('[MQTT] Connected to broker:', config.mqtt.url);

    mqttClient.subscribe(config.mqtt.receiveTopic, (err) => {
      if (err) {
        console.error('[MQTT] Failed to subscribe to receive topic:', err);
      } else {
        console.log('[MQTT] Subscribed to receive topic:', config.mqtt.receiveTopic);
      }
    });

    mqttClient.subscribe(config.mqtt.statusTopic, (err) => {
      if (err) {
        console.error('[MQTT] Failed to subscribe to status topic:', err);
      } else {
        console.log('[MQTT] Subscribed to status topic:', config.mqtt.statusTopic);
      }
    });
  });

  mqttClient.on('message', (topic, message) => {
    const messageStr = message.toString();
    console.log(`[MQTT] Received message on ${topic}:`, messageStr);

    if (topic === config.mqtt.receiveTopic) {
      forwardMessageToFrontends(messageStr);
      return;
    }

    const topicParts = topic.split('/');
    if (topicParts.length >= 3 && topicParts[topicParts.length - 3] === 'status' && topicParts[topicParts.length - 1] === 'log') {
      const instanceId = topicParts[topicParts.length - 2];
      console.log(`[MQTT] Log message from ${instanceId}:`, messageStr);
      frontends.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
          client.send(messageStr);
        }
      });
      return;
    } else if (topicParts.length >= 2 && topicParts[topicParts.length - 2] === 'status') {
      const instanceId = topicParts[topicParts.length - 1];
      console.log(`[MQTT] Status message from ${instanceId}:`, messageStr);
      if (messageStr.length === 0 || !instanceId.trim()) {
        activeMQTTInstances.delete(instanceId);
        broadcastActiveInstances();
        return;
      }
      try {
        const instanceData = JSON.parse(messageStr);
        if (instanceData.instance && instanceData.instance.trim() !== "") {
          activeMQTTInstances.set(instanceId, { ...instanceData });
          broadcastActiveInstances();
        } else {
          console.warn(`[MQTT] Ignoring empty instance data from ${instanceId}`);
        }
      } catch (err) {
        console.error('Failed to parse instance data as JSON:', err);
      }
      return;
    }

    console.error(`[MQTT] UNHANDLED: Received message on ${topic}:`, messageStr);
  });

  mqttClient.on('error', (err) => {
    console.error('[MQTT] Connection error:', err);
  });

  mqttClient.on('close', () => {
    console.log('[MQTT] Connection closed.');
  });
}

function connectToWebSocketInstance(url, delay = 500) {
  const maxDelay = 300000;
  const ws = new WebSocket(url);

  ws.on('open', () => {
    console.log(`[WS] Connected to ${url}`);
    activeWSInstances.set(ws, {});
  });

  ws.on('message', (event) => {
    const messageString = event.toString();

    console.log(`[WS] Received from ${url}:`, messageString);

    if (messageString.startsWith("CON:")) {
      try {
        const instanceData = JSON.parse(messageString.slice(4));
        if (instanceData.instance && instanceData.instance.trim() !== "") {
          activeWSInstances.set(ws, { ...instanceData });
          broadcastActiveInstances();
        } else {
          console.warn(`[MQTT] Ignoring empty instance data from ${instanceId}`);
        }
      } catch (error) {
        console.error("Failed to parse CON message:", error);
      }
      return;
    }

    forwardMessageToFrontends(messageString);
  });

  ws.on('close', () => {
    console.log(`[WS] Disconnected from ${url}`);
    activeWSInstances.delete(ws);

    const newDelay = Math.min(delay * 2, maxDelay);
    console.log(`[WS] Retrying connection to ${url} in ${newDelay}ms`);
    setTimeout(() => connectToWebSocketInstance(url, newDelay), newDelay);
  });

  ws.on('error', (err) => {
    console.error(`[WS] Error with ${url}:`, err);
  });

  activeWSInstances.set(ws, {});
}

function sendToInstances(message) {
  let formattedMessage = "";

  try {
    const parsedMessage = JSON.parse(message);

    if (Array.isArray(parsedMessage)) {
      formattedMessage = parsedMessage.map(cmd => JSON.stringify(cmd)).join("\n");
    } else {
      formattedMessage = JSON.stringify(parsedMessage);
    }
  } catch (err) {
    console.error("Failed to parse command message as JSON:", err);
    return;
  }

  if (mqttClient && mqttClient.connected) {
    mqttClient.publish(config.mqtt.sendTopic, formattedMessage);
  }

  activeWSInstances.forEach((value, ws) => {
    if (value.instance && activeMQTTInstances.has(value.instance)) {
      return;
    }
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(formattedMessage);
    }
  });
}

function forwardMessageToFrontends(message) {
  frontends.forEach((ws, index) => {
    if (ws.readyState === WebSocket.OPEN) {
      try {
        ws.send(message);
      } catch (err) {
        console.error(`Failed to send message to frontend at index ${index}:`, err);
        removeFrontend(ws);
      }
    } else {
      removeFrontend(ws);
    }
  });
}

function generateInstancesMessage() {
  const wsInstanceArray = Array.from(activeWSInstances.values()).filter(instance => instance && instance.instance);
  const mqttInstanceArray = Array.from(activeMQTTInstances.values()).filter(instance => instance && instance.instance);
  const message = "CLI:" + JSON.stringify({ clients: [...wsInstanceArray, ...mqttInstanceArray] });
  return message;
}

function broadcastActiveInstances() {
  const message = generateInstancesMessage();
  console.log('broadcastActiveInstances ->', message);
  forwardMessageToFrontends(message);
}

// --- Express App Routes ---
app.use(bodyParser.json());
app.use(express.static(path.join(__dirname, '../public')));

app.get('/api/remotes', (req, res) => {
  const remotesDir = path.join(__dirname, 'config/remotes');
  fs.readdir(remotesDir, (err, files) => {
    if (err) {
      return res.status(500).send('Error reading remotes directory');
    }

    const remoteFiles = files.filter(file => file.endsWith('.json'));
    const remotes = [];

    remoteFiles.forEach((file) => {
      try {
        const filePath = path.join(remotesDir, file);
        const remoteConfig = require(filePath);

        // Validate `buttons` array
        if (!Array.isArray(remoteConfig.buttons)) {
          console.warn(`Skipping remote with invalid buttons array: ${file}`);
          return; // Skip this remote
        }

        remotes.push(remoteConfig);
      } catch (error) {
        console.error(`Failed to load remote file: ${file}`, error);
      }
    });

    // Sort remotes by button count (ascending)
    remotes.sort((a, b) => (a.buttons.length || 0) - (b.buttons.length || 0));

    res.json(remotes);
  });
});

// --- HTTP and WebSocket Server ---
const server = app.listen(port, () => {
  console.log(`Server running at http://localhost:${port}`);
});

// WebSocket upgrade handling (when the frontend connects via WS)
server.on('upgrade', (request, socket, head) => {
  wss.handleUpgrade(request, socket, head, (ws) => {
    wss.emit('connection', ws, request);
  });
});
