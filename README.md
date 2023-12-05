# The Home office automation project

Here's that plan of this project <br>
<img src="images/diagram_svg.svg" alt="image" height="400"><br>

In this project there consists of 4 parts that are works together.

1. Node-RED <br>
Node-RED is a Open Source Low-Code platform written in Javascript which is run as a backend for the whole project. It's also connects to the database like `influxdb` to store and get query the data of the rfid module and then Node-RED is also connected to Zigbee2MQTT service through MQTT to expand the possibility of available devices.
1. ESP32 Relay board <br>
This board is connected to the same WiFi network as the Node-RED and communicate with Node-RED through web-socket protocol. This board is powered by normal 5V power supply and be able to control 6 relays through web-socket command
1. Zigbee Devices <br>
These device connect to Node-RED by a dongle which forward the message to the MQTT broker through Zigbee2MQTT. Zigbee2MQTT also has it's own web interface so that we can config. More about Zigbee2MQTT can be found [here](docs/mqtt.md)

