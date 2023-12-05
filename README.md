# The Home office automation project

Here's that plan of this project <br>
<p align="center">
    <img 
        src="images/diagram_svg.svg" alt="image"
        height="600"
    >
</p>

## Overview
In this project there consists of 5 parts that are works together.

1. Node-RED <br>
Node-RED is a Open Source Low-Code platform written in Javascript which is run as a backend for the whole project. It's also connects to the database like `influxdb` to store and get query the data of the rfid module and then Node-RED is also connected to Zigbee2MQTT service through MQTT to expand the possibility of available devices.
1. ESP32 Relay board <br>
This board is connected to the same WiFi network as the Node-RED and communicate with Node-RED through web-socket protocol. This board is powered by normal 5V power supply and be able to control 6 relays through web-socket command
1. Zigbee Devices <br>
These device connect to Node-RED by a dongle which forward the message to the MQTT broker through Zigbee2MQTT. Zigbee2MQTT also has it's own web interface so that we can config.
1. Motion Detector <br>
This device operates by sensing the motion sensor and and sends it to the [MQTT](#more-about-mqtt) broker

## More about MQTT
<p>
MQTT in simple term is like a chat server for IOT devices. There are 2 main types of operations that MQTT provides. Each are based on something called "Topic" which is like a chat room we're in when someone send some message to the topic everyone recieves and vise-versa.
</p>
<p>
The two operations mentioned earlier are called "Publish" and "Subscribe", the device which wants to send the message will "Publish" the data to the "Topic" and then the devices that are "Subscribe" to that topic will recieves it like shown in below diagram.
</p>

<p align="center">
    <img src="images/mqtt-publish-subscribe.svg"alt="image" height="500"><br>
</p>