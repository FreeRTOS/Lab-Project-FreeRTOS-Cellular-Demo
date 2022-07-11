/*
 * Amazon FreeRTOS CELLULAR Preview Release
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

/* Cellular includes. */
#include "cellular_config.h"
#include "cellular_config_defaults.h"
#include "cellular_comm_interface.h"

#include "cellular_platform.h"

/*-----------------------------------------------------------*/

#define TEST_RX_DATA_RECEIVED_BIT   ( 0x00000001 )
#define TEST_SEND_RECV_TIMEOUT      ( 1000 )

/*-----------------------------------------------------------*/

extern CellularCommInterface_t CellularCommInterface;

/*-----------------------------------------------------------*/

static CellularCommInterfaceError_t receiveCallback( void * pUserData, CellularCommInterfaceHandle_t commInterfaceHandle )
{
    CellularCommInterfaceError_t commIfError = IOT_COMM_INTERFACE_SUCCESS;
    BaseType_t higherPriorityTaskWoken;
    PlatformEventGroupHandle_t eventGroupHandle = ( PlatformEventGroupHandle_t )pUserData;

    configASSERT( pUserData != NULL );
    
    PlatformEventGroup_SetBitsFromISR( eventGroupHandle, TEST_RX_DATA_RECEIVED_BIT, &higherPriorityTaskWoken );
    if( higherPriorityTaskWoken != pdFAIL )
    {
        portYIELD_FROM_ISR( higherPriorityTaskWoken );
    }
    return commIfError;
}
/*-----------------------------------------------------------*/

void CommIfDemo( void * pvParameters )
{
    CellularCommInterfaceError_t commIfError = IOT_COMM_INTERFACE_SUCCESS;
    CellularCommInterfaceHandle_t commIfHandle = NULL;
    uint32_t bytesSend = 0;
    uint32_t bytesRead = 0;
    uint32_t totalByteRead = 0;
    PlatformEventGroupHandle_t eventGroupHandle = NULL;
    PlatformEventGroup_EventBits eventBits = 0;
    char testMessage[] = "AT\r\n";
    char recvDataBuf[ 128 ];

    eventGroupHandle = PlatformEventGroup_Create();
    configASSERT( eventGroupHandle != NULL );

    /* Open comm interface without callback function. */
    commIfError = CellularCommInterface.open( receiveCallback,
                                              eventGroupHandle,
                                              &commIfHandle );
    configASSERT( commIfError == IOT_COMM_INTERFACE_SUCCESS );

    while( true )
    {
        /* Send some bytes through the comm interface. */
        configPRINTF( ( "Send to modem : %s", testMessage ) );
        commIfError = CellularCommInterface.send( commIfHandle,
                                                  testMessage,
                                                  sizeof( testMessage ),
                                                  TEST_SEND_RECV_TIMEOUT,
                                                  &bytesSend );
        if( commIfError != IOT_COMM_INTERFACE_SUCCESS )
        {
            continue;
        }

        /* Wait the event group, should timeout. */
        configPRINTF( ( "Wait receive callback function...\r\n" ) );
        eventBits = PlatformEventGroup_WaitBits( eventGroupHandle, 
                                                 TEST_RX_DATA_RECEIVED_BIT,
                                                 pdFALSE,
                                                 pdFALSE,
                                                 pdMS_TO_TICKS( TEST_SEND_RECV_TIMEOUT ) );
        if( eventBits != TEST_RX_DATA_RECEIVED_BIT )
        {
            continue;
        }

        /* Read and compare. */
        memset( recvDataBuf, sizeof( recvDataBuf ), 0 );
        totalByteRead = 0;
        do
        {
            commIfError = CellularCommInterface.recv( commIfHandle,
                                                      &recvDataBuf[ totalByteRead ],
                                                      sizeof( recvDataBuf ) - totalByteRead,
                                                      TEST_SEND_RECV_TIMEOUT,
                                                      &bytesRead );
            totalByteRead = totalByteRead + bytesRead;
        } while( commIfError != IOT_COMM_INTERFACE_SUCCESS );
        recvDataBuf[ totalByteRead ] = '\0';

        configPRINTF( ( "Received data : %s", recvDataBuf ) );
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    }
}
/*-----------------------------------------------------------*/
