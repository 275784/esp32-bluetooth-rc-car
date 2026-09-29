# ESP32 Bluetooth RC Car

![RC Car](images/rc-car.jpg)

Wireless RC vehicle controlled by an Xbox controller over Bluetooth.

The vehicle uses an ESP32-DevKitV1 as the main controller. The ESP32 receives commands from the Xbox controller, processes the input and generates PWM signals for the ESC and steering servo.

The system also includes a custom PCB, power distribution, battery status indication and software safety mechanisms.

## Features

- Xbox controller input over Bluetooth
- ESP32-based control
- Brushless motor control through ESC
- Servo-based steering
- Progressive acceleration (Soft Start)
- Safe direction switching through neutral
- Trigger and steering dead zones
- Run / Stop mode
- Normal and Sport driving modes
- Battery voltage indication
- Custom PCB
- Fail-safe behavior after Bluetooth disconnection

## System Architecture

```text
Xbox Controller
      │
   Bluetooth
      │
      ▼
    ESP32
   │     │
   │     └──────► Steering Servo
   │
   └────────────► ESC ─────► Brushless Motor


LiPo Battery
      │
      ├──────────► Power Distribution
      ├──────────► Voltage Indicator
      │
      └──────────► XL4015 Step-Down
                         │
                         ├────► ESP32
                         └────► Servo
```

## How It Works

1. The Xbox controller connects to the ESP32 using Bluetooth.
2. The ESP32 reads trigger, button and joystick inputs.
3. Throttle input is converted into a PWM signal for the ESC.
4. The steering axis is mapped to the steering servo position.
5. The battery status is monitored by the analog voltage indicator.
6. The firmware continuously updates motor and steering outputs.
7. If the controller connection is lost, the system immediately returns to a safe state.

## Hardware

- ESP32-DevKitV1
- Brushless motor
- Electronic Speed Controller (ESC)
- Steering servo
- LiPo battery
- XL4015 step-down converter
- Custom PCB
- Battery voltage indicator

## Software

- C++
- Arduino IDE
- ESP32
- Bluepad32
- ESP32Servo
- Bluetooth
- PWM

## Motor Control

The ESC operates using a 50 Hz PWM signal.

The firmware uses:

- `1000 µs` — minimum
- `1500 µs` — neutral
- `2000 µs` — maximum

Two driving modes are available:

- **Normal**
- **Sport**

The throttle input is processed using a dead zone and mapped to the corresponding PWM range.

A progressive acceleration mechanism limits how quickly the PWM command changes.

## Steering Control

Steering is controlled using the horizontal axis of the Xbox controller.

A configurable dead zone prevents small joystick movements from causing unwanted steering.

The steering angle is smoothly adjusted toward the target position.

## Safety Features

The firmware includes several mechanisms designed to prevent uncontrolled movement:

- motor output returns to neutral when the controller disconnects
- steering returns to the center position
- switching between forward and reverse requires passing through neutral
- Run / Stop functionality
- trigger dead zone
- steering dead zone
- controlled acceleration

## Hardware Design

The project includes a custom PCB integrating:

- ESP32
- ESC interface
- servo interface
- power distribution
- XL4015 step-down converter
- analog battery voltage indicator

### PCB Design

![PCB 3D Design](images/pcb-3d.png)

![PCB Layout](images/pcb-layout.jpg)

### Analog Battery Voltage Indicator

The battery voltage indicator was designed and analyzed in LTspice.

![Battery Voltage Indicator Schematic](hardware/ltspice/battery-voltage-indicator-schematic.png)

The simulation was used to analyze the voltage thresholds and LED current characteristics of the indicator circuit.

![LED Current Characteristics](hardware/ltspice/led-current-characteristics.png)

## My Contribution

This was a team project.

My contribution included:

- electrical schematic design
- PCB design in KiCad
- PCB assembly and soldering
- hardware integration
- system testing

## Project Status

**Completed and tested.**

## Possible Improvements

- Add obstacle detection
- Add Wi-Fi telemetry
- Add mobile monitoring
- Add live camera streaming
- Add additional sensors
- Add autonomous driving functionality

## Project Files

- `src/` — Arduino firmware
- `hardware/kicad/` — PCB design
- `hardware/ltspice/` — analog circuit simulation
- `images/` — project documentation images

## Authors

- Konrad Misztela
- Miriam Witiw
- Maks Hula
- Mateusz Bochenek