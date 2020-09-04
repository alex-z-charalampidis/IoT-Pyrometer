#!/bin/bash
clear
option_selected=-1
gnome_existing=0
mode=0
reboot_device () {
 mosquitto_pub -h alexios.tech -u esp -P hitman112 -t ir-iot/rpc -m '{"src":"ir-iot","id":1,"method":"reboot","args": {"timer": 0}}'
}

mosquitto_window () {
 if [ "$gnome_existing" -eq "0" ]
 then
 gnome-terminal --command="mosquitto_sub -h alexios.tech -u esp -P hitman112 -t 'ir-iot/rpc' "
 gnome_existing=1
 fi
}

reset_reason () {
 mosquitto_pub -h alexios.tech -u esp -P hitman112 -t ir-iot/rpc -m '{"src":"ir-iot","id":1,"method":"reset_reason","args": {"a": 0}}'
}

battery_v () {
 mosquitto_pub -h alexios.tech -u esp -P hitman112 -t ir-iot/rpc -m '{"src":"ir-iot","id":1,"method":"battery_voltage","args": {"a": 0}}'
}

rssi () {
 mosquitto_pub -h alexios.tech -u esp -P hitman112 -t ir-iot/rpc -m '{"src":"ir-iot","id":1,"method":"rssi","args": {"a": 0}}'
}

other () {
 mosquitto_pub -h alexios.tech -u esp -P hitman112 -t ir-iot/rpc -m '{"src":"ir-iot","id":1,"method":"av","args": {"avg_filter": 12, "loop_timer":3000, "select_filter": 2}}'
}


distance () {
 mosquitto_pub -h alexios.tech -u esp -P hitman112 -t ir-iot/rpc -m '{"src":"ir-iot","id":1,"method":"distance_set","args": {"min": 10, "max": 130}}'
}

mlx () {
 mosquitto_pub -h alexios.tech -u esp -P hitman112 -t ir-iot/rpc -m '{"src":"ir-iot","id":1,"method":"avg","args": {"a": 7}}'
}

change_mode() {
 if [ "$1" -eq "1" ]
 then
 mosquitto_pub -h alexios.tech -u esp -P hitman112 -t ir-iot/rpc -m '{"src":"ir-iot","id":1,"method":"change_mode","args": {"mode": 1}}'
 elif [ "$1" -eq "0" ]
 then
 mosquitto_pub -h alexios.tech -u esp -P hitman112 -t ir-iot/rpc -m '{"src":"ir-iot","id":1,"method":"change_mode","args": {"mode": 0}}'
 else
 printf "\nError.Invalid mode selected\n"
 fi
}



echo "*******************************"
echo "*   DEVICE COMMAND & CONTROL  *"
echo "*   CREATED BY Z.A.C	      *"
echo "*******************************"
echo "-------------------------------"

printf "|Select an option             |"
printf "\n|0: Exit                      |"
printf "\n|1: Reboot Device             |"
printf "\n|2: Reset Reason	      |"
printf "\n|3: Battery Voltage	      |"
printf "\n|4: Change mode	              |\n"
printf "\n|5: Rssi                     |\n"
printf "\n|6: Distance                     |\n"
printf "\n|7: Mlx average		|\n"
echo "-------------------------------"

while [ "$option_selected" -ne "0" ]
do 
printf "\nChoose option:"
read option_selected
if [ "$option_selected" -eq "1" ] 
then
 printf "\nRebooting Device\n"
 mosquitto_window
 reboot_device
elif [ "$option_selected" -eq "2" ]
then
 printf "\nAsking Reset reason...\n"
 mosquitto_window
 reset_reason
elif [ "$option_selected" -eq "3" ]
then
 printf "\nAsking Battery Voltage...\n"
 mosquitto_window
 battery_v
elif [ "$option_selected" -eq "4" ]
then
 printf "\nMode selection [ 0 = manual, 1=auto]:"
 read mode
 mosquitto_window
 change_mode "$mode"
elif [ "$option_selected" -eq "5" ]
then
 printf "\nAsking rssi:"
 mosquitto_window
 rssi
elif [ "$option_selected" -eq "6" ]
then
 printf "\nSetting distance"
 mosquitto_window
 distance
elif [ "$option_selected" -eq "7" ]
then
 printf "\nSetting distance"
 mosquitto_window
 mlx
elif [ "$option_selected" -eq "8" ]
then
 printf "\nSetting distance"
 mosquitto_window
 other
elif [ "$option_selected" -eq "0" ]
then
 clear
 printf "Exiting Programme...\n"
 exit
else 
 printf "\nOption not recognized\n"
fi

done


