#!/bin/bash

VENV_DIR="venv"

if [ ! -d "$VENV_DIR" ]; then
    echo "Creating virtual environment..."
    python3 -m venv $VENV_DIR
fi

source $VENV_DIR/bin/activate

pip install --upgrade pip

REQUIRED_PACKAGES=(
    "pyyaml"
    "paho-mqtt"
    "websocket-client"
)

echo "Checking and installing dependencies..."
for package in "${REQUIRED_PACKAGES[@]}"; do
    if ! pip show $package > /dev/null 2>&1; then
        echo "Installing $package..."
        pip install $package
    else
        echo "$package is already installed."
    fi
done

if [ $# -eq 0 ]
  then
    python3 beo_decoder.py --mqtt-broker 192.168.6.66 --ws-url ws://192.168.6.101/ws
else
    python3 beo_decoder.py "$@"
fi
