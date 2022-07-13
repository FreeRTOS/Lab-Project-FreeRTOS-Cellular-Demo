# Introduction

This document describes how to integrate the cellular library to run MQTT mutual authentication demo on STM32 platform.
The demo is constructed incrementally. Checkpoints are provided in the each step to help verifing the integration result.


All of the only required sofware components are included in this repository. The demo repository seperates the code into the following categorires:
* library ( lib folder )
* library adapting software ( source folder )

The purpose of doing this is to help user integrate these libraries into their existing code base.
Library adapting software provides example implementation and may be modified to adapt to different platform.

# Incremental build steps and checkpoints

<p align="center"><img src="../../doc/cellular_component_and_interface.png" width="70%"><br>
Components and Interfaces</p>

Starting from the button of the software stack. The demo is built incrementally in the following orders:
1. **Creating an project with IDE tool**
    * Checkpoint : Compile and run the executable image to print some message without problem
2. **Integrate FreeRTOS kernel**
    * Checkpoint : Create cellularDemo task to print some message without problem
3. **Implement the comm interface for Cellular Interface**
    * Checkpoint : Run the comm_if_demo to interact with the cellular modem without problem
4. **Integrate the Cellular Interface**
    * Checkpoint : Run the setupCellular function to register to cellular network without problem
5. **Implement the transport interface**
    * Checkpoint : Run transport interface test in FreeRTOS-Libraries-Integration-Tests without problem
6. **Integrate the coreMQTT and mutual authenticated demo**
    * Checkpoint : Run the demo code without problem

If you encounter problems at checkpoint for integration step, it is recommended that you troubleshoot the problem before proceeding to the next step.

# Software and hardware requiremnt and environment setup

The followings hardware components are required to run this demo:
* [STM32F411RE Nucleo-64 board](https://www.st.com/en/evaluation-tools/nucleo-f411re.html)
* [LTE connectivity expansion board](https://www.st.com/content/st_com/en/products/evaluation-tools/solution-evaluation-tools/communication-and-connectivity-solution-eval-boards/steval-stmodlte.html)
* [x-NUCLEO-STMODA1](https://www.st.com/en/ecosystems/x-nucleo-stmoda1.html)

The following software components are used in this demo. These components and adapting interface are included in this repository:
* FreeRTOS kernel
* FreeRTOS Cellular Interface
* coreMQTT
* Backoff algorithm
* MbedTLS

[STM32 CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) is used to run this project.


## 1. Create a project from STM32 CubeIDE
Create a project from STM32 CubeIDE. 
* File->New->STM32 project 
    * Board Selector TAB->NECLEO-F411RE
    * Project Name : stm32_F411RE_nucleo_64_mqtt_mutual_auth_demo
    * Project location : <path_to_cellular_demo>\Lab-Project-FreeRTOS-Cellular-Demo\projects\stm32_F411RE_nucleo_64_mqtt_mutual_auth_demo
    * Targeted Language : C
    * Target Binary Type : Executable
    * Targeted Project Type : STM32Cube

### Device Configure Tool
NVIC
* NVIC tab :
    * Priority Group bits : 4-bits for pre-emption priority
    * USART1 Preemption Priority : 5
* Code configruation tab
    * Generate IRQ Handler : unselect the following
        * System service call via SWI instruction
        * Pendable requrest for system service
        * Time base: System tick timer

> Caution : Priority Group bits must be set to 4-bits due to the following reason
> 1. SVC must be call with its interrupt enabled. 0-bit can't be used.
> 2. Comm interface use USART1. USART1 interrupt handler calls FreeRTOS APIs. USART1 interrupt priority must lower than configMAX_SYSCALL_INTERRUPT_PRIORITY priority ( in this demo is 5 )

USART1
* Mode : Asynchronous mode
* Hardware Flow Control (RS232) : None
* USART1 global interrupt : enabled
* USART1 Preemption Priority : 5 ( Go to NVIC tab to udpate the value )
> Caution : CTS/RTS pins on x-NUCLEO-STMODA1 can't be assiged in F411RE Nucleo-64. Therefore, CTS/RTS must be disabled.

SYS
* Timebase Source : TIM1

> Caution : SYS tick will be used in FreeRTOS as tick source. SYS tick can't be used as SYS timebase source.
The SYS timebase is used in drivers. They are for different purpose.


### Add printf support
USART2 is for debugging.
USART1 is for cellular comm interface.

Add the following code in the Core/Src/main.c file.
```C
/* USER CODE BEGIN 4 */
int _write(int fd, char * ptr, int len)
{
    HAL_UART_Transmit( &huart2, (uint8_t *) ptr, len, HAL_MAX_DELAY );
    return len;
}
/* USER CODE END 4 */
```

### Checkpoint
We can try to print something with the printf function now.
* Add the folloinwg code to your main function and run the executable image.

```C
int main(void)
{
    ...
    /* USER CODE BEGIN 2 */
    printf( "-- Cellular interface MQTT mutual auth demo ---\r\n" );
    /* USER CODE END 2 */
    ...
}
```

## 2. Integrate FreeRTOS Kernel
The following brief steps can be used to integrate FreeRTOS libraries into your code base.
* Add library source files
* Add library include path
* Add library configuration files
* Add library adaption files
* Invoke library APIs in your application

### The steps to integrate FreeRTOS kernel
1. Add the following source files to your project
    * Lab-FreeRTOS-Cellular-Demo/lib/FreeRTOS/*.c 
    * Lab-FreeRTOS-Cellular-Demo/lib/FreeRTOS/portable/GCC/ARM_CM4F
2. Add the following include path to your project
    * ../../lib/FreeRTOS/include
    * ../../lib/FreeRTOS/portable/GCC/ARM_CM4F
3. Add the FreeRTOSConfig.h file to Core/Inc
4. Add the heap_4.c file to Core/Src
5. Add the hook function requried by FreeRTOS kernel and invoke the FreeRTOS Kernel
    * Reference the Core/Src/app_main.c file


### Checkpoint
We can try to create a task and print something inside the thread with the following code.
```C
void yourMainFunction( void )
{
    ...
    xTaskCreate( CellularDemoTask,         /* Function that implements the task. */
                 "CellularDemo",           /* Text name for the task - only used for debugging. */
                 2048, /* Size of stack (in words, not bytes) to allocate for the task. */
                 NULL,                     /* Task parameter - not used in this case. */
                 1,  /* Task priority, must be between 0 and configMAX_PRIORITIES - 1. */
                 NULL );                   /* Used to pass out a handle to the created task - not used in this case. */

    vTaskStartScheduler();
    ...
}

static void CellularDemoTask( void * pvParameters )
{
    for(;;)
    {
        configPRINTF( ( "FreeRTOS cellular demo task" ) );
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    }
}
```

### Note for STM32 CubeIDE
This section is provided to add existing source files with STM32 CubeIDE. Only the FreeRTOS/Source are described.
You can do the same for other source files.

1. Drag the Lab-FreeRTOS-Cellular-Demo/lib folder to your project
    * Selet **Link to files and folders**
    * Create link locations relative to : PROJECT_LOC

2. Add FreeRTOS kernel source files
Add source file
    * Right click on project name->New->Source Folder
        * Folder name->Browse->lib/FreeRTOS

3. Execlude unrequired source file
    * Execlude portable from FreeRTOS
        * Right click lib/FreeRTOS->Resource configuration->Execlude from build. Select all and okay.

4. Add the FreeRTOS include path to the project
    * Right click on project name->property->C/C++ General->Path and Symbols->Includes tab
    * Add the following path
        * ../../lib/FreeRTOS/include
        * ../../lib/FreeRTOS/portable/GCC/ARM_CM4F

## 3. Implement cellular comm interface
### The steps to implement celllar comm interface
1. Add source folder to source
    * source/cellular
2. Add the include path
    * ../../lib/cellular/source/include
    * ../../lib/cellular/source/interface
    * ../../source/cellular
    * ../../source/logging
3. Add the cellular_config.h
4. Add comm interface for ST
    * src
        * comm_if_st.c
        * iot_fifo.c
        * device_control.c
    * inc
        * iot_fifo.h

### Checkpoint
Comm interface should work now. We can open the comm interface and send some data to modem.
Modem should be powered on in comm interface open function.
* Add the comm_if_demo.c to Core/Src
* Invoke CommIfDemo in a FreeRTOS task

Example result

```
Send to modem : AT
Wait receive callback function...
Received data : AT

OK
```

## 4. Integrate the Cellular Interface
### The steps to integrate the Cellular Interface
1. Add source folder to source
    * lib/cellular/source
    * lib/cellular/modules/bg96

2. Add the following include path to project
    * lib/cellular/source/include/private
    * lib/cellular/source/include/common

3. copy the source to core/Src
    * source/cellular_setup.c

4. Patch the bg96 modem implementation
    * The default BG96 implementation default enable the flow control. The pins in F411RE Nucleo-64 board can be used as CTS/RTS.
    * Apply the following patch in BG96 modem driver and define CELLULAR_BG96_DISABLE_FLOW_CONTROL in cellular_config.h.
```C
		#ifndef CELLULAR_BG96_DISABLE_FLOW_CONTROL
			if( cellularStatus == CELLULAR_SUCCESS )
			{
				/* Enable RTS/CTS hardware flow control. */
				atReqGetNoResult.pAtCmd = "AT+IFC=2,2";
				cellularStatus = sendAtCommandWithRetryTimeout( pContext, &atReqGetNoResult );
			}
		#endif
````

### Checkpoint
The cellular library is ready. We can invoke the setupCellular function in the CellularDemoTask to register to cellular network.

Example result
```
[INFO] [CellularLib] [CellularDevice_PowerOn:288] CellularDevice_PowerOn +
[INFO] [CellularLib] [CellularDevice_PowerOn:339] CellularDevice_PowerOn -
[ERROR] [CellularLib] [_Cellular_AtcmdRequestTimeoutWithCallbackRaw:254] pkt_recv status=1, AT cmd ATE0 timed out
>>>  Cellular SIM okay  <<<
>>>  Cellular GetServiceStatus failed 0, ps registration status 0  <<<
>>>  Cellular GetServiceStatus failed 0, ps registration status 2  <<<
>>>  Cellular GetServiceStatus failed 0, ps registration status 2  <<<
>>>  Cellular module registered  <<<
>>>  Cellular module registered, IP address 100.102.60.4  <<<
[INFO] [CellularLib] [CellularDemoTask:144] ---------STARTING DEMO---------
```

## 5. Implement the transport interface for coreMQTT
### The steps to implement the transport interface for coreMQTT
1. Add the following source folder
    * source/coreMQTT
    * lib/ThirdParty/mbedtls/library
    
2. Add the following include path
    * ../../source/coreMQTT
    * ../../source/mbedtls
    * ../../lib/ThirdParty/mbedtls/include
    * ../../lib/coreMQTT/source/interface

3. Add the mbedtls_config.h to core/inc

5. Add the MBEDTLS_CONFIG_FILE macro to select the mbedtls config file
    * Right click project->property->C/C++ General->Paths and Symbol->Symbols
    * Create macro MBEDTLS_CONFIG_FILE with value "mbedtls_config.h"

4. Update the mbedlts_freertos_port.c file
    * The random function mbedtls_freertos_port.c file is implemented with WIN32 APIs.
    * Reference the Core/Src/mbedtls_freertos_port.c file for STM32 implementation

### Checkpoint
You can consider to use the [transport interface test](https://github.com/FreeRTOS/FreeRTOS-Libraries-Integration-Tests/tree/main/src/transport_interface) to verify your transort interface implementation.

## 6. Integrate the coreMQTT and mutual authenticated demo
### The steps to integrate the coreMQTT and mutual authenticated demo
1. Add coreMQTT source folder
    * lib/coreMQTT/source
2. Add coreMQTT include path
    * ../../lib/coreMQTT/source/include
3. Add the core_mqtt_config.h to Core/Inc
4. Add the demo code, MutualAuthMQTTExample.c, to Core/Src
5. Add the demo_config.h file to Core/Inc and update config in the file.
6. Update the cellular demo to invoke the RunMQTTTask in CellularDemoTask

### Checkpoint
The mutual authenticated demo application should be able to run now. The MQTT message can be observed from AWS IoT Core now.
