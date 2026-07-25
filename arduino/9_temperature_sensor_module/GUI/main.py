#!/usr/bin/env python3
import queue
import threading
import time
import tkinter as tk
from tkinter import ttk, messagebox

import serial
from serial.tools import list_ports

BAUD_RATE = 9600
ADC_MAX = 1023
VREF = 5.0


class App:
    def __init__(self, root):
        self.root = root
        self.root.title("Fan and Temperature Monitor")
        self.root.geometry("720x480")

        self.ser = None
        self.stop_event = threading.Event()
        self.q = queue.Queue()

        self.port_var = tk.StringVar()
        self.conn_var = tk.StringVar(value="Disconnected")
        self.temp_var = tk.StringVar(value="— °C")
        self.fan_var = tk.StringVar(value="OFF")
        self.adc_var = tk.StringVar(value="—")
        self.voltage_var = tk.StringVar(value="— V")
        self.digital_var = tk.StringVar(value="—")
        self.status_var = tk.StringVar(value="Waiting for data...")

        # Replace these with two real calibration measurements.
        self.t1_var = tk.DoubleVar(value=20.0)
        self.a1_var = tk.IntVar(value=500)
        self.t2_var = tk.DoubleVar(value=40.0)
        self.a2_var = tk.IntVar(value=350)

        self.build_ui()
        self.refresh_ports()
        self.process_queue()
        self.root.protocol("WM_DELETE_WINDOW", self.close)

    def build_ui(self):
        main = ttk.Frame(self.root, padding=16)
        main.pack(fill="both", expand=True)

        ttk.Label(
            main,
            text="Temperature-Controlled Fan",
            font=("TkDefaultFont", 18, "bold"),
        ).pack(anchor="w", pady=(0, 12))

        serial_frame = ttk.LabelFrame(main, text="Serial connection", padding=10)
        serial_frame.pack(fill="x")

        ttk.Label(serial_frame, text="Port:").grid(row=0, column=0, sticky="w")
        self.port_box = ttk.Combobox(
            serial_frame, textvariable=self.port_var, state="readonly", width=28
        )
        self.port_box.grid(row=0, column=1, padx=8, sticky="ew")

        ttk.Button(serial_frame, text="Refresh", command=self.refresh_ports).grid(
            row=0, column=2, padx=4
        )

        self.connect_btn = ttk.Button(
            serial_frame, text="Connect", command=self.toggle_connection
        )
        self.connect_btn.grid(row=0, column=3, padx=4)

        ttk.Label(serial_frame, textvariable=self.conn_var).grid(
            row=1, column=0, columnspan=4, sticky="w", pady=(8, 0)
        )
        serial_frame.columnconfigure(1, weight=1)

        live = ttk.LabelFrame(main, text="Live telemetry", padding=14)
        live.pack(fill="both", expand=True, pady=12)

        self.add_row(live, 0, "Estimated temperature:", self.temp_var, 22)
        self.add_row(live, 1, "Fan status:", self.fan_var, 20)
        self.add_row(live, 2, "Analog value:", self.adc_var, 14)
        self.add_row(live, 3, "Analog voltage:", self.voltage_var, 14)
        self.add_row(live, 4, "Digital input:", self.digital_var, 14)

        cal = ttk.LabelFrame(main, text="Two-point temperature calibration", padding=10)
        cal.pack(fill="x")

        ttk.Label(cal, text="Point 1").grid(row=0, column=0, padx=(0, 6))
        ttk.Entry(cal, textvariable=self.t1_var, width=7).grid(row=0, column=1)
        ttk.Label(cal, text="°C at ADC").grid(row=0, column=2, padx=4)
        ttk.Entry(cal, textvariable=self.a1_var, width=7).grid(row=0, column=3)

        ttk.Label(cal, text="Point 2").grid(row=0, column=4, padx=(20, 6))
        ttk.Entry(cal, textvariable=self.t2_var, width=7).grid(row=0, column=5)
        ttk.Label(cal, text="°C at ADC").grid(row=0, column=6, padx=4)
        ttk.Entry(cal, textvariable=self.a2_var, width=7).grid(row=0, column=7)

        ttk.Label(
            cal,
            text=(
                "Use a trusted thermometer and do not adjust the module "
                "potentiometer after calibration."
            ),
            wraplength=660,
        ).grid(row=1, column=0, columnspan=8, sticky="w", pady=(8, 0))

        ttk.Label(main, textvariable=self.status_var).pack(anchor="w", pady=(8, 0))

    @staticmethod
    def add_row(parent, row, label, variable, size):
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky="w", pady=7)
        ttk.Label(
            parent,
            textvariable=variable,
            font=("TkDefaultFont", size, "bold"),
        ).grid(row=row, column=1, sticky="w", padx=12, pady=7)

    def refresh_ports(self):
        ports = [p.device for p in list_ports.comports()]
        self.port_box["values"] = ports
        if ports and self.port_var.get() not in ports:
            self.port_var.set(ports[0])
        elif not ports:
            self.port_var.set("")
            self.status_var.set("No serial ports found.")

    def toggle_connection(self):
        if self.ser and self.ser.is_open:
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        port = self.port_var.get().strip()
        if not port:
            messagebox.showerror("Serial error", "Select a serial port.")
            return

        try:
            self.ser = serial.Serial(port, BAUD_RATE, timeout=0.5)
            time.sleep(2.0)
            self.ser.reset_input_buffer()
        except serial.SerialException as exc:
            messagebox.showerror("Serial error", str(exc))
            self.ser = None
            return

        self.stop_event.clear()
        threading.Thread(target=self.reader, daemon=True).start()
        self.connect_btn.config(text="Disconnect")
        self.conn_var.set(f"Connected to {port} at {BAUD_RATE} baud")
        self.status_var.set("Waiting for telemetry...")

    def disconnect(self):
        self.stop_event.set()
        if self.ser:
            try:
                self.ser.close()
            except serial.SerialException:
                pass
        self.ser = None
        self.connect_btn.config(text="Connect")
        self.conn_var.set("Disconnected")

    def reader(self):
        while not self.stop_event.is_set() and self.ser:
            try:
                line = self.ser.readline().decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                parsed = self.parse_line(line)
                if parsed is None:
                    self.q.put(("status", f"Unrecognized line: {line}"))
                else:
                    self.q.put(("data", parsed))
            except (serial.SerialException, OSError) as exc:
                self.q.put(("status", f"Serial error: {exc}"))
                break

    @staticmethod
    def parse_line(line):
        try:
            fields = {}
            for part in line.split(","):
                key, value = part.split("=", 1)
                fields[key.strip()] = value.strip()

            analog = int(fields["ANALOG_SIGNAL"])
            digital = int(fields["DIGITAL_SIGNAL"])

            if not 0 <= analog <= ADC_MAX or digital not in (0, 1):
                return None

            return analog, digital
        except (ValueError, KeyError):
            return None

    def process_queue(self):
        try:
            while True:
                kind, payload = self.q.get_nowait()
                if kind == "data":
                    self.update_values(*payload)
                else:
                    self.status_var.set(payload)
        except queue.Empty:
            pass

        self.root.after(50, self.process_queue)

    def update_values(self, analog, digital):
        voltage = analog * VREF / ADC_MAX
        temperature = self.estimate_temperature(analog)

        self.adc_var.set(f"{analog} / {ADC_MAX}")
        self.voltage_var.set(f"{voltage:.3f} V")
        self.digital_var.set("HIGH (1)" if digital else "LOW (0)")
        self.fan_var.set("ON" if digital else "OFF")
        self.temp_var.set(
            f"{temperature:.1f} °C" if temperature is not None else "Calibration error"
        )
        self.status_var.set("Telemetry received.")

    def estimate_temperature(self, adc):
        try:
            t1 = float(self.t1_var.get())
            a1 = int(self.a1_var.get())
            t2 = float(self.t2_var.get())
            a2 = int(self.a2_var.get())
        except (tk.TclError, ValueError):
            return None

        if a1 == a2:
            return None

        return t1 + (adc - a1) * (t2 - t1) / (a2 - a1)

    def close(self):
        self.disconnect()
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    App(root)
    root.mainloop()
