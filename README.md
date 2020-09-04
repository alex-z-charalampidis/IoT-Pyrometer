# IoT-Pyrometer
Hardware,Firmware and Cloud repository for an IoT pyrometer - a device that measures temperatures using infrared radiation

# Hardware 
The device uses MLX90640, a 32x24 pixel thermal camera to measure the temperature of a target along with VL53L0X ToF sensor to measure distance from the target surface.
The first PCB of the device is the main board which holds ESP32, a power mux for battery back-up operation, circuitry for battery level measurement along with FTDI232 for USB communications.

![pyrometer](https://github.com/alex-z-charalampidis/IoT-Pyrometer/blob/master/FINAL_PCB.PNG?raw=true)

The second PCB is the sense board which holds the thermal camera & the distance sensor

![sense_board](https://github.com/alex-z-charalampidis/IoT-Pyrometer/blob/master/MLX_BOARD.PNG)

# Software
Software is better documented in its own README at Firmware/source_file

# Cloud
The cloud is a linux server running mosquitto (MQTT broker) at the back-end and a website hosted with NGINX at the front-end 

![dashboard](https://github.com/alex-z-charalampidis/IoT-Pyrometer/blob/master/Dashboard.PNG?raw=true)

