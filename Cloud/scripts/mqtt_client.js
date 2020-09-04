/*
Original dashboard by @bordignon on twitter
Edited and mutated by @alexios.tech  
*/

        //settings BEGIN
        function topic_listener(){
            var MQTTbroker = 'alexios.tech';
            var MQTTport = 8080;
            var MQTTsubTopic = document.getElementById('topic-input').value || 'temp'; //works with wildcard # and + topics dynamically now
            //settings END

            var chart; // global variable for chart
            var dataTopics = new Array();

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
                    console.log("time series chart connected");
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

                //check if it is a new topic, if not add it to the array
                if (dataTopics.indexOf(message.destinationName) < 0) {

                    dataTopics.push(message.destinationName); //add new topic to array
                    var y = dataTopics.indexOf(message.destinationName); //get the index no

                    //create new data series for the chart
                    var newseries = {
                        id: y,
                        name: message.destinationName,
                        data: []
                    };

                    chart.addSeries(newseries); //add the series

                };

                var y = dataTopics.indexOf(message.destinationName); //get the index no of the topic from the array
                var myEpoch = new Date().getTime(); //get current epoch time
                var thenum = message.payloadString.replace(/^\D+/g, ''); //remove any text spaces from the message
                var plotMqtt = [myEpoch, Number(thenum)]; //create the array
                if (isNumber(thenum)) { //check if it is a real number and not text
                    console.log('is a propper number, will send to chart.')
                    plot(plotMqtt, y); //send it to the plot function
                };
            };

            //check if a real number	
            function isNumber(n) {
                return !isNaN(parseFloat(n)) && isFinite(n);
            };

            //function that is called once the document has loaded
            function init() {

                //i find i have to set this to false if i have trouble with timezones.
                Highcharts.setOptions({
                    global: {
                        useUTC: false
                    }
                });

                // Connect to MQTT broker
                client.connect(options);

            };


            //this adds the plots to the chart	
            function plot(point, chartno) {
                console.log(point);

                var series = chart.series[0],
                    shift = series.data.length > 20; // shift if the series is 
                // longer than 20
                // add the point
                chart.series[chartno].addPoint(point, true, shift);

            };

            //settings for the chart
            $(document).ready(function() {
                chart = new Highcharts.Chart({
                    chart: {
                        renderTo: 'container',
                        defaultSeriesType: 'spline'
                    },
                    title: {
                        text: 'Live websocket MQTT stream on topic:' + MQTTsubTopic
                    },
                    subtitle: {
                        text: 'broker: ' + MQTTbroker + ' | port: ' + MQTTport + ' | topic : ' + MQTTsubTopic
                    },
                    xAxis: {
                        type: 'datetime',
                        tickPixelInterval: 150,
                        maxZoom: 20 * 1000
                    },
                    yAxis: {
                        minPadding: 0.2,
                        maxPadding: 0.2,
                        title: {
                            text: 'Value',
                            margin: 80
                        }
                    },
                    series: []
                });
            });
            init();
        }