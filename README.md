# Vine Robots for Bed Transfer
Supporting code for pneumatic robotics system for patient bed transfer

## Pressure Regulator Control
Controls QB4 pressure regulator via 0-5V analog signal generated via PWM. Users input desired pressure via serial interface

## TCW Peripheral
Embedded electronics system in tip-clutching winches. Set up as a Bluetooth peripheral device and with a unique name, the system can receive commands via BLE to open and close winch

## TCW Central
Portable Bluetooth-enabled device that connects to all the tip-clutching winches and sends commands via SPDT switches.

## Frame Control 
Also set up as a Bluetooth peripheral, this system receives commands and operates 8 distinct brushless motors via PWM interface. Custom data structures and functions to create new commands, control individual and groups of motors, adjust speed/dir, and run different sequences of motor control

