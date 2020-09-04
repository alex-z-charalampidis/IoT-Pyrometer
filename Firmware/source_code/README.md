# IoT pyrometric device firmware

## Installation
#### You need to have esp-idf downloaded

## MQTT
#### The device listens for rpc calls via MQTT at >ir-iot/rpc 
#### It replies for: battery at > ir-iot/battery
#### 		     reboot > ir-iot/reboot
####                 power out reason > ir-iot/powerout
####                 mode > ir-iot/mode
####                 rssi > ir-iot/rssi

## MLX90640
#### MLX90640 is now back at 1fps 