### Bluetooth low energy example code for interfacing with buttplug

Inspired by Apache NimBLE kbdhid role example here
  https://github.com/olegos76/nimble_kbdhid_example

This example creates GATT server and then starts advertising, waiting to be connected
to a GATT client. It supports different actuators and sensors as configured in main.c

You will also need a patched buttplug library to recognize the device. See:

It uses ESP32's Bluetooth controller and NimBLE stack based BLE host.

Tested with ESP-IDF v5.2.1

How to use example (with ESP-IDF v5.2.1):

First, install ESP-IDF and tools according to ESP-IDF documentation.
See https://docs.espressif.com/projects/esp-idf/en/v5.2.1/esp32/get-started/index.html

### Build the project

```
idf.py build
```
### Flash and monitor the project

```
idf.py flash && idf.py monitor
```
