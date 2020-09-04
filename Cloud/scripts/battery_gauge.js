function battery_gauge() {
    var MQTTbroker = 'alexios.tech';
    var MQTTport = 8080;
    var MQTTsubTopic = 'ir-iot/rpc';
    //settings END

    //mqtt broker 
    //generate random client Name so that many clients can see the data
    var clientName = Math.random().toString(20).substring(5);
    var client = new Paho.MQTT.Client(MQTTbroker, MQTTport, clientName);
    client.onMessageArrived = onMessageArrived;
    client.onConnectionLost = onConnectionLost;
    //connect to broker is at the bottom of the init() function !!!!

    //userName:js and password:jserver can be used when server requires authentication
    //mqtt connecton options including the mqtt broker subscriptions
    var options = {
        timeout: 3,
        useSSL: true,
        onSuccess: function() {
            console.log("battery gauge connected");
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
        console.log(message.destinationName, '', message.payloadString);
    };
	
    client.connect(options);
	
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
        colorStart: '#6FADCF', // Colors
        colorStop: '#8FC0DA', // just experiment with them
        strokeColor: '#E0E0E0', // to see which ones work best for you
        generateGradient: true,
        highDpiSupport: true, // High resolution support

    };
    var target = document.getElementById('battery'); // your canvas element
    var gauge = new Gauge(target).setOptions(opts); // create sexy gauge!
    gauge.maxValue = 4.2; // set max gauge value
    gauge.setMinValue(0); // Prefer setter over gauge.minValue = 0
    gauge.animationSpeed = 48; // set animation speed (32 is default value)
    gauge.set(3.3); // set actual value
}