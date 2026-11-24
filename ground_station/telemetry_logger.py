import csv
import json
import time
from collections import deque
from datetime import datetime
import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

PORT = "COM7"
BAUD = 115200
WINDOW = 100  # 10 seconds of rolling telemetry at 10 Hz

ser = serial.Serial(PORT, BAUD, timeout=1)
time.sleep(2)  # Allow Arduino Uno auto-reset on serial open
ser.reset_input_buffer()

log_filename = f"telemetry_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
csv_file = open(log_filename, mode="w", newline="")
writer = csv.writer(csv_file)
writer.writerow(["timestamp", "mode", "pan_deg", "tilt_deg", "ldr", "temp_c"])

print(f"Connected to {PORT} @ {BAUD} baud. Logging to {log_filename}...")

t_buf = deque(maxlen=WINDOW)
pan_buf = deque(maxlen=WINDOW)
tilt_buf = deque(maxlen=WINDOW)
ldr_buf = deque(maxlen=WINDOW)
temp_buf = deque(maxlen=WINDOW)
start_time = time.time()

plt.style.use("dark_background")
fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 7), sharex=True)
fig.suptitle("Pan-Tilt Robotics Node — Live UART Telemetry (10 Hz)", fontsize=13, fontweight="bold")

line_pan, = ax1.plot([], [], color="#00e5ff", label="Pan (°)")
line_tilt, = ax1.plot([], [], color="#ffab40", label="Tilt (°)")
ax1.set_ylabel("Angle (°)")
ax1.legend(loc="upper left")
ax1.grid(True, alpha=0.25)

line_ldr, = ax2.plot([], [], color="#ffd740", label="LDR (0–1023)")
ax2.set_ylabel("Light ADC")
ax2.set_ylim(0, 1050)
ax2.legend(loc="upper left")
ax2.grid(True, alpha=0.25)

line_temp, = ax3.plot([], [], color="#ff5252", label="Temp (°C)")
ax3.set_ylabel("Temp (°C)")
ax3.set_xlabel("Elapsed Time (s)")
ax3.legend(loc="upper left")
ax3.grid(True, alpha=0.25)

mode_text = ax1.text(0.99, 0.85, "MODE: MANUAL", transform=ax1.transAxes,
                     ha="right", color="#69f0ae", fontweight="bold")

def update(frame):
    while ser.in_waiting:
        raw = ser.readline().decode("utf-8", errors="ignore").strip()
        if not raw.startswith("{"):
            continue
        try:
            pkt = json.loads(raw)
            elapsed = round(time.time() - start_time, 2)
            mode = pkt["mode"]
            pan = float(pkt["pan_deg"])
            tilt = float(pkt["tilt_deg"])
            ldr = int(pkt["ldr"])
            temp = float(pkt["temp_c"])

            writer.writerow([datetime.now().isoformat(), mode, pan, tilt, ldr, temp])
            csv_file.flush()

            t_buf.append(elapsed)
            pan_buf.append(pan)
            tilt_buf.append(tilt)
            ldr_buf.append(ldr)
            temp_buf.append(temp)

            mode_text.set_text(f"MODE: {mode}")
        except (json.JSONDecodeError, KeyError, ValueError):
            pass

    if t_buf:
        line_pan.set_data(t_buf, pan_buf)
        line_tilt.set_data(t_buf, tilt_buf)
        line_ldr.set_data(t_buf, ldr_buf)
        line_temp.set_data(t_buf, temp_buf)

        ax3.set_xlim(max(0, t_buf[-1] - 10), t_buf[-1] + 0.5)
        ax1.relim(); ax1.autoscale_view(scalex=False)
        ax3.relim(); ax3.autoscale_view(scalex=False)

    return line_pan, line_tilt, line_ldr, line_temp, mode_text

ani = FuncAnimation(fig, update, interval=50, blit=False, cache_frame_data=False)

try:
    plt.tight_layout()
    plt.show()
finally:
    ser.close()
    csv_file.close()
    print(f"Session saved cleanly to {log_filename}")