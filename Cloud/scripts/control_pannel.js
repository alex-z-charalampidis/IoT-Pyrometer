function control_panel_connection() {
    var MQTTbroker = 'alexios.tech';
    var MQTTport = 8080;
    var MQTTsubTopic = 'ir-iot/#';
    //settings END

    //mqtt broker 
    //generate random client name so that every browser session has one client
    var clientName = Math.random().toString(20).substring(5);
    var client = new Paho.MQTT.Client(MQTTbroker, MQTTport, clientName);
    client.onMessageArrived = onMessageArrived;
    client.onConnectionLost = onConnectionLost;
    //connect to broker is near the bottom

    //mqtt connecton options including the mqtt broker subscriptions
    var options = {
        timeout: 3,
        useSSL: true,
        onSuccess: function() {
            console.log("front end client listening");
            // Connection succeeded; subscribe to our topics
            client.subscribe(MQTTsubTopic, {
                qos: 1
            });
        },
        onFailure: function(message) {
            console.log("Connection failed, ERROR: " + message.errorMessage);
            //window.setTimeout(location.reload(),20000); //wait 20seconds before trying to connect again.
        },
        keepAliveInterval: 20
    };
    //can be used to reconnect on connection lost
    function onConnectionLost(responseObject) {
        console.log("connection lost: " + responseObject.errorMessage);
        //window.setTimeout(location.reload(),20000); //wait 20seconds before trying to connect again.
    };

    //what is done when a message arrives from the broker
    function onMessageArrived(message) {
        if (message.destinationName == "ir-iot/battery") {
            change_battery(message.payloadString);
        } else if (message.destinationName == "ir-iot/reboot") {
            reboot_cb(message.payloadString);
        } else if (message.destinationName == "ir-iot/powerout") {
            powerout_cb(message.payloadString);
        } else if (message.destinationName == "ir-iot/mode") {
            mode_cb(message.payloadString);
        } else if (message.destinationName == "ir-iot/rssi") {
            rssi_update(message.payloadString);
        } else if (message.destinationName == 'temp') {
            //Resets "success" colors of buttons to normal colors
            reset_button_color();
        } else if (message.destinationName == "ir-iot/distance") {
            distance_update(message.payloadString);
        }
        /*else {
        			console.log("Received message from unrecognised topic");
        		}*/
    };
    //Connect client to all the above topics 
    client.connect(options);

    //Initialize battery gauge canvas 
    var opts = {
        angle: 0.09, // The span of the gauge arc
        lineWidth: 0.17, // The line thickness
        radiusScale: 0.7, // Relative radius
        pointer: {
            length: 0.45, // // Relative to gauge radius
            strokeWidth: 0.06, // The thickness
            color: '#000000' // Fill color
        },
        limitMax: false, // If false, max value increases automatically if value > maxValue
        limitMin: false, // If true, the min value of the gauge will be fixed
        colorStart: '#0EA03A', // Colors
        colorStop: '#DD6C21', // just experiment with them
        strokeColor: '#E0E0E0', // to see which ones work best for you
        generateGradient: true,
        highDpiSupport: true, // High resolution support

    };
    var target = document.getElementById('battery'); // your canvas element
    var gauge = new Gauge(target).setOptions(opts); // create sexy gauge!
    gauge.maxValue = 4.2; // set max gauge value
    gauge.setMinValue(3.2); // Prefer setter over gauge.minValue = 0
    gauge.animationSpeed = 30; // set animation speed (32 is default value)
    gauge.set(3.5); // set actual value
    document.getElementById('battery-gauge-text').innerHTML = "Battery Voltage:" + gauge.value + "V";

    //Battery gauge update function
    function change_battery(msg) {
        gauge.set(msg);
        document.getElementById('battery-gauge-text').innerHTML = "Battery Voltage: " + msg + "V";
    };

    //Reboot callback function
    function reboot_cb(msg) {
        if (msg == 'Rebooting') {
            document.getElementById('reboot_button').className = "btn btn-success";
        } else {
            document.getElementById('reboot_button').className = "btn btn-primary";
        }
    };

    //Power out callback function
    function powerout_cb(msg) {
        console.log('Power out reason:' + msg);
    };

    //Change mode callback function
    function mode_cb(msg) {
        if (msg == 'Device set to Auto mode') {
            document.getElementById('dev_auto_button').className = "btn btn-success";
        } else if (msg == 'Device set to Manual mode') {
            document.getElementById('dev_man_button').className = "btn btn-success";
        } else {
            console.log('Unrecognised msg received on change mode cb:' + msg);
        }
    };

    //Updates rssi text
    function rssi_update(msg) {
        document.getElementById('rssi').innerHTML = "Device RSSI:" + msg;
    };
    function distance_update(msg) {
        document.getElementById('distance').innerHTML = "Distance from target:" + msg +"mm";
    }
    //Resets button class thus restoring the color
    function reset_button_color(){
        document.getElementById('reboot_button').className = "btn btn-primary";
        document.getElementById('dev_auto_button').className ="btn btn-primary";
        document.getElementById('dev_man_button').className = "btn btn-primary";
    };


}

/* Function: >Connects a client to the mqtt server with a random ID
 * > Sends an rpc command
 * > Disconnects client 
 */
var MQTTbroker = 'alexios.tech';
var MQTTport = 8080;
var MQTTpubTopic = 'ir-iot/rpc';
var clientName = Math.random().toString(20).substring(5);
var client = new Paho.MQTT.Client(MQTTbroker, MQTTport, clientName);
client.onMessageArrived = onMessageArrived;
client.onConnectionLost = onConnectionLost;
var options = {
    timeout: 3,
    useSSL: true,
    onSuccess: function() {
        // Connection succeeded; subscribe to our topics
        console.log('control panel client connected');
    },
    onFailure: function(message) {
        console.log("Connection failed, ERROR: " + message.errorMessage);
        //window.setTimeout(location.reload(),20000); //wait 20seconds before trying to connect again.
    },
    keepAliveInterval: 20
};
client.connect(options);
//can be used to reconnect on connection lost
function onConnectionLost(responseObject) {
    if (responseObject.errorMessage == "AMQJSC0000I OK.") {
        //Do nothing or potentially handle it differently in the future
    } else {
        console.log("connection lost: " + responseObject.errorMessage);
    }

    //window.setTimeout(location.reload(),20000); //wait 20seconds before trying to connect again.
};

//what is done when a message arrives from the broker
function onMessageArrived(message) {
    console.log(message.destinationName, '', message.payloadString);
};

function rpc_function(rpc_func) {
    var payload = null;
    if (rpc_func == "1") {
        payload = '{"src":"ir-iot","id":1,"method":"change_mode","args": {"mode": 1}}';
    } else if (rpc_func == "2") {
        payload = '{"src":"ir-iot","id":1,"method":"change_mode","args": {"mode": 0}}';
    } else if (rpc_func == "3") {
        payload = '{"src":"ir-iot","id":1,"method":"reboot","args": {"timer": 0}}';
    } else if (rpc_func == "4") {
        payload = '{"src":"ir-iot","id":1,"method":"battery_voltage","args": {"a": 0}}'
    } else if (rpc_func == "5") {
        payload = '{"src":"ir-iot","id":1,"method":"rssi","args": {"a": 0}}'
    } else if (rpc_func == "6") {
        payload = '{"src":"ir-iot","id":1,"method":"distance","args": {"a": 0}}'
    }
    var message = new Paho.MQTT.Message(payload);
    message.destinationName = MQTTpubTopic;
    message.qos = 1;
    client.send(message);
    console.log('RPC message sent');
}
