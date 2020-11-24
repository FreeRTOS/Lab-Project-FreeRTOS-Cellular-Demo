# FreeRTOS Labs - Cellular Demo

## Introduction

FreeRTOS has a complete suite of networking stacks designed for IoT applications.  Applications can use communication protocols at different levels - MQTT, HTTP, Secure Sockets or TCP.  Common connectivity technologies such as Ethernet, Wi-Fi and BLE have been supported by FreeRTOS, with a wide selection of chips and modules already integrated for out-of-the box usage.

The demos in this project show how to establish mutually authenticated MQTT connections with AWS IoT Core by using cellular connectivity.  The demos use the [Cellular HAL libraries](http://ec2-52-36-17-39.us-west-2.compute.amazonaws.com/cellular/index.html) from a sub-moduled external project.  The Cellular HAL libraries expose the capability of a few popular cellular modems through a uniform API.  

1. Quectel BG96
2. Sierra HL7802
3. Ublox Sara R4 series

The MQTT and HTTP libraries of FreeRTOS use an abstract [Transport Interface](https://github.com/FreeRTOS/coreMQTT/blob/main/source/interface/transport_interface.h) to exchange data with any transport in a generic way.  The demos in this project offer a [reusable implementation](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-Cellular-Demo/blob/master/source/coreMQTT/using_mbedtls.c) of the [Transport Interface](https://github.com/FreeRTOS/coreMQTT/blob/main/source/interface/transport_interface.h) on top of the uniform API exposed by the Cellular HAL libraries.  

The demos in this project can be run in the [FreeRTOS Windows Simulator](https://freertos.org/FreeRTOS-Windows-Simulator-Emulator-for-Visual-Studio-and-Eclipse-MingW.html).  You will need a Windows PC and one of the supported cellular modems.  A version of Visual Studio such as the [free Community version of Visual Studios](https://visualstudio.microsoft.com/vs/community/) will be needed for building the demos.


## Components and Interfaces

This project makes use of 4 sub-modules from other GitHub projects, shown as yellow boxes in the diagram below. 

<img src="doc/cellular_component_and_interface.png" width="70%"><br>
Figure 1. Components and Interfaces

The components shown as blue boxes and dotted lines are provided by this project:

1. The [Transport Interface](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-Cellular-Demo/tree/main/source/coreMQTT) is needed by the MQTT library (sub-moduled from the [coreMQTT](https://github.com/freertos/coreMQTT) project) to send and receive packets.
2. The [mbedTLS platform porting layer](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-Cellular-Demo/tree/main/source/mbedtls) is needed by the mbedTLS library to run on FreeRTOS.
3. The [Comm Interface](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-Cellular-Demo/tree/main/source/cellular) is used by the Cellular HAL libraries to communicate with the cellular modems over UART connections.
4. The [Demo Application](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-Cellular-Demo/tree/main/source).

## Developer References and API Documents

Please refer to [cellular HAL library API document.](http://ec2-52-36-17-39.us-west-2.compute.amazonaws.com/cellular/index.html)


## Download the source code

This repo uses Git Submodules to bring in dependent components.
To clone using HTTPS:

```
git clone https://github.com/FreeRTOS/Lab-Project-FreeRTOS-Cellular-Demo.git --recurse-submodules
```

Using SSH:

```
git clone git@github.com:FreeRTOS/Lab-Project-FreeRTOS-Cellular-Demo.git --recurse-submodules
```

If you have downloaded the repo without using the `--recurse-submodules` argument, you need to run:

```
git submodule update --init --recursive
```

## Source Code Organization

The demo project is called mqtt_mutual_auth_demo.sln and can be found on [Github](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-Cellular-Demo/tree/main/projects) in the following directory for different cellular modules.

* [projects/bg96_mqtt_mutual_auth_demo](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-Cellular-Demo/tree/master/projects/bg96_mqtt_mutual_auth_demo)
* [projects/hl7802_mqtt_mutual_auth_demo](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-Cellular-Demo/tree/master/projects/hl7802_mqtt_mutual_auth_demo)
* [projects/sara_r4_mqtt_mutual_auth_demo](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-Cellular-Demo/tree/master/projects/sara_r4_mqtt_mutual_auth_demo)

```
./Lab-Project-FreeRTOS-Cellular-HAL
├── lib
│   ├──  cellular( submodule : Lab-Project-FreeRTOS-Cellular-HAL )
│   ├──  coreMQTT ( submodule : coreMQTT )
│   ├──  FreeRTOS ( submodule : FreeRTOS-Kernel )
│   └──  mbedtls ( submodule : mbedtls )
├── projects
│   ├──  bg96_mqtt_mutual_auth_demo ( demo project for Quectel bg96)
│   ├──  hl7802_mqtt_mutual_auth_demo ( demo project for Sierra Wireless hl7802)
│   └──  sara_r4_mqtt_mutual_auth_demo ( demo project for u-blox sara_r4)
└── source
    ├── cellular
    │   └── ( code for adapting Cellular HAL libraries with this demo )
    ├── coreMQTT
    │   └── ( code for adapting coreMQTT with this demo )
    ├── FreeRTOS
    │   └── ( code for adapting FreeRTOS with this demo )
    ├── mbedtls
    │   └── ( code for adapting mbedtls with this demo )
    ├── MutualAuthMQTTExample.c
    ├── cellular_setup.c
    ├── demo_config.h
    ├── exponential_backoff.c
    ├── exponential_backoff.h
    ├── logging_levels.h
    ├── logging_stack.h
    └── main.c
```



## Configure Application Settings

### **Configure cellular network**

Cellular configurations can be found in [“source/cellular/cellular_config.h”](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-Cellular-Demo/blob/master/source/cellular/cellular_config.h). Modify the following configuration for your network environment.
| Configuration   |      Description      |  Value |
|-----------------|-----------------------|--------|
| CELLULAR_COMM_INTERFACE_PORT | Cellular communication interface make use of COM port on computer to communicate with cellular module on windows simulator. | Your COM port connected to cellular module |
| CELLULAR_APN                 | Default APN for network registration. | Specify the value according to your network operator. |
| CELLULAR_PDN_CONTEXT_ID      | PDN context id for cellular network. | Default value is CELLULAR_PDN_CONTEXT_ID_MIN. |
| CELLULAR_PDN_CONNECT_TIMEOUT | PDN connect timeout for network registration. | Default value is 100000 milliseconds. |

Note

> The u-blox SARA-R4 series support set mobile network operators.
Set **CELLULAR_CONFIG_SET_MNO_PROFILE **in [config_r4.h](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-Cellular-HAL/blob/50b4ebaa1dc363efc3f7b03dbec9414c4dcda2a7/modules/sara_r4/cellular_r4.h) according to your mobile network operator.



### **Configure MQTT broker setting**

The configuration to connect MQTT broker can be found in “[source/demo_config.h](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-Cellular-Demo/blob/main/source/demo_config.h)”. Refer to the [documentation](https://www.freertos.org/mqtt/mutual-authentication-mqtt-example.html#configuration) for more information about the settings.

### **Configure other sub-modules**

[“source/FreeRTOS/FreeRTOSConfig.h](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-Cellular-Demo/blob/main/source/FreeRTOS/FreeRTOSConfig.h)”, “[source/mbedtls/mbedtls_config.h](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-Cellular-Demo/blob/master/source/mbedtls/mbedtls_config.h)” and “[source/coreMQTT/core_mqtt_config.h](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-Cellular-Demo/blob/main/source/coreMQTT/core_mqtt_config.h)” are configurations for the corresponding sub-modules. Please refer to the document of each module for more information.

## Demo app execution flow

The demo apps perform three types of operations:

1. Register to a cellular network
2. Establish a secure connection with the MQTT broker of AWS IoT
3. Perform MQTT operations

The following diagram illustrate the interactions between the demo apps and various components.
<img src="doc/cellular_demo_sequence.png"><br>

## Build and run the demos

1. Download the source code from [FreeRTOS repository.](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-Cellular-Demo)
2. Configure application settings following the “Configure Application Settings” section.
3. In Visual Studio, open one of the mqtt_mutual_auth_demo.sln projects that matches your cellular modem.
4. Compile and run.

The following is the console output of a successful execution of the BG96 mqtt_mutual_auth_demo.sln project. 

```
[INFO] [CELLULAR] [commTaskThread:287] Cellular commTaskThread started
>>>  Cellular SIM okay  <<<
>>>  Cellular GetServiceStatus failed 0, ps registration status 0  <<<
>>>  Cellular module registered  <<<
>>>  Cellular module registered, IP address 10.160.13.238  <<<
[INFO] [MQTTDemo] [prvConnectToServerWithBackoffRetries:583] Creating a TLS connection to a2zzppv7s4siea-ats.iot.us-west-2.amazonaws.com:8883.

[INFO] [MQTTDemo] [MQTTDemoTask:465] Creating an MQTT connection to a2zzppv7s4siea-ats.iot.us-west-2.amazonaws.com.

[INFO] [MQTTDemo] [prvCreateMQTTConnectionWithBroker:683] An MQTT connection is established with a2zzppv7s4siea-ats.iot.us-west-2.amazonaws.com.
[INFO] [MQTTDemo] [prvMQTTSubscribeWithBackoffRetries:741] Attempt to subscribe to the MQTT topic testClient13:24:47/example/topic.

[INFO] [MQTTDemo] [prvMQTTSubscribeWithBackoffRetries:748] SUBSCRIBE sent for topic testClient13:24:47/example/topic to broker.


[INFO] [MQTTDemo] [prvMQTTProcessResponse:872] Subscribed to the topic testClient13:24:47/example/topic with maximum QoS 1.

[INFO] [MQTTDemo] [MQTTDemoTask:479] Publish to the MQTT topic testClient13:24:47/example/topic.

[INFO] [MQTTDemo] [MQTTDemoTask:485] Attempt to receive publish message from broker.

[INFO] [MQTTDemo] [prvMQTTProcessResponse:853] PUBACK received for packet Id 2.

[INFO] [MQTTDemo] [MQTTDemoTask:490] Keeping Connection Idle...


[INFO] [MQTTDemo] [MQTTDemoTask:479] Publish to the MQTT topic testClient13:24:47/example/topic.

[INFO] [MQTTDemo] [MQTTDemoTask:485] Attempt to receive publish message from broker.

[INFO] [MQTTDemo] [prvMQTTProcessIncomingPublish:908] Incoming QoS : 1

[INFO] [MQTTDemo] [prvMQTTProcessIncomingPublish:919]
Incoming Publish Topic Name: testClient13:24:47/example/topic matches subscribed topic.
Incoming Publish Message : Hello World!
```

## 

