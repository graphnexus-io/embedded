#!/usr/bin/env python3

from __future__ import annotations

import queue
import threading
import time
import tkinter as tk
from tkinter import messagebox, ttk
from typing import Optional

import serial
import serial.tools.list_ports


class StepperGui:
    BAUD_RATE = 115200
    GUI_QUEUE_INTERVAL_MS = 50
    ARDUINO_RESET_DELAY_S = 1.8

    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("Arduino Stepper Motor and Encoder Controller")
        self.root.geometry("1120x900")
        self.root.minsize(920, 700)

        # Serial communication
        self.ser: Optional[serial.Serial] = None
        self.reader_thread: Optional[threading.Thread] = None
        self.reader_running = threading.Event()
        self.receive_queue: queue.Queue[tuple[str, str]] = queue.Queue()
        self.write_lock = threading.Lock()

        # Connection variables
        self.port_var = tk.StringVar()
        self.connection_status_var = tk.StringVar(value="Disconnected")

        # Motion variables
        self.pulses_var = tk.StringVar(value="800")
        self.direction_var = tk.StringVar(value="CW")
        self.speed_var = tk.StringVar(value="120")

        # Controller configuration
        self.acceleration_var = tk.StringVar(value="2000")
        self.motor_ppr_var = tk.StringVar(value="800")
        self.encoder_cpr_var = tk.StringVar(value="4000")
        self.tolerance_var = tk.StringVar(value="40")
        self.encoder_sign_var = tk.StringVar(value="1")
        self.stream_interval_var = tk.StringVar(value="100")
        self.maximum_rpm_var = tk.StringVar(value="600")

        # Live metrics
        self.state_var = tk.StringVar(value="UNKNOWN")
        self.driver_enabled_var = tk.StringVar(value="No")

        self.command_position_var = tk.StringVar(value="0")
        self.target_position_var = tk.StringVar(value="0")
        self.encoder_position_var = tk.StringVar(value="0")
        self.expected_encoder_var = tk.StringVar(value="0")
        self.position_error_var = tk.StringVar(value="0")
        self.remaining_pulses_var = tk.StringVar(value="0")

        self.command_revolutions_var = tk.StringVar(value="0.0000")
        self.encoder_revolutions_var = tk.StringVar(value="0.0000")

        self.command_rpm_var = tk.StringVar(value="0.00")
        self.encoder_rpm_var = tk.StringVar(value="0.00")

        self.invalid_transitions_var = tk.StringVar(value="0")
        self.current_tolerance_var = tk.StringVar(value="40")

        self.error_status_var = tk.StringVar(value="Position error: unknown")
        self.encoder_status_var = tk.StringVar(value="Encoder signal: unknown")

        self.build_ui()
        self.refresh_ports()

        self.root.after(
            self.GUI_QUEUE_INTERVAL_MS,
            self.process_receive_queue,
        )

        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    # =========================================================
    # UI construction
    # =========================================================

    def build_ui(self) -> None:
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)

        self.main_pane = ttk.Panedwindow(
            self.root,
            orient=tk.VERTICAL,
        )
        self.main_pane.grid(
            row=0,
            column=0,
            sticky="nsew",
            padx=12,
            pady=12,
        )

        upper_frame = ttk.Frame(self.main_pane)
        upper_frame.columnconfigure(0, weight=1)
        upper_frame.columnconfigure(1, weight=1)

        self.build_connection_frame(upper_frame)
        self.build_motion_frame(upper_frame)
        self.build_metrics_frame(upper_frame)
        self.build_configuration_frame(upper_frame)

        log_container = ttk.Frame(self.main_pane)
        log_container.columnconfigure(0, weight=1)
        log_container.rowconfigure(0, weight=1)

        self.build_log_frame(log_container)

        self.main_pane.add(
            upper_frame,
            weight=3,
        )
        self.main_pane.add(
            log_container,
            weight=2,
        )

    def build_connection_frame(self, parent: ttk.Frame) -> None:
        frame = ttk.LabelFrame(
            parent,
            text="Serial connection",
            padding=10,
        )
        frame.grid(
            row=0,
            column=0,
            columnspan=2,
            sticky="ew",
            pady=(0, 10),
        )

        frame.columnconfigure(1, weight=1)

        ttk.Label(
            frame,
            text="Serial port:",
        ).grid(
            row=0,
            column=0,
            sticky="w",
        )

        self.port_box = ttk.Combobox(
            frame,
            textvariable=self.port_var,
            state="readonly",
            width=30,
        )
        self.port_box.grid(
            row=0,
            column=1,
            sticky="ew",
            padx=8,
        )

        ttk.Button(
            frame,
            text="Refresh",
            command=self.refresh_ports,
        ).grid(
            row=0,
            column=2,
            padx=4,
        )

        self.connect_button = ttk.Button(
            frame,
            text="Connect",
            command=self.toggle_connection,
        )
        self.connect_button.grid(
            row=0,
            column=3,
            padx=4,
        )

        ttk.Label(
            frame,
            textvariable=self.connection_status_var,
        ).grid(
            row=0,
            column=4,
            sticky="w",
            padx=(12, 0),
        )

    def build_motion_frame(self, parent: ttk.Frame) -> None:
        frame = ttk.LabelFrame(
            parent,
            text="Motion control",
            padding=12,
        )
        frame.grid(
            row=1,
            column=0,
            sticky="nsew",
            padx=(0, 5),
            pady=(0, 10),
        )

        frame.columnconfigure(1, weight=1)

        ttk.Label(
            frame,
            text="Motor pulses:",
        ).grid(
            row=0,
            column=0,
            sticky="w",
            pady=4,
        )

        ttk.Entry(
            frame,
            textvariable=self.pulses_var,
        ).grid(
            row=0,
            column=1,
            sticky="ew",
            padx=(8, 0),
            pady=4,
        )

        ttk.Label(
            frame,
            text="Direction:",
        ).grid(
            row=1,
            column=0,
            sticky="w",
            pady=4,
        )

        ttk.Combobox(
            frame,
            textvariable=self.direction_var,
            values=("CW", "CCW"),
            state="readonly",
        ).grid(
            row=1,
            column=1,
            sticky="ew",
            padx=(8, 0),
            pady=4,
        )

        ttk.Label(
            frame,
            text="Speed, RPM:",
        ).grid(
            row=2,
            column=0,
            sticky="w",
            pady=4,
        )

        ttk.Entry(
            frame,
            textvariable=self.speed_var,
        ).grid(
            row=2,
            column=1,
            sticky="ew",
            padx=(8, 0),
            pady=4,
        )

        self.start_button = ttk.Button(
            frame,
            text="Start movement",
            command=self.send_move,
        )
        self.start_button.grid(
            row=3,
            column=0,
            columnspan=2,
            sticky="ew",
            pady=(12, 4),
        )

        stop_frame = ttk.Frame(frame)
        stop_frame.grid(
            row=4,
            column=0,
            columnspan=2,
            sticky="ew",
            pady=4,
        )

        stop_frame.columnconfigure(0, weight=1)
        stop_frame.columnconfigure(1, weight=1)

        ttk.Button(
            stop_frame,
            text="Controlled stop",
            command=lambda: self.send_command("STOP"),
        ).grid(
            row=0,
            column=0,
            sticky="ew",
            padx=(0, 4),
        )

        self.estop_button = tk.Button(
            stop_frame,
            text="EMERGENCY STOP",
            command=lambda: self.send_command("ESTOP"),
            font=("TkDefaultFont", 10, "bold"),
        )
        self.estop_button.grid(
            row=0,
            column=1,
            sticky="ew",
            padx=(4, 0),
        )

        driver_frame = ttk.Frame(frame)
        driver_frame.grid(
            row=5,
            column=0,
            columnspan=2,
            sticky="ew",
            pady=(10, 4),
        )

        for column in range(4):
            driver_frame.columnconfigure(column, weight=1)

        ttk.Button(
            driver_frame,
            text="Enable",
            command=lambda: self.send_command("ENABLE"),
        ).grid(
            row=0,
            column=0,
            sticky="ew",
            padx=2,
        )

        ttk.Button(
            driver_frame,
            text="Disable",
            command=lambda: self.send_command("DISABLE"),
        ).grid(
            row=0,
            column=1,
            sticky="ew",
            padx=2,
        )

        ttk.Button(
            driver_frame,
            text="Zero",
            command=lambda: self.send_command("ZERO"),
        ).grid(
            row=0,
            column=2,
            sticky="ew",
            padx=2,
        )

        ttk.Button(
            driver_frame,
            text="Clear fault",
            command=lambda: self.send_command("CLEAR_FAULT"),
        ).grid(
            row=0,
            column=3,
            sticky="ew",
            padx=2,
        )

        status_frame = ttk.Frame(frame)
        status_frame.grid(
            row=6,
            column=0,
            columnspan=2,
            sticky="ew",
            pady=(12, 0),
        )

        status_frame.columnconfigure(1, weight=1)

        ttk.Label(
            status_frame,
            text="Controller state:",
        ).grid(
            row=0,
            column=0,
            sticky="w",
        )

        self.state_label = tk.Label(
            status_frame,
            textvariable=self.state_var,
            anchor="w",
            font=("TkDefaultFont", 11, "bold"),
        )
        self.state_label.grid(
            row=0,
            column=1,
            sticky="ew",
            padx=(8, 0),
        )

        ttk.Label(
            status_frame,
            text="Driver enabled:",
        ).grid(
            row=1,
            column=0,
            sticky="w",
            pady=(4, 0),
        )

        ttk.Label(
            status_frame,
            textvariable=self.driver_enabled_var,
        ).grid(
            row=1,
            column=1,
            sticky="w",
            padx=(8, 0),
            pady=(4, 0),
        )

    def build_metrics_frame(self, parent: ttk.Frame) -> None:
        frame = ttk.LabelFrame(
            parent,
            text="Live motor and encoder metrics",
            padding=12,
        )
        frame.grid(
            row=1,
            column=1,
            sticky="nsew",
            padx=(5, 0),
            pady=(0, 10),
        )

        frame.columnconfigure(1, weight=1)
        frame.columnconfigure(3, weight=1)

        metrics = (
            ("Command position:", self.command_position_var),
            ("Target position:", self.target_position_var),
            ("Encoder position:", self.encoder_position_var),
            ("Expected encoder:", self.expected_encoder_var),
            ("Position error:", self.position_error_var),
            ("Remaining pulses:", self.remaining_pulses_var),
            ("Command revolutions:", self.command_revolutions_var),
            ("Encoder revolutions:", self.encoder_revolutions_var),
            ("Command RPM:", self.command_rpm_var),
            ("Measured RPM:", self.encoder_rpm_var),
            ("Invalid transitions:", self.invalid_transitions_var),
            ("Error tolerance:", self.current_tolerance_var),
        )

        for index, (label_text, variable) in enumerate(metrics):
            row = index // 2
            group = index % 2

            label_column = group * 2
            value_column = label_column + 1

            ttk.Label(
                frame,
                text=label_text,
            ).grid(
                row=row,
                column=label_column,
                sticky="w",
                padx=(0, 8),
                pady=4,
            )

            ttk.Label(
                frame,
                textvariable=variable,
                font=("TkFixedFont", 10, "bold"),
            ).grid(
                row=row,
                column=value_column,
                sticky="w",
                padx=(0, 16),
                pady=4,
            )

        self.error_status_label = tk.Label(
            frame,
            textvariable=self.error_status_var,
            anchor="w",
            font=("TkDefaultFont", 10, "bold"),
        )
        self.error_status_label.grid(
            row=6,
            column=0,
            columnspan=4,
            sticky="ew",
            pady=(12, 4),
        )

        self.encoder_status_label = tk.Label(
            frame,
            textvariable=self.encoder_status_var,
            anchor="w",
            font=("TkDefaultFont", 10, "bold"),
        )
        self.encoder_status_label.grid(
            row=7,
            column=0,
            columnspan=4,
            sticky="ew",
            pady=4,
        )

        button_frame = ttk.Frame(frame)
        button_frame.grid(
            row=8,
            column=0,
            columnspan=4,
            sticky="ew",
            pady=(12, 0),
        )

        button_frame.columnconfigure(0, weight=1)
        button_frame.columnconfigure(1, weight=1)

        ttk.Button(
            button_frame,
            text="Request current status",
            command=lambda: self.send_command("STATUS"),
        ).grid(
            row=0,
            column=0,
            sticky="ew",
            padx=(0, 4),
        )

        ttk.Button(
            button_frame,
            text="Clear encoder errors",
            command=lambda: self.send_command(
                "CLEAR_ENCODER_ERRORS"
            ),
        ).grid(
            row=0,
            column=1,
            sticky="ew",
            padx=(4, 0),
        )

    def build_configuration_frame(self, parent: ttk.Frame) -> None:
        frame = ttk.LabelFrame(
            parent,
            text="Controller configuration",
            padding=12,
        )
        frame.grid(
            row=2,
            column=0,
            columnspan=2,
            sticky="ew",
            pady=(0, 5),
        )

        for column in range(6):
            frame.columnconfigure(column, weight=1)

        fields = (
            (
                "Acceleration, pulses/s²:",
                self.acceleration_var,
                self.apply_acceleration,
            ),
            (
                "Motor pulses/revolution:",
                self.motor_ppr_var,
                self.apply_motor_ppr,
            ),
            (
                "Encoder counts/revolution:",
                self.encoder_cpr_var,
                self.apply_encoder_cpr,
            ),
            (
                "Error tolerance, counts:",
                self.tolerance_var,
                self.apply_tolerance,
            ),
            (
                "Encoder direction sign:",
                self.encoder_sign_var,
                self.apply_encoder_sign,
            ),
            (
                "Telemetry interval, ms:",
                self.stream_interval_var,
                self.apply_stream_interval,
            ),
        )

        for index, (label_text, variable, command) in enumerate(fields):
            group_column = (index % 3) * 2
            base_row = (index // 3) * 2

            ttk.Label(
                frame,
                text=label_text,
            ).grid(
                row=base_row,
                column=group_column,
                columnspan=2,
                sticky="w",
                padx=4,
                pady=(2, 0),
            )

            ttk.Entry(
                frame,
                textvariable=variable,
                width=15,
            ).grid(
                row=base_row + 1,
                column=group_column,
                sticky="ew",
                padx=(4, 2),
                pady=(2, 8),
            )

            ttk.Button(
                frame,
                text="Apply",
                command=command,
            ).grid(
                row=base_row + 1,
                column=group_column + 1,
                sticky="ew",
                padx=(2, 4),
                pady=(2, 8),
            )

        extra_frame = ttk.Frame(frame)
        extra_frame.grid(
            row=4,
            column=0,
            columnspan=6,
            sticky="ew",
            pady=(4, 0),
        )

        for column in range(5):
            extra_frame.columnconfigure(column, weight=1)

        ttk.Button(
            extra_frame,
            text="Start telemetry",
            command=lambda: self.send_command("STREAM ON"),
        ).grid(
            row=0,
            column=0,
            sticky="ew",
            padx=3,
        )

        ttk.Button(
            extra_frame,
            text="Stop telemetry",
            command=lambda: self.send_command("STREAM OFF"),
        ).grid(
            row=0,
            column=1,
            sticky="ew",
            padx=3,
        )

        ttk.Button(
            extra_frame,
            text="Read configuration",
            command=lambda: self.send_command("CONFIG"),
        ).grid(
            row=0,
            column=2,
            sticky="ew",
            padx=3,
        )

        ttk.Button(
            extra_frame,
            text="Apply all settings",
            command=self.apply_all_settings,
        ).grid(
            row=0,
            column=3,
            sticky="ew",
            padx=3,
        )

        ttk.Button(
            extra_frame,
            text="Reset display",
            command=self.reset_display_metrics,
        ).grid(
            row=0,
            column=4,
            sticky="ew",
            padx=3,
        )

    def build_log_frame(self, parent: ttk.Frame) -> None:
        frame = ttk.LabelFrame(
            parent,
            text="Serial log",
            padding=8,
        )
        frame.grid(
            row=0,
            column=0,
            sticky="nsew",
        )

        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(0, weight=1)

        self.output = tk.Text(
            frame,
            width=100,
            height=14,
            wrap="none",
            font=("TkFixedFont", 9),
        )
        self.output.grid(
            row=0,
            column=0,
            sticky="nsew",
        )

        vertical_scrollbar = ttk.Scrollbar(
            frame,
            orient=tk.VERTICAL,
            command=self.output.yview,
        )
        vertical_scrollbar.grid(
            row=0,
            column=1,
            sticky="ns",
        )

        horizontal_scrollbar = ttk.Scrollbar(
            frame,
            orient=tk.HORIZONTAL,
            command=self.output.xview,
        )
        horizontal_scrollbar.grid(
            row=1,
            column=0,
            sticky="ew",
        )

        self.output.configure(
            yscrollcommand=vertical_scrollbar.set,
            xscrollcommand=horizontal_scrollbar.set,
        )

        controls = ttk.Frame(frame)
        controls.grid(
            row=2,
            column=0,
            columnspan=2,
            sticky="ew",
            pady=(8, 0),
        )

        ttk.Button(
            controls,
            text="Clear log",
            command=self.clear_log,
        ).pack(
            side=tk.RIGHT,
        )

        ttk.Button(
            controls,
            text="Request status",
            command=lambda: self.send_command("STATUS"),
        ).pack(
            side=tk.RIGHT,
            padx=(0, 8),
        )

        ttk.Button(
            controls,
            text="Help",
            command=lambda: self.send_command("HELP"),
        ).pack(
            side=tk.RIGHT,
            padx=(0, 8),
        )

    # =========================================================
    # Serial port handling
    # =========================================================

    def refresh_ports(self) -> None:
        ports = sorted(
            port.device
            for port in serial.tools.list_ports.comports()
        )

        self.port_box["values"] = ports

        current_port = self.port_var.get()

        preferred_ports = (
            "/dev/ttyUSB0",
            "/dev/ttyACM0",
        )

        for preferred in preferred_ports:
            if preferred in ports:
                self.port_var.set(preferred)
                return

        if current_port in ports:
            self.port_var.set(current_port)
        elif ports:
            self.port_var.set(ports[0])
        else:
            self.port_var.set("")

    def toggle_connection(self) -> None:
        if self.is_connected():
            self.disconnect()
        else:
            self.connect()

    def connect(self) -> None:
        port = self.port_var.get().strip()

        if not port:
            messagebox.showerror(
                "Connection error",
                "Select a serial port first.",
            )
            return

        try:
            self.ser = serial.Serial(
                port=port,
                baudrate=self.BAUD_RATE,
                timeout=0.2,
                write_timeout=1.0,
            )

            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()

        except serial.SerialException as error:
            self.ser = None

            messagebox.showerror(
                "Serial error",
                str(error),
            )

            self.connection_status_var.set(
                "Connection failed"
            )
            return

        self.reader_running.set()

        self.reader_thread = threading.Thread(
            target=self.serial_reader,
            daemon=True,
        )
        self.reader_thread.start()

        self.connection_status_var.set(
            f"Connected to {port}"
        )
        self.connect_button.configure(
            text="Disconnect"
        )

        self.log(
            f"Connected to {port} at "
            f"{self.BAUD_RATE} baud"
        )

        self.root.after(
            int(self.ARDUINO_RESET_DELAY_S * 1000),
            self.initialize_controller,
        )

    def disconnect(self) -> None:
        self.reader_running.clear()

        serial_connection = self.ser
        self.ser = None

        if serial_connection is not None:
            try:
                serial_connection.close()
            except serial.SerialException:
                pass

        self.connection_status_var.set("Disconnected")
        self.connect_button.configure(text="Connect")

        self.state_var.set("UNKNOWN")
        self.driver_enabled_var.set("No")
        self.update_state_display()

        self.log("Disconnected")

    def initialize_controller(self) -> None:
        if not self.is_connected():
            return

        self.send_command("STREAM ON")
        self.send_command(
            f"SET_STREAM_MS {self.stream_interval_var.get()}"
        )
        self.send_command("STATUS")
        self.send_command("CONFIG")

    def serial_reader(self) -> None:
        while self.reader_running.is_set():
            serial_connection = self.ser

            if serial_connection is None:
                break

            try:
                raw_line = serial_connection.readline()

            except serial.SerialException as error:
                self.receive_queue.put(
                    ("serial_error", str(error))
                )
                break

            if not raw_line:
                continue

            line = raw_line.decode(
                "ascii",
                errors="replace",
            ).strip()

            if line:
                self.receive_queue.put(
                    ("line", line)
                )

    def process_receive_queue(self) -> None:
        try:
            while True:
                event_type, content = (
                    self.receive_queue.get_nowait()
                )

                if event_type == "serial_error":
                    self.log(
                        f"< SERIAL ERROR: {content}"
                    )
                    self.connection_status_var.set(
                        "Serial communication error"
                    )
                    continue

                self.log(f"< {content}")
                self.process_controller_line(content)

        except queue.Empty:
            pass

        self.root.after(
            self.GUI_QUEUE_INTERVAL_MS,
            self.process_receive_queue,
        )

    # =========================================================
    # Serial response parsing
    # =========================================================

    def process_controller_line(self, line: str) -> None:
        if line.startswith("DATA"):
            values = self.parse_key_value_line(line)
            self.update_metrics(values)
            return

        if line.startswith("CONFIG"):
            values = self.parse_key_value_line(line)
            self.update_configuration(values)
            return

        if line.startswith("FAULT"):
            values = self.parse_key_value_line(line)

            self.state_var.set("FAULT")

            if "ERROR" in values:
                self.position_error_var.set(values["ERROR"])

            if "TOLERANCE" in values:
                self.current_tolerance_var.set(
                    values["TOLERANCE"]
                )

            self.update_state_display()
            self.update_error_display()
            return

        if line.startswith("DONE"):
            values = self.parse_key_value_line(line)

            self.state_var.set("IDLE")

            if "CMD_POS" in values:
                self.command_position_var.set(
                    values["CMD_POS"]
                )

            if "ENC_POS" in values:
                self.encoder_position_var.set(
                    values["ENC_POS"]
                )

            if "EXPECTED_ENC" in values:
                self.expected_encoder_var.set(
                    values["EXPECTED_ENC"]
                )

            if "ERROR" in values:
                self.position_error_var.set(
                    values["ERROR"]
                )

            self.update_state_display()
            self.update_error_display()
            return

        if line.startswith("OK MOVE"):
            self.state_var.set("RUNNING")
            self.update_state_display()
            return

        if line.startswith("OK STOPPING"):
            self.state_var.set("STOPPING")
            self.update_state_display()
            return

        if line.startswith("OK ESTOP"):
            self.state_var.set("ESTOPPED")
            self.update_state_display()
            return

        if line.startswith("OK ENABLED"):
            self.driver_enabled_var.set("Yes")
            return

        if line.startswith("OK DISABLED"):
            self.driver_enabled_var.set("No")
            return

        if line.startswith("OK ZERO"):
            self.reset_display_metrics()
            self.state_var.set("IDLE")
            self.update_state_display()
            return

        if line.startswith("OK FAULT_CLEARED"):
            self.state_var.set("IDLE")
            self.update_state_display()
            return

    @staticmethod
    def parse_key_value_line(line: str) -> dict[str, str]:
        values: dict[str, str] = {}

        for part in line.split()[1:]:
            if "=" not in part:
                continue

            key, value = part.split("=", 1)
            values[key] = value

        return values

    def update_metrics(self, values: dict[str, str]) -> None:
        mapping = {
            "STATE": self.state_var,
            "CMD_POS": self.command_position_var,
            "TARGET": self.target_position_var,
            "ENC_POS": self.encoder_position_var,
            "EXPECTED_ENC": self.expected_encoder_var,
            "ERROR": self.position_error_var,
            "DISTANCE": self.remaining_pulses_var,
            "CMD_REV": self.command_revolutions_var,
            "ENC_REV": self.encoder_revolutions_var,
            "CMD_RPM": self.command_rpm_var,
            "ENC_RPM": self.encoder_rpm_var,
            "INVALID": self.invalid_transitions_var,
            "TOLERANCE": self.current_tolerance_var,
        }

        for key, variable in mapping.items():
            if key in values:
                variable.set(values[key])

        if "ENABLED" in values:
            self.driver_enabled_var.set(
                "Yes" if values["ENABLED"] == "1" else "No"
            )

        self.update_state_display()
        self.update_error_display()
        self.update_encoder_display()

    def update_configuration(
        self,
        values: dict[str, str],
    ) -> None:
        mapping = {
            "MOTOR_PPR": self.motor_ppr_var,
            "ENCODER_CPR": self.encoder_cpr_var,
            "ENCODER_SIGN": self.encoder_sign_var,
            "ACCEL": self.acceleration_var,
            "MAX_RPM": self.maximum_rpm_var,
            "TOLERANCE": self.tolerance_var,
            "TELEMETRY_MS": self.stream_interval_var,
        }

        for key, variable in mapping.items():
            if key in values:
                variable.set(values[key])

    # =========================================================
    # Visual state indication
    # =========================================================

    def update_state_display(self) -> None:
        state = self.state_var.get().upper()

        if state == "RUNNING":
            self.state_label.configure(fg="green")

        elif state == "STOPPING":
            self.state_label.configure(fg="orange")

        elif state in ("FAULT", "ESTOPPED"):
            self.state_label.configure(fg="red")

        elif state == "IDLE":
            self.state_label.configure(fg="blue")

        else:
            self.state_label.configure(fg="black")

    def update_error_display(self) -> None:
        try:
            error = abs(
                int(float(self.position_error_var.get()))
            )
            tolerance = int(
                float(self.current_tolerance_var.get())
            )
        except ValueError:
            self.error_status_var.set(
                "Position error: invalid data"
            )
            self.error_status_label.configure(
                fg="red"
            )
            return

        if error > tolerance:
            self.error_status_var.set(
                f"Position error: FAULT "
                f"({error} > {tolerance})"
            )
            self.error_status_label.configure(
                fg="red"
            )

        elif error > tolerance * 0.5:
            self.error_status_var.set(
                f"Position error: WARNING "
                f"({error} counts)"
            )
            self.error_status_label.configure(
                fg="orange"
            )

        else:
            self.error_status_var.set(
                f"Position error: OK "
                f"({error} counts)"
            )
            self.error_status_label.configure(
                fg="green"
            )

    def update_encoder_display(self) -> None:
        try:
            invalid = int(
                float(self.invalid_transitions_var.get())
            )
        except ValueError:
            self.encoder_status_var.set(
                "Encoder signal: invalid data"
            )
            self.encoder_status_label.configure(
                fg="red"
            )
            return

        if invalid == 0:
            self.encoder_status_var.set(
                "Encoder signal: OK"
            )
            self.encoder_status_label.configure(
                fg="green"
            )
        else:
            self.encoder_status_var.set(
                f"Encoder signal: {invalid} "
                f"invalid transitions"
            )
            self.encoder_status_label.configure(
                fg="orange"
            )

    # =========================================================
    # Movement and configuration commands
    # =========================================================

    def send_move(self) -> None:
        pulses = self.get_positive_integer(
            self.pulses_var,
            "Motor pulses",
        )
        if pulses is None:
            return

        rpm = self.get_positive_float(
            self.speed_var,
            "Speed",
        )
        if rpm is None:
            return

        direction = self.direction_var.get().strip()

        if direction not in ("CW", "CCW"):
            messagebox.showerror(
                "Invalid direction",
                "Direction must be CW or CCW.",
            )
            return

        self.send_command(
            f"MOVE {pulses} {direction} {rpm:g}"
        )

    def apply_acceleration(self) -> None:
        value = self.get_positive_float(
            self.acceleration_var,
            "Acceleration",
        )

        if value is not None:
            self.send_command(
                f"SET_ACCEL {value:g}"
            )

    def apply_motor_ppr(self) -> None:
        value = self.get_positive_integer(
            self.motor_ppr_var,
            "Motor pulses per revolution",
        )

        if value is not None:
            self.send_command(
                f"SET_MOTOR_PPR {value}"
            )

    def apply_encoder_cpr(self) -> None:
        value = self.get_positive_integer(
            self.encoder_cpr_var,
            "Encoder counts per revolution",
        )

        if value is not None:
            self.send_command(
                f"SET_ENCODER_CPR {value}"
            )

    def apply_tolerance(self) -> None:
        value = self.get_non_negative_integer(
            self.tolerance_var,
            "Error tolerance",
        )

        if value is not None:
            self.send_command(
                f"SET_TOLERANCE {value}"
            )

    def apply_encoder_sign(self) -> None:
        try:
            value = int(self.encoder_sign_var.get())
        except ValueError:
            messagebox.showerror(
                "Invalid encoder sign",
                "Encoder sign must be 1 or -1.",
            )
            return

        if value not in (-1, 1):
            messagebox.showerror(
                "Invalid encoder sign",
                "Encoder sign must be 1 or -1.",
            )
            return

        self.send_command(
            f"SET_ENCODER_SIGN {value}"
        )

    def apply_stream_interval(self) -> None:
        value = self.get_positive_integer(
            self.stream_interval_var,
            "Telemetry interval",
        )

        if value is None:
            return

        if value < 20 or value > 5000:
            messagebox.showerror(
                "Invalid telemetry interval",
                "Telemetry interval must be between "
                "20 and 5000 ms.",
            )
            return

        self.send_command(
            f"SET_STREAM_MS {value}"
        )

    def apply_all_settings(self) -> None:
        if not self.is_connected():
            messagebox.showerror(
                "Not connected",
                "Connect to the Arduino first.",
            )
            return

        acceleration = self.get_positive_float(
            self.acceleration_var,
            "Acceleration",
        )
        motor_ppr = self.get_positive_integer(
            self.motor_ppr_var,
            "Motor pulses per revolution",
        )
        encoder_cpr = self.get_positive_integer(
            self.encoder_cpr_var,
            "Encoder counts per revolution",
        )
        tolerance = self.get_non_negative_integer(
            self.tolerance_var,
            "Error tolerance",
        )
        stream_interval = self.get_positive_integer(
            self.stream_interval_var,
            "Telemetry interval",
        )

        try:
            encoder_sign = int(
                self.encoder_sign_var.get()
            )
        except ValueError:
            encoder_sign = 0

        if any(
            value is None
            for value in (
                acceleration,
                motor_ppr,
                encoder_cpr,
                tolerance,
                stream_interval,
            )
        ):
            return

        if encoder_sign not in (-1, 1):
            messagebox.showerror(
                "Invalid encoder sign",
                "Encoder sign must be 1 or -1.",
            )
            return

        if stream_interval < 20 or stream_interval > 5000:
            messagebox.showerror(
                "Invalid telemetry interval",
                "Telemetry interval must be between "
                "20 and 5000 ms.",
            )
            return

        commands = (
            f"SET_ACCEL {acceleration:g}",
            f"SET_MOTOR_PPR {motor_ppr}",
            f"SET_ENCODER_CPR {encoder_cpr}",
            f"SET_TOLERANCE {tolerance}",
            f"SET_ENCODER_SIGN {encoder_sign}",
            f"SET_STREAM_MS {stream_interval}",
            "CONFIG",
        )

        for command in commands:
            self.send_command(command)

    def send_command(self, command: str) -> bool:
        if not self.is_connected():
            messagebox.showerror(
                "Not connected",
                "Connect to the Arduino first.",
            )
            return False

        serial_connection = self.ser

        if serial_connection is None:
            return False

        try:
            with self.write_lock:
                serial_connection.write(
                    f"{command}\n".encode("ascii")
                )
                serial_connection.flush()

            self.log(f"> {command}")
            return True

        except serial.SerialException as error:
            messagebox.showerror(
                "Serial error",
                str(error),
            )
            return False

    # =========================================================
    # Input validation
    # =========================================================

    @staticmethod
    def get_positive_integer(
        variable: tk.StringVar,
        field_name: str,
    ) -> Optional[int]:
        try:
            value = int(variable.get())
        except ValueError:
            messagebox.showerror(
                "Invalid value",
                f"{field_name} must be an integer.",
            )
            return None

        if value <= 0:
            messagebox.showerror(
                "Invalid value",
                f"{field_name} must be greater than zero.",
            )
            return None

        return value

    @staticmethod
    def get_non_negative_integer(
        variable: tk.StringVar,
        field_name: str,
    ) -> Optional[int]:
        try:
            value = int(variable.get())
        except ValueError:
            messagebox.showerror(
                "Invalid value",
                f"{field_name} must be an integer.",
            )
            return None

        if value < 0:
            messagebox.showerror(
                "Invalid value",
                f"{field_name} cannot be negative.",
            )
            return None

        return value

    @staticmethod
    def get_positive_float(
        variable: tk.StringVar,
        field_name: str,
    ) -> Optional[float]:
        try:
            value = float(variable.get())
        except ValueError:
            messagebox.showerror(
                "Invalid value",
                f"{field_name} must be a number.",
            )
            return None

        if value <= 0:
            messagebox.showerror(
                "Invalid value",
                f"{field_name} must be greater than zero.",
            )
            return None

        return value

    # =========================================================
    # General helpers
    # =========================================================

    def is_connected(self) -> bool:
        return bool(
            self.ser is not None and
            self.ser.is_open
        )

    def reset_display_metrics(self) -> None:
        self.command_position_var.set("0")
        self.target_position_var.set("0")
        self.encoder_position_var.set("0")
        self.expected_encoder_var.set("0")
        self.position_error_var.set("0")
        self.remaining_pulses_var.set("0")

        self.command_revolutions_var.set("0.0000")
        self.encoder_revolutions_var.set("0.0000")

        self.command_rpm_var.set("0.00")
        self.encoder_rpm_var.set("0.00")

        self.invalid_transitions_var.set("0")
        self.current_tolerance_var.set(
            self.tolerance_var.get()
        )

        self.error_status_var.set(
            "Position error: OK (0 counts)"
        )
        self.encoder_status_var.set(
            "Encoder signal: OK"
        )

        self.update_error_display()
        self.update_encoder_display()

    def log(self, text: str) -> None:
        timestamp = time.strftime("%H:%M:%S")

        self.output.insert(
            tk.END,
            f"[{timestamp}] {text}\n",
        )
        self.output.see(tk.END)

    def clear_log(self) -> None:
        self.output.delete(
            "1.0",
            tk.END,
        )

    def on_close(self) -> None:
        self.disconnect()
        self.root.destroy()


def main() -> None:
    root = tk.Tk()
    StepperGui(root)
    root.mainloop()


if __name__ == "__main__":
    main()