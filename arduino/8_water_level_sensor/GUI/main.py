import tkinter as tk
from tkinter import ttk, messagebox

import serial
import serial.tools.list_ports


BAUD_RATE = 9600 

# Calibration points:
# sensor value -> water level percentage
CALIBRATION_POINTS = [
    (0.0, 0.0),
    (480.0, 25.0),
    (555.0, 50.0),
    (565.0, 80.0),
    (580.0, 100.0),
]


class WaterLevelMonitor:
    def __init__(self, root):
        self.root = root
        self.root.title("Arduino Water Level Monitor")
        self.root.geometry("460x680")
        self.root.resizable(False, False)

        self.serial_port = None
        self.serial_buffer = ""

        self.create_widgets()
        self.refresh_ports()

        self.root.after(50, self.read_serial_data)
        self.root.protocol("WM_DELETE_WINDOW", self.close_program)

    def create_widgets(self):
        self.create_connection_section()
        self.create_water_level_section()

    def create_connection_section(self):
        connection_frame = ttk.LabelFrame(
            self.root,
            text="Serial connection",
            padding=12
        )
        connection_frame.pack(
            fill="x",
            padx=15,
            pady=(15, 8)
        )

        ttk.Label(
            connection_frame,
            text="Port:"
        ).grid(
            row=0,
            column=0,
            padx=5,
            pady=5
        )

        self.port_combobox = ttk.Combobox(
            connection_frame,
            width=22,
            state="readonly"
        )
        self.port_combobox.grid(
            row=0,
            column=1,
            padx=5,
            pady=5
        )

        self.refresh_button = ttk.Button(
            connection_frame,
            text="Refresh",
            command=self.refresh_ports
        )
        self.refresh_button.grid(
            row=0,
            column=2,
            padx=5,
            pady=5
        )

        self.connect_button = ttk.Button(
            connection_frame,
            text="Connect",
            command=self.toggle_connection
        )
        self.connect_button.grid(
            row=1,
            column=0,
            columnspan=3,
            sticky="ew",
            padx=5,
            pady=8
        )

        self.connection_status_label = ttk.Label(
            connection_frame,
            text="Disconnected"
        )
        self.connection_status_label.grid(
            row=2,
            column=0,
            columnspan=3,
            pady=5
        )

    def create_water_level_section(self):
        display_frame = ttk.LabelFrame(
            self.root,
            text="Water level",
            padding=12
        )
        display_frame.pack(
            fill="both",
            expand=True,
            padx=15,
            pady=(8, 15)
        )

        self.canvas_width = 300
        self.canvas_height = 430

        self.tank_left = 75
        self.tank_top = 30
        self.tank_right = 185
        self.tank_bottom = 390

        self.canvas = tk.Canvas(
            display_frame,
            width=self.canvas_width,
            height=self.canvas_height,
            bg="white",
            highlightthickness=0
        )
        self.canvas.pack(pady=5)

        self.canvas.create_rectangle(
            self.tank_left,
            self.tank_top,
            self.tank_right,
            self.tank_bottom,
            outline="black",
            width=4
        )

        self.water_rectangle = self.canvas.create_rectangle(
            self.tank_left + 4,
            self.tank_bottom - 4,
            self.tank_right - 4,
            self.tank_bottom - 4,
            fill="blue",
            outline=""
        )

        for percentage in range(0, 101, 10):
            y = self.percentage_to_y(percentage)

            self.canvas.create_line(
                self.tank_right + 6,
                y,
                self.tank_right + 18,
                y,
                fill="black",
                width=1
            )

            self.canvas.create_text(
                self.tank_right + 26,
                y,
                text=f"{percentage}%",
                anchor="w",
                font=("Arial", 10)
            )

        self.percentage_label = ttk.Label(
            display_frame,
            text="0.0%",
            font=("Arial", 32, "bold")
        )
        self.percentage_label.pack(pady=(5, 2))

        self.raw_value_label = ttk.Label(
            display_frame,
            text="Raw sensor value: 0",
            font=("Arial", 12)
        )
        self.raw_value_label.pack(pady=2)

        self.calibration_label = ttk.Label(
            display_frame,
            text="Calibration: 0->0%, 480->25%, 555->50%, 565->80%, 580->100%",
            font=("Arial", 9)
        )
        self.calibration_label.pack(pady=(4, 2))

    def percentage_to_y(self, percentage):
        inner_top = self.tank_top + 4
        inner_bottom = self.tank_bottom - 4
        usable_height = inner_bottom - inner_top

        return inner_bottom - (
            usable_height * percentage / 100.0
        )

    def sensor_value_to_percentage(self, raw_value):
        if raw_value <= CALIBRATION_POINTS[0][0]:
            return CALIBRATION_POINTS[0][1]

        if raw_value >= CALIBRATION_POINTS[-1][0]:
            return CALIBRATION_POINTS[-1][1]

        for index in range(len(CALIBRATION_POINTS) - 1):
            value_1, percent_1 = CALIBRATION_POINTS[index]
            value_2, percent_2 = CALIBRATION_POINTS[index + 1]

            if value_1 <= raw_value <= value_2:
                return percent_1 + (
                    (raw_value - value_1)
                    * (percent_2 - percent_1)
                    / (value_2 - value_1)
                )

        return 0.0

    def refresh_ports(self):
        ports = serial.tools.list_ports.comports()
        port_names = [port.device for port in ports]

        self.port_combobox["values"] = port_names

        if port_names:
            self.port_combobox.current(0)
        else:
            self.port_combobox.set("")

    def toggle_connection(self):
        if self.serial_port is None:
            self.connect_to_serial()
        else:
            self.disconnect_serial()

    def connect_to_serial(self):
        selected_port = self.port_combobox.get()

        if not selected_port:
            messagebox.showwarning(
                "No port selected",
                "Select an Arduino serial port."
            )
            return

        try:
            self.serial_port = serial.Serial(
                port=selected_port,
                baudrate=BAUD_RATE,
                timeout=0
            )

            self.serial_buffer = ""

            self.connection_status_label.config(
                text=f"Connected to {selected_port}"
            )

            self.connect_button.config(text="Disconnect")
            self.port_combobox.config(state="disabled")
            self.refresh_button.config(state="disabled")

        except serial.SerialException as error:
            self.serial_port = None

            messagebox.showerror(
                "Connection error",
                str(error)
            )

    def disconnect_serial(self):
        if self.serial_port is not None:
            try:
                self.serial_port.close()
            except serial.SerialException:
                pass

        self.serial_port = None
        self.serial_buffer = ""

        self.connection_status_label.config(
            text="Disconnected"
        )

        self.connect_button.config(text="Connect")
        self.port_combobox.config(state="readonly")
        self.refresh_button.config(state="normal")

    def read_serial_data(self):
        if self.serial_port is not None:
            try:
                bytes_waiting = self.serial_port.in_waiting

                if bytes_waiting > 0:
                    received_data = self.serial_port.read(
                        bytes_waiting
                    ).decode(
                        "utf-8",
                        errors="ignore"
                    )

                    self.serial_buffer += received_data

                    while "\n" in self.serial_buffer:
                        line, self.serial_buffer = (
                            self.serial_buffer.split("\n", 1)
                        )

                        self.process_serial_line(line.strip())

            except serial.SerialException:
                self.disconnect_serial()

                messagebox.showerror(
                    "Serial error",
                    "The serial connection was lost."
                )

        self.root.after(50, self.read_serial_data)

    def process_serial_line(self, line):
        if not line:
            return

        try:
            raw_value = float(line)
        except ValueError:
            return

        percentage = self.sensor_value_to_percentage(raw_value)

        percentage = max(
            0.0,
            min(percentage, 100.0)
        )

        self.update_display(
            raw_value,
            percentage
        )

    def update_display(self, raw_value, percentage):
        water_top = self.percentage_to_y(percentage)

        self.canvas.coords(
            self.water_rectangle,
            self.tank_left + 4,
            water_top,
            self.tank_right - 4,
            self.tank_bottom - 4
        )

        self.percentage_label.config(
            text=f"{percentage:.1f}%"
        )

        self.raw_value_label.config(
            text=f"Raw sensor value: {raw_value:.0f}"
        )

    def close_program(self):
        self.disconnect_serial()
        self.root.destroy()


def main():
    root = tk.Tk()
    WaterLevelMonitor(root)
    root.mainloop()


if __name__ == "__main__":
    main()
