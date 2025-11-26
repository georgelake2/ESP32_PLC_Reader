import time
import serial
import json

PORT = "/dev/ttyACM0"
BAUD = 115200
OUT = "logs/log_test.jsonl"

def extract_json(text: str):
    text = text.strip()
    if not text:
        return None
    # Pure JSON line
    if text.startswith("{") and text.endswith("}"):
        return text
    # ESP-IDF prefix case: I (...) JSON: {...}
    if "JSON:" in text:
        _, after = text.split("JSON:", 1)
        after = after.strip()
        if after.startswith("{") and after.endswith("}"):
            return after
    return None

with serial.Serial(PORT, BAUD, timeout=1) as ser, open(OUT, "w", encoding="utf-8") as f:
    # Pulse reset (DTR/RTS) to restart ESP32
    ser.dtr = False
    ser.rts = False
    time.sleep(0.1)
    ser.dtr = True
    ser.rts = True

    # Optional: discard first line after reset
    ser.readline()

    print("Logging JSON… Ctrl+C to stop")
    while True:
        line = ser.readline()
        if not line:
            continue
        text = line.decode("utf-8", errors="ignore")
        js = extract_json(text)
        if js is None:
            continue
        f.write(js + "\n")
        print(js)
        f.flush()
