let activeInstances = []; // Store active instances

setupWebSocket();

document.addEventListener("DOMContentLoaded", () => {
  fetchRemotes();
});

window.addEventListener("load", adjustTabSpacing);
window.addEventListener("resize", adjustTabSpacing);

function adjustTabSpacing() {
  const tabs = document.getElementById("tabs");
  const spacer = document.getElementById("tab-spacer");

  requestAnimationFrame(() => {
    setTimeout(() => {
      let tabHeight = tabs.offsetHeight;

      // For iOS Safari, check visualViewport height
      if (window.visualViewport) {
        tabHeight = Math.max(tabHeight, window.visualViewport.height * 0.05);
      }
      spacer.style.height = tabHeight + "px";
    }, 100); // Delay ensures iOS updates correctly
  });
}

function setupWebSocket() {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  const host = window.location.host;
  ws = new WebSocket(`${protocol}//${host}`);

  ws.onopen = () => {
    console.log('[Frontend] Connected to WebSocket server');
    updateStatus(true);
  };

  ws.onmessage = (event) => {
    if (typeof event.data === 'string') {
      data = event.data;
    } else {
      console.error('Unknown data type received:', event.data);
      return;
    }
    const messageString = event.data;
    //console.log('Received message:', messageString);

    if (messageString.startsWith("CLI:")) {
      try {
        const jsonData = JSON.parse(messageString.slice(4));
        if (jsonData.clients) {
          activeInstances = jsonData.clients;
          renderActiveClients(activeInstances);
          fetchRemotes();
        }
      } catch (e) {
        console.error("Failed to parse CLI message:", e);
      }
    } else {
      logMessage(messageString);
    }
  };

  ws.onclose = () => {
    console.log('[Frontend] WebSocket connection closed');
    updateStatus(false);
    setTimeout(setupWebSocket, 2000);
  };

  ws.onerror = (error) => {
    console.error('[Frontend] WebSocket Error:', error);
  };
}

function updateStatus(isConnected) {
  const statusElement = document.getElementById('status');
  statusElement.textContent = isConnected ? "Connected" : "Disconnected";
  statusElement.className = isConnected ? "status connected" : "status disconnected";
}

function fetchRemotes() {
  fetch('/api/remotes')
    .then(response => response.json())
    .then(remotes => {
      const groupedRemotes = {};
      const currentActiveTab = document.querySelector("#tab-container button.active");
      const activeTabName = currentActiveTab ? currentActiveTab.textContent.trim() : null;
      remotes.forEach(remote => {
        remote.instance = findMatchingInstance(remote.hostname);
        const group = remote.group && remote.group.trim() ? remote.group : "Ungrouped";
        if (!groupedRemotes[group]) {
          groupedRemotes[group] = [];
        }
        groupedRemotes[group].push(remote);
      });
      renderRemoteTabs(groupedRemotes, activeTabName);
    })
    .catch(error => console.error('Error loading remotes:', error));
}

function findMatchingInstance(remoteHostname) {
  if (!remoteHostname) {
    return null;
  }

  const matchedInstance = activeInstances.find(client =>
    client.hostname ? client.hostname.toLowerCase().includes(remoteHostname.toLowerCase()) : false
  );
  return matchedInstance ? matchedInstance.instance : null;
}

function renderRemoteTabs(groupedRemotes, activeTabName = null) {
  const tabContainer = document.getElementById("tab-container");
  tabContainer.innerHTML = "";

  let firstTab = null;
  let newActiveTab = null;

  Object.keys(groupedRemotes).forEach((group, index) => {
    if (groupedRemotes[group].length === 0)
      return;

    const tabButton = document.createElement("button");
    tabButton.textContent = group;
    tabButton.onclick = () => {
      document.getElementById("log-container").style.display = "none";
      document.getElementById("remote-container").style.display = "flex";

      document.querySelectorAll("#tab-container button").forEach(btn => btn.classList.remove("active"));
      tabButton.classList.add("active");

      renderGroup(groupedRemotes[group]);
    };

    if (activeTabName && group === activeTabName) {
      newActiveTab = tabButton;
    }

    if (index === 0) firstTab = tabButton;
    tabContainer.appendChild(tabButton);
  });

  const logTabButton = document.createElement("button");
  logTabButton.textContent = "Status";
  logTabButton.onclick = () => {
    document.getElementById("remote-container").style.display = "none";
    document.getElementById("log-container").style.display = "block";

    document.querySelectorAll("#tab-container button").forEach(btn => btn.classList.remove("active"));
    logTabButton.classList.add("active");
  };
  tabContainer.appendChild(logTabButton);

  if (newActiveTab) {
    newActiveTab.click();
  } else if (firstTab) {
    firstTab.click();
  } else {
    logTabButton.click();
  }

  adjustTabSpacing();
}

function renderGroup(remotes) {
  const remoteContainer = document.getElementById("remote-container");
  remoteContainer.innerHTML = "";

  remotes.forEach(remote => {
    if (!remote.buttons || remote.buttons.length === 0)
      return;

    renderRemote(remote);
  });
}

function renderRemote(remote) {
  const remoteDiv = document.createElement('div');
  remoteDiv.classList.add('remote');

  const columns = remote.columns || 3;
  remoteDiv.style.gridTemplateColumns = `repeat(${columns}, 1fr)`;
  remoteDiv.style.maxWidth = `${columns * 105}px`;

  const remoteTitle = document.createElement('h2');
  remoteTitle.textContent = remote.name;
  remoteTitle.style.gridColumn = `span ${columns}`;
  remoteDiv.appendChild(remoteTitle);

  if (!Array.isArray(remote.buttons)) {
    console.error(`Invalid buttons array in remote: ${remote.name}`, remote.buttons);
    return;
  }

  remote.buttons.forEach(button => {
    if (!button || Object.keys(button).length === 0) {
      const dummy = document.createElement('div');
      dummy.classList.add('dummy');
      remoteDiv.appendChild(dummy);
    } else if (button && button.label && button.command) {
      const btn = document.createElement('button');
      btn.textContent = button.label;

      if (btn.textContent.trim().length === 1) {
        btn.style.fontSize = '24px';
        btn.style.fontWeight = 'bold';
      }

      if (button.command.repeat < 0) {
        setupPressAndHold(btn, button.command, remote.instance);
      } else {
        setupPress(btn, button.command, remote.instance);
      }

      remoteDiv.appendChild(btn);
    } else {
      console.warn(`Invalid button configuration at index ${index} in remote:`, button);
    }
  });

  updateRemoteAvailability(remote, remoteDiv);
  document.getElementById('remote-container').appendChild(remoteDiv);
}

function updateRemoteAvailability(remote, remoteDiv) {
  const isAvailable = activeInstances.some(client => client.instance === remote.instance);
  remoteDiv.style.backgroundColor = isAvailable ? "#f5f5f5" : "#ffcccc"; 
}

function setupPress(btn, command, instance) {
  btn.onmousedown = () => handlePress(command, instance);
  btn.ontouchstart = (e) => {
    e.preventDefault();
    handlePress(command, instance);
  };
}

function setupPressAndHold(btn, command, instance) {
  let pressTimer;

  const startPress = () => {
    handlePress(command, instance);
    pressTimer = setInterval(() => {
      handlePress(command, instance);
    }, 500);
  };

  const stopPress = () => {
    clearInterval(pressTimer);
    handleRelease(command, instance);
  };

  btn.onmousedown = startPress;
  btn.onmouseup = stopPress;
  btn.onmouseleave = stopPress;
  btn.ontouchstart = (e) => {
    e.preventDefault();
    startPress();
  };
  btn.ontouchend = stopPress;
  btn.ontouchcancel = stopPress;
}

function handlePress(command, instance) {
  if (instance && !command.instance) {
    command.instance = instance;
  }
  console.log(`Sending command: ${JSON.stringify(command)}`);
  if (ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(command));
  }
}

function handleRelease(command, instance) {
  if (instance && !command.instance) {
    command.instance = instance;
  }

  const releaseCommand = { ...command };
  delete releaseCommand.value;
  delete releaseCommand.hex;
  delete releaseCommand.address;
  delete releaseCommand.command;
  delete releaseCommand.extended;
  delete releaseCommand.repeat;

  console.log(`Sending release command: ${JSON.stringify(releaseCommand)}`);
  if (ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(releaseCommand));
  }
}

function renderActiveClients(clients) {
  const clientsContainer = document.getElementById('active-clients');
  clientsContainer.innerHTML = ''; // Clear previous content

  clients.forEach(client => {
    const clientDiv = document.createElement('div');
    clientDiv.classList.add('client');

    // Create a clickable URL link element
    const clientUrl = document.createElement('a');
    clientUrl.href = client.url;
    clientUrl.textContent = client.url;
    clientUrl.target = "_blank"; // Open in a new tab
    clientUrl.rel = "noopener noreferrer"; // Security best practice

    // Format the client information
    clientDiv.innerHTML = `ID: ${client.instance}, Name: ${client.hostname}, Description: ${client.description}, Url: `;
    clientDiv.appendChild(clientUrl); // Append the link separately

    clientsContainer.appendChild(clientDiv);
  });
}

function logMessage(message) {
  const logEntry = document.createElement('div');
  logEntry.textContent = `[${new Date().toISOString()}] ${message}`;

  if (message.startsWith("LOG:")) {
    logEntry.style.color = "blue";
  }

  document.getElementById('log').prepend(logEntry);
}
