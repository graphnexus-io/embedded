#!/usr/bin/env python3
import queue
import threading
import time
import tkinter as tk
from tkinter import messagebox, ttk

import serial
from serial.tools import list_ports

BAUD_RATE = 9600
ADC_MAX = 1023
UPDATE_INTERVAL_MS = 50

BACKGROUND = "#080d18"
PANEL = "#111a2b"
PANEL_LIGHT = "#18243a"
TEXT = "#f4f7fb"
MUTED = "#92a2ba"
ACCENT = "#48b8ff"
RED = "#ff3b4f"
GREEN = "#28d17c"
TRACK = "#24334d"


class InfraredDetectorApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Near-Infrared Detector")
        self.root.geometry("1100x680")
        self.root.minsize(900, 600)
        self.root.configure(bg=BACKGROUND)

        self.serial_connection = None
        self.stop_event = threading.Event()
        self.data_queue = queue.Queue()

        self.port_var = tk.StringVar()
        self.connection_var = tk.StringVar(value="DISCONNECTED")
        self.intensity_var = tk.StringVar(value="---")
        self.percent_var = tk.StringVar(value="0.0% of ADC range")
        self.digital_var = tk.StringVar(value="WAITING FOR DATA")
        self.status_var = tk.StringVar(value="Select a serial port to begin")

        self.configure_styles()
        self.build_ui()
        self.refresh_ports()
        self.process_queue()
        self.root.protocol("WM_DELETE_WINDOW", self.close)

    def configure_styles(self):
        style = ttk.Style()
        style.theme_use("clam")
        style.configure(
            "Dark.TCombobox",
            fieldbackground=PANEL_LIGHT,
            background=PANEL_LIGHT,
            foreground=TEXT,
            arrowcolor=TEXT,
            bordercolor="#30415f",
            lightcolor="#30415f",
            darkcolor="#30415f",
            padding=8,
        )
        style.map(
            "Dark.TCombobox",
            fieldbackground=[("readonly", PANEL_LIGHT)],
            foreground=[("readonly", TEXT)],
            selectbackground=[("readonly", PANEL_LIGHT)],
            selectforeground=[("readonly", TEXT)],
        )
        style.configure(
            "Accent.TButton",
            font=("TkDefaultFont", 11, "bold"),
            foreground="#06101c",
            background=ACCENT,
            borderwidth=0,
            padding=(18, 10),
        )
        style.map("Accent.TButton", background=[("active", "#79cbff")])
        style.configure(
            "Secondary.TButton",
            font=("TkDefaultFont", 11),
            foreground=TEXT,
            background=PANEL_LIGHT,
            borderwidth=0,
            padding=(14, 10),
        )
        style.map("Secondary.TButton", background=[("active", "#263754")])

    def build_ui(self):
        header = tk.Frame(self.root, bg=BACKGROUND)
        header.pack(fill="x", padx=36, pady=(28, 18))

        title_block = tk.Frame(header, bg=BACKGROUND)
        title_block.pack(side="left")
        tk.Label(
            title_block,
            text="NEAR-INFRARED DETECTOR",
            font=("TkDefaultFont", 25, "bold"),
            fg=TEXT,
            bg=BACKGROUND,
        ).pack(anchor="w")
        tk.Label(
            title_block,
            text="Live analog intensity and digital threshold monitor",
            font=("TkDefaultFont", 12),
            fg=MUTED,
            bg=BACKGROUND,
        ).pack(anchor="w", pady=(5, 0))

        connection = tk.Frame(header, bg=BACKGROUND)
        connection.pack(side="right", pady=4)
        self.connection_dot = tk.Canvas(
            connection, width=14, height=14, bg=BACKGROUND, highlightthickness=0
        )
        self.connection_dot.pack(side="left", padx=(0, 8))
        self.connection_dot.create_oval(2, 2, 12, 12, fill=MUTED, outline="")
        tk.Label(
            connection,
            textvariable=self.connection_var,
            font=("TkDefaultFont", 10, "bold"),
            fg=MUTED,
            bg=BACKGROUND,
        ).pack(side="left")

        serial_panel = tk.Frame(
            self.root, bg=PANEL, highlightbackground="#24334d", highlightthickness=1
        )
        serial_panel.pack(fill="x", padx=36, pady=(0, 18))

        tk.Label(
            serial_panel,
            text="SERIAL PORT",
            font=("TkDefaultFont", 9, "bold"),
            fg=MUTED,
            bg=PANEL,
        ).pack(side="left", padx=(20, 12), pady=16)
        self.port_box = ttk.Combobox(
            serial_panel,
            textvariable=self.port_var,
            state="readonly",
            width=26,
            style="Dark.TCombobox",
        )
        self.port_box.pack(side="left", padx=(0, 10), pady=10)
        ttk.Button(
            serial_panel,
            text="Refresh",
            command=self.refresh_ports,
            style="Secondary.TButton",
        ).pack(side="left", padx=4, pady=10)
        self.connect_button = ttk.Button(
            serial_panel,
            text="Connect",
            command=self.toggle_connection,
            style="Accent.TButton",
        )
        self.connect_button.pack(side="left", padx=8, pady=10)
        tk.Label(
            serial_panel,
            textvariable=self.status_var,
            font=("TkDefaultFont", 10),
            fg=MUTED,
            bg=PANEL,
        ).pack(side="right", padx=20)

        content = tk.Frame(self.root, bg=BACKGROUND)
        content.pack(fill="both", expand=True, padx=36, pady=(0, 30))
        content.grid_columnconfigure(0, weight=3, uniform="cards")
        content.grid_columnconfigure(1, weight=2, uniform="cards")
        content.grid_rowconfigure(0, weight=1)

        intensity_card = self.make_card(content)
        intensity_card.grid(row=0, column=0, sticky="nsew", padx=(0, 10))
        detection_card = self.make_card(content)
        detection_card.grid(row=0, column=1, sticky="nsew", padx=(10, 0))

        tk.Label(
            intensity_card,
            text="INFRARED INTENSITY",
            font=("TkDefaultFont", 11, "bold"),
            fg=MUTED,
            bg=PANEL,
        ).pack(anchor="w", padx=30, pady=(28, 0))

        value_row = tk.Frame(intensity_card, bg=PANEL)
        value_row.pack(anchor="w", padx=30, pady=(42, 8))
        tk.Label(
            value_row,
            textvariable=self.intensity_var,
            font=("TkDefaultFont", 58, "bold"),
            fg=TEXT,
            bg=PANEL,
        ).pack(side="left", anchor="s")
        tk.Label(
            value_row,
            text=f" / {ADC_MAX} ADC",
            font=("TkDefaultFont", 17),
            fg=MUTED,
            bg=PANEL,
        ).pack(side="left", anchor="s", pady=12)

        self.gauge = tk.Canvas(
            intensity_card,
            height=42,
            bg=PANEL,
            highlightthickness=0,
        )
        self.gauge.pack(fill="x", padx=32, pady=(26, 3))
        self.gauge.bind("<Configure>", self.redraw_gauge)
        self.current_intensity = 0

        tk.Label(
            intensity_card,
            textvariable=self.percent_var,
            font=("TkDefaultFont", 13),
            fg=ACCENT,
            bg=PANEL,
        ).pack(anchor="w", padx=32, pady=(8, 0))

        tk.Label(
            detection_card,
            text="DIGITAL SIGNAL",
            font=("TkDefaultFont", 11, "bold"),
            fg=MUTED,
            bg=PANEL,
        ).pack(anchor="w", padx=30, pady=(28, 0))

        self.indicator = tk.Canvas(
            detection_card,
            width=190,
            height=190,
            bg=PANEL,
            highlightthickness=0,
        )
        self.indicator.pack(pady=(36, 15))
        self.indicator_glow = self.indicator.create_oval(
            7, 7, 183, 183, fill="#283347", outline=""
        )
        self.indicator_circle = self.indicator.create_oval(
            25, 25, 165, 165, fill=MUTED, outline="#a7b1c0", width=3
        )
        self.indicator_text = self.indicator.create_text(
            95,
            95,
            text="—",
            fill=TEXT,
            font=("TkDefaultFont", 30, "bold"),
        )

        tk.Label(
            detection_card,
            text="INFRARED SOURCE DETECTED",
            font=("TkDefaultFont", 14, "bold"),
            fg=TEXT,
            bg=PANEL,
        ).pack()
        self.digital_label = tk.Label(
            detection_card,
            textvariable=self.digital_var,
            font=("TkDefaultFont", 12, "bold"),
            fg=MUTED,
            bg=PANEL,
        )
        self.digital_label.pack(pady=(9, 0))

        tk.Label(
            detection_card,
            text="HIGH = red   •   LOW = green",
            font=("TkDefaultFont", 10),
            fg=MUTED,
            bg=PANEL,
        ).pack(side="bottom", pady=25)

    @staticmethod
    def make_card(parent):
        return tk.Frame(
            parent, bg=PANEL, highlightbackground="#24334d", highlightthickness=1
        )

    def refresh_ports(self):
        ports = [port.device for port in list_ports.comports()]
        self.port_box["values"] = ports
        if ports and self.port_var.get() not in ports:
            self.port_var.set(ports[0])
            self.status_var.set("Ready to connect")
        elif not ports:
            self.port_var.set("")
            self.status_var.set("No serial ports found")

    def toggle_connection(self):
        if self.serial_connection and self.serial_connection.is_open:
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        port = self.port_var.get().strip()
        if not port:
            messagebox.showerror("Serial error", "Select a serial port first.")
            return

        try:
            self.serial_connection = serial.Serial(port, BAUD_RATE, timeout=0.5)
            time.sleep(2.0)
            self.serial_connection.reset_input_buffer()
        except serial.SerialException as exc:
            messagebox.showerror("Serial error", str(exc))
            self.serial_connection = None
            return

        self.stop_event.clear()
        threading.Thread(target=self.read_serial, daemon=True).start()
        self.connect_button.configure(text="Disconnect")
        self.connection_var.set("CONNECTED")
        self.connection_dot.itemconfigure(1, fill=GREEN)
        self.status_var.set(f"Listening on {port} at {BAUD_RATE} baud")

    def disconnect(self):
        self.stop_event.set()
        if self.serial_connection:
            try:
                self.serial_connection.close()
            except serial.SerialException:
                pass
        self.serial_connection = None
        self.connect_button.configure(text="Connect")
        self.connection_var.set("DISCONNECTED")
        self.connection_dot.itemconfigure(1, fill=MUTED)
        self.status_var.set("Serial connection closed")

    def read_serial(self):
        while not self.stop_event.is_set() and self.serial_connection:
            try:
                line = (
                    self.serial_connection.readline()
                    .decode("utf-8", errors="replace")
                    .strip()
                )
                if not line:
                    continue
                parsed = self.parse_line(line)
                if parsed is None:
                    self.data_queue.put(("status", f"Unrecognized data: {line}"))
                else:
                    self.data_queue.put(("data", parsed))
            except (serial.SerialException, OSError) as exc:
                self.data_queue.put(("error", f"Serial error: {exc}"))
                break

    @staticmethod
    def parse_line(line):
        try:
            fields = {}
            for part in line.split(","):
                key, value = part.split("=", 1)
                fields[key.strip()] = value.strip()

            intensity = int(fields["INFRARED_INTENSITY"])
            digital = int(fields["DIGITAL_SIGNAL"])
            if not 0 <= intensity <= ADC_MAX or digital not in (0, 1):
                return None
            return intensity, digital
        except (KeyError, ValueError):
            return None

    def process_queue(self):
        try:
            while True:
                event, payload = self.data_queue.get_nowait()
                if event == "data":
                    self.update_display(*payload)
                else:
                    self.status_var.set(payload)
                    if event == "error":
                        self.disconnect()
        except queue.Empty:
            pass
        self.root.after(UPDATE_INTERVAL_MS, self.process_queue)

    def update_display(self, intensity, digital):
        self.current_intensity = intensity
        percentage = intensity * 100.0 / ADC_MAX
        self.intensity_var.set(str(intensity))
        self.percent_var.set(f"{percentage:.1f}% of ADC range")
        self.redraw_gauge()

        color = RED if digital else GREEN
        glow = "#632334" if digital else "#17573f"
        state = "HIGH (1)" if digital else "LOW (0)"
        self.indicator.itemconfigure(self.indicator_glow, fill=glow)
        self.indicator.itemconfigure(
            self.indicator_circle, fill=color, outline="#ff9ca7" if digital else "#8ff5bd"
        )
        self.indicator.itemconfigure(self.indicator_text, text="HIGH" if digital else "LOW")
        self.digital_var.set(state)
        self.digital_label.configure(fg=color)
        self.status_var.set("Receiving live telemetry")

    def redraw_gauge(self, _event=None):
        self.gauge.delete("all")
        width = max(self.gauge.winfo_width(), 20)
        bar_left, bar_right = 3, width - 3
        bar_top, bar_bottom = 12, 30
        self.gauge.create_rectangle(
            bar_left, bar_top, bar_right, bar_bottom, fill=TRACK, outline=""
        )
        fill_right = bar_left + (bar_right - bar_left) * self.current_intensity / ADC_MAX
        if fill_right > bar_left:
            self.gauge.create_rectangle(
                bar_left, bar_top, fill_right, bar_bottom, fill=ACCENT, outline=""
            )
        for fraction in (0.25, 0.5, 0.75):
            x = bar_left + (bar_right - bar_left) * fraction
            self.gauge.create_line(x, bar_top, x, bar_bottom, fill=PANEL, width=2)

    def close(self):
        self.disconnect()
        self.root.destroy()


if __name__ == "__main__":
    window = tk.Tk()
    InfraredDetectorApp(window)
    window.mainloop()
