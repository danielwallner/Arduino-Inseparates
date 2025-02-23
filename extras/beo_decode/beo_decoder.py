import yaml
import json
import paho.mqtt.client as mqtt
import websocket
import threading
import argparse
import time

parser = argparse.ArgumentParser(description="MQTT/WebSocket listener for Bang & Olufsen messages.")
parser.add_argument("--mqtt-broker", type=str, help="MQTT broker address")
parser.add_argument("--mqtt-topic", type=str, default="inseparates/commands/IRtoMQTT", help="MQTT topic to subscribe to")
parser.add_argument("--ws-url", type=str, help="WebSocket server URL")
parser.add_argument("--config", type=str, default="messages.yaml", help="Path to YAML config file")
args = parser.parse_args()

with open(args.config, "r") as f:
    format_desc = yaml.safe_load(f)

def decode_message(protocol, hex_value, bits):
    if protocol not in format_desc:
        return f"Unsupported protocol - Raw: 0x{hex_value.upper()} b{bin(int(hex_value, 16))[2:].zfill(bits)}"

    bit_config = format_desc[protocol].get(f"{bits}bit", None)
    if not bit_config:
        return f"Unsupported bit length - Raw: 0x{hex_value.upper()} b{bin(int(hex_value, 16))[2:].zfill(bits)}"

    common_config = {entry["name"]: entry for entry in format_desc[protocol].get("common", [])}

    int_value = int(hex_value, 16)
    decoded_fields = {}
    shift = bits
    format_type = None
    format_variants = None

    for field in bit_config:
        field_name = field["name"]
        field_bits = field.get("bits", 0)
        shift -= field_bits
        field_value = (int_value >> shift) & ((1 << field_bits) - 1)

        if field_name == "format":
            format_type = field_value
            format_variants = field.get("variants", {})

        field_display = f"0x{field_value:02X} b{field_value:0{field_bits}b}"
        lookup_table = field.get("values", common_config.get(field_name, {}).get("values", {}))

        if field_value in lookup_table:
            field_display += f" ({lookup_table[field_value]})"

        decoded_fields[field_name] = field_display

    if format_variants and format_type in format_variants:
        specific_format = format_variants[format_type]
        for field in specific_format:
            field_name = field["name"]
            field_bits = field["bits"]
            shift -= field_bits
            field_value = (int_value >> shift) & ((1 << field_bits) - 1)
            field_display = f"0x{field_value:02X} b{field_value:0{field_bits}b}"
            lookup_table = field.get("values", common_config.get(field_name, {}).get("values", {}))

            if field_value in lookup_table:
                field_display += f" ({lookup_table[field_value]})"

            decoded_fields[field_name] = field_display

    if not decoded_fields:
        return f"No format variant match - Raw: 0x{hex_value.upper()} b{bin(int_value)[2:].zfill(bits)}"

    decoded_str = "".join(
        f" {k}: {v}" for k, v in decoded_fields.items()
    )
    return decoded_str

def process_message(message):
    try:
        data = json.loads(message)
        hex_value = data.get("hex", "0x0")[2:]  # Strip "0x" prefix
        protocol = data.get("protocol_name", "")
        bits = data.get("bits", 0)

        decoded = decode_message(protocol, hex_value, bits)

        print(f"Raw: {message}")
        print(f"Decoded: {decoded}\n")
    except json.JSONDecodeError:
        print(f"Raw: {message}")
    except Exception as e:
        print(f"Error processing message: {e}")

def on_mqtt_message(client, userdata, msg):
    process_message(msg.payload.decode("utf-8"))

def start_mqtt():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_message = on_mqtt_message
    client.connect(args.mqtt_broker, 1883, 60)
    client.subscribe(args.mqtt_topic)
    client.loop_forever()

def on_ws_message(ws, message):
    process_message(message)

def on_ws_error(ws, error):
    print(f"WebSocket error: {error}")

def on_ws_close(ws, close_status_code, close_msg):
    print("WebSocket closed, attempting reconnect...")

def on_ws_open(ws):
    print("WebSocket connected")

def start_websocket():
    retry_delay = 1
    while True:
        try:
            ws = websocket.WebSocketApp(
                args.ws_url,
                on_message=on_ws_message,
                on_error=on_ws_error,
                on_close=on_ws_close
            )
            ws.on_open = on_ws_open
            ws.run_forever()
        except Exception as e:
            print(f"WebSocket error: {e}. Reconnecting in {retry_delay} seconds...")
            time.sleep(retry_delay)
            retry_delay = min(retry_delay * 2, 120)

if args.mqtt_broker:
    threading.Thread(target=start_mqtt, daemon=True).start()

if args.ws_url:
    threading.Thread(target=start_websocket, daemon=True).start()

while True:
    time.sleep(1)
