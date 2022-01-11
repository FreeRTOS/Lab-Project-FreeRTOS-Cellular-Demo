/*
 * FreeRTOS
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

/**
 * @file cellular_setup.c
 * @brief Setup cellular connectivity for board with cellular module.
 */

/* FreeRTOS include. */
#include <FreeRTOS.h>
#include "task.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* FreeRTOS Cellular Library include. */
#include "cellular_config.h"
#include "cellular_config_defaults.h"
#include "cellular_types.h"
#include "cellular_api.h"
#include "cellular_comm_interface.h"

/*-----------------------------------------------------------*/

#ifndef CELLULAR_APN
    #error "CELLULAR_APN is not defined in cellular_config.h"
#endif

#define CELLULAR_SIM_CARD_WAIT_INTERVAL_MS       ( 500UL )
#define CELLULAR_MAX_SIM_RETRY                   ( 5U )

#define CELLULAR_PDN_CONNECT_WAIT_INTERVAL_MS    ( 1000UL )

#define CELLULAR_PDN_CONTEXT_NUM                 ( CELLULAR_PDN_CONTEXT_ID_MAX - CELLULAR_PDN_CONTEXT_ID_MIN + 1U )

/*-----------------------------------------------------------*/

/* the default Cellular comm interface in system. */
extern CellularCommInterface_t CellularCommInterface;

/*-----------------------------------------------------------*/

/* Secure socket needs application to provide the cellular handle and pdn context id. */
/* User of secure sockets cellular should provide this variable. */
CellularHandle_t CellularHandle = NULL;

/* User of secure sockets cellular should provide this variable. */
uint8_t CellularSocketPdnContextId = CELLULAR_PDN_CONTEXT_ID;

/*-----------------------------------------------------------*/

bool setupCellular( void )
{
    bool cellularRet = true;
    CellularError_t cellularStatus = CELLULAR_SUCCESS;
    CellularSimCardStatus_t simStatus = { 0 };
    CellularServiceStatus_t serviceStatus = { 0 };
    CellularCommInterface_t * pCommIntf = &CellularCommInterface;
    uint8_t tries = 0;
    CellularPdnConfig_t pdnConfig = { CELLULAR_PDN_CONTEXT_IPV4, CELLULAR_PDN_AUTH_NONE, CELLULAR_APN, "", "" };
    CellularPdnStatus_t PdnStatusBuffers[ CELLULAR_PDN_CONTEXT_NUM ] = { 0 };
    char localIP[ CELLULAR_IP_ADDRESS_MAX_SIZE ] = { '\0' };
    uint32_t timeoutCountLimit = ( CELLULAR_PDN_CONNECT_TIMEOUT / CELLULAR_PDN_CONNECT_WAIT_INTERVAL_MS ) + 1U;
    uint32_t timeoutCount = 0;
    uint8_t NumStatus = 0;
    bool pdnStatus = false;
    uint32_t i = 0U;

    /* Initialize Cellular Comm Interface. */
    cellularStatus = Cellular_Init( &CellularHandle, pCommIntf );

    if( cellularStatus != CELLULAR_SUCCESS )
    {
        configPRINTF( ( ">>>  Cellular_Init failure %d  <<<\r\n", cellularStatus ) );
    }
    else
    {
        /* wait until SIM is ready */
        for( tries = 0; tries < CELLULAR_MAX_SIM_RETRY; tries++ )
        {
            cellularStatus = Cellular_GetSimCardStatus( CellularHandle, &simStatus );

            if( ( cellularStatus == CELLULAR_SUCCESS ) &&
                ( ( simStatus.simCardState == CELLULAR_SIM_CARD_INSERTED ) &&
                  ( simStatus.simCardLockState == CELLULAR_SIM_CARD_READY ) ) )
            {
                configPRINTF( ( ">>>  Cellular SIM okay  <<<\r\n" ) );
                break;
            }
            else
            {
                configPRINTF( ( ">>>  Cellular SIM card state %d, Lock State %d <<<\r\n",
                                simStatus.simCardState,
                                simStatus.simCardLockState ) );
            }

            vTaskDelay( pdMS_TO_TICKS( CELLULAR_SIM_CARD_WAIT_INTERVAL_MS ) );
        }

        if( cellularStatus != CELLULAR_SUCCESS )
        {
            configPRINTF( ( ">>>  Cellular SIM failure  <<<\r\n" ) );
        }
    }

    /* Setup the PDN config. */
    if( cellularStatus == CELLULAR_SUCCESS )
    {
        cellularStatus = Cellular_SetPdnConfig( CellularHandle, CellularSocketPdnContextId, &pdnConfig );

        if( cellularStatus != CELLULAR_SUCCESS )
        {
            configPRINTF( ( ">>>  Cellular_SetPdnConfig failure %d  <<<\r\n", cellularStatus ) );
        }
    }

    /* Rescan network. */
    if( cellularStatus == CELLULAR_SUCCESS )
    {
        cellularStatus = Cellular_RfOff( CellularHandle );

        if( cellularStatus != CELLULAR_SUCCESS )
        {
            configPRINTF( ( ">>>  Cellular_RfOff failure %d  <<<\r\n", cellularStatus ) );
        }
    }

    if( cellularStatus == CELLULAR_SUCCESS )
    {
        cellularStatus = Cellular_RfOn( CellularHandle );

        if( cellularStatus != CELLULAR_SUCCESS )
        {
            configPRINTF( ( ">>>  Cellular_RfOn failure %d  <<<\r\n", cellularStatus ) );
        }
    }

    /* Get service status. */
    if( cellularStatus == CELLULAR_SUCCESS )
    {
        while( timeoutCount < timeoutCountLimit )
        {
            cellularStatus = Cellular_GetServiceStatus( CellularHandle, &serviceStatus );

            if( ( cellularStatus == CELLULAR_SUCCESS ) &&
                ( ( serviceStatus.psRegistrationStatus == REGISTRATION_STATUS_REGISTERED_HOME ) ||
                  ( serviceStatus.psRegistrationStatus == REGISTRATION_STATUS_ROAMING_REGISTERED ) ) )
            {
                configPRINTF( ( ">>>  Cellular module registered  <<<\r\n" ) );
                break;
            }
            else
            {
                configPRINTF( ( ">>>  Cellular GetServiceStatus failed %d, ps registration status %d  <<<\r\n",
                                cellularStatus, serviceStatus.psRegistrationStatus ) );
            }

            timeoutCount++;

            if( timeoutCount >= timeoutCountLimit )
            {
                configPRINTF( ( ">>>  Cellular module can't be registered  <<<\r\n" ) );
            }

            vTaskDelay( pdMS_TO_TICKS( CELLULAR_PDN_CONNECT_WAIT_INTERVAL_MS ) );
        }
    }

    #ifdef CELLULAR_MODEM_NO_EPS_NETWORK
        if( cellularStatus == CELLULAR_SUCCESS )
        {
            /*Quectel GSM Modules require starting the TCPIP task*/
            cellularStatus = Cellular_StartTCPIP( CellularHandle );

            if( cellularStatus != CELLULAR_SUCCESS )
            {
                configPRINTF( ( ">>>  Cellular_StartTCPIP failure %d  <<<\r\n", cellularStatus ) );
            }
        }
    #endif /* ifdef CELLULAR_MODEM_NO_EPS_NETWORK */

    if( cellularStatus == CELLULAR_SUCCESS )
    {
        cellularStatus = Cellular_ActivatePdn( CellularHandle, CellularSocketPdnContextId );

        if( cellularStatus != CELLULAR_SUCCESS )
        {
            configPRINTF( ( ">>>  Cellular_ActivatePdn failure %d  <<<\r\n", cellularStatus ) );
        }
    }

    if( cellularStatus == CELLULAR_SUCCESS )
    {
        cellularStatus = Cellular_GetIPAddress( CellularHandle, CellularSocketPdnContextId, localIP, sizeof( localIP ) );

        if( cellularStatus != CELLULAR_SUCCESS )
        {
            configPRINTF( ( ">>>  Cellular_GetIPAddress failure %d  <<<\r\n", cellularStatus ) );
        }
    }

    #ifndef CELLULAR_MODEM_NO_EPS_NETWORK
    if( cellularStatus == CELLULAR_SUCCESS )
    {
        cellularStatus = Cellular_GetPdnStatus( CellularHandle, PdnStatusBuffers, CELLULAR_PDN_CONTEXT_NUM, &NumStatus );

        if( cellularStatus != CELLULAR_SUCCESS )
        {
            configPRINTF( ( ">>>  Cellular_GetPdnStatus failure %d  <<<\r\n", cellularStatus ) );
        }
    }

    if( cellularStatus == CELLULAR_SUCCESS )
    {
        for( i = 0U; i < NumStatus; i++ )
        {
            if( ( PdnStatusBuffers[ i ].contextId == CellularSocketPdnContextId ) && ( PdnStatusBuffers[ i ].state == 1 ) )
            {
                pdnStatus = true;
                break;
            }
        }

        if( pdnStatus == false )
        {
            configPRINTF( ( ">>>  Cellular PDN is not activated <<<" ) );
        }
    }

    if( ( cellularStatus == CELLULAR_SUCCESS ) && ( pdnStatus == true ) )
    #else /* ifndef CELLULAR_MODEM_NO_EPS_NETWORK */
    if (cellularStatus == CELLULAR_SUCCESS)
    #endif /* ifdef CELLULAR_MODEM_NO_EPS_NETWORK */
    {
        configPRINTF( ( ">>>  Cellular module registered, IP address %s  <<<\r\n", localIP ) );
        cellularRet = true;
    }
    else
    {
        cellularRet = false;
    }

    return cellularRet;
}

/*-----------------------------------------------------------*/
