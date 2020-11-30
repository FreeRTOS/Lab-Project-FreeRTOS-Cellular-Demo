/*
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
 */

/*
 * Get device information from 1NCE server including:
 * 1. thingName
 * 2. AWS IoT Core endpoint
 * 3. AWS IoT Core root CA
 * 4. Device X.509 certificate
 * 5. Device private key
 */

/* Standard includes. */
#include <string.h>
#include <stdio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Demo Specific configs. */
#include "demo_config.h"

/* MQTT library includes. */
#include "core_mqtt.h"

/* Exponential backoff retry include. */
#include "exponential_backoff.h"

/* Transport interface implementation include header for TLS. */
#include "using_mbedtls.h"

/* 1NCE onboarding header*/
#include "1nce_zero_touch_provisioning.h"

/*-----------------------------------------------------------*/

/**
 * @brief 1NCE onboarding endpoint and port.
 */
#define ONBOARDING_ENDPOINT                  "device.connectivity-suite.cloud"
#define ONBOARDING_PORT                      ( 443U )

/**
 * @brief Transport timeout in milliseconds for transport send and receive.
 */
#define nceTRANSPORT_SEND_RECV_TIMEOUT_MS    ( 30000U )


/**
 * @brief Size of buffer to keep received data.
 */
#define RECV_BUFFER_LEN      ( 1800U )

/**
 * @brief Size of buffer to keep local strings.
 */
#define MAX_LOCAL_STR_LEN    ( 100U )

/**
 * @brief Size of buffer to keep certificate.
 */
#define MAX_CERT_LEN         ( 2000U )

/**
 * @brief Size of buffer to keep key.
 */
#define MAX_KEY_LEN          ( 3000U )

/*-----------------------------------------------------------*/
/* Static variables */

/**
 * @brief thing name acquired from onboarding response.
 */
static char nceThingName[ MAX_LOCAL_STR_LEN ];

/**
 * @brief AWS IoT endpoint acquired from onboarding response.
 */
static char nceEndpoint[ MAX_LOCAL_STR_LEN ];

/**
 * @brief new topic name started with thing name.
 */
static char nceExampleTopic[ MAX_LOCAL_STR_LEN ];

/**
 * @brief root CA acquired from onboarding response.
 */
static char rootCA[ MAX_CERT_LEN ];

/**
 * @brief client certificate acquired from onboarding response.
 */
static char clientCert[ MAX_CERT_LEN ];

/**
 * @brief device private key acquired from onboarding response.
 */
static char prvKey[ MAX_KEY_LEN ];

/**
 * @brief credentials acquired from onboarding response.
 */
static NetworkCredentials_t nceNetworkCredentials = { 0 };

/**
 * @brief TLS network object for connecting to 1NCE server.
 */
static NetworkContext_t xNetworkContext = { 0 };


/* A buffer to temporaryly save received data. */
static char PART[ RECV_BUFFER_LEN ];

/*-----------------------------------------------------------*/

/**
 * @brief Connect to 1NCE server with reconnection retries.
 *
 * If connection fails, retry is attempted after a timeout.
 * Timeout value will exponentially increase until maximum
 * timeout value is reached or the number of attempts are exhausted.
 *
 * @param[out] pxNetworkContext The parameter to return the created network context.
 *
 * @return The status of the final connection attempt.
 */
static TlsTransportStatus_t nce_connect( NetworkContext_t * pxNetworkContext );

/**
 * @brief re-initialize MQTT connection information
 * from received onboarding response.
 *
 * @return The status of the excution result.
 */
static uint8_t nceReinitConnParams( char * completeResponse,
                                    char ** pThingName,
                                    char ** pEndpoint,
                                    char ** pExampleTopic,
                                    char ** pRootCA,
                                    char ** pClientCert,
                                    char ** pPrvKey );

/**
 * @brief Replace matched patterns in a string.
 *
 * @param[in] orig: the target string to be replaced.
 * @param[in] rep: the string pattern to be replaced.
 * @param[in] with: the string pattern to replace.
 *
 * @return the pointer pointing to the new string after being replaced.
 */
char * str_replace( char * orig,
                    char * rep,
                    char * with );

/*-----------------------------------------------------------*/

uint8_t nce_onboard( char ** pThingName,
                     char ** pEndpoint,
                     char ** pExampleTopic,
                     char ** pRootCA,
                     char ** pClientCert,
                     char ** pPrvKey )
{
    uint8_t status = EXIT_FAILURE;
    char completeResponse[ 5000 ];

    memset( completeResponse, '\0', 5000 );

    LogInfo( ( "Start 1NCE device onboarding." ) );

    /* Create TLS connection for onboarding request. */
    TlsTransportStatus_t xNetworkStatus = nce_connect( &xNetworkContext );

    if( TLS_TRANSPORT_SUCCESS != xNetworkStatus )
    {
        LogError( ( "Failed to connect to 1NCE server." ) );
        return status;
    }

    /* Build onboarding request. */
    char packetToSent[ 100 ];

    memset( packetToSent, '\0', 100 * sizeof( char ) );
    sprintf( packetToSent, "GET /device-api/onboarding HTTP/1.1\r\n"
                           "Host: %s\r\n"
                           "Accept: text/csv\r\n\r\n", ONBOARDING_ENDPOINT );
    LogInfo( ( "Send onboarding request:\r\n%.*s",
               strlen( packetToSent ),
               packetToSent ) );

    /* Send onboarding request. */
    int32_t sentBytes = TLS_FreeRTOS_send( &xNetworkContext,
                                           &packetToSent,
                                           strlen( packetToSent ) );

    configASSERT( sentBytes > 0 );

    if( sentBytes <= 0 )
    {
        LogError( ( "Failed to send onboarding request." ) );
        return status;
    }

    /* Receive onboarding response. */
    int32_t recvBytes = TLS_FreeRTOS_recv( &xNetworkContext,
                                           &PART[ 0 ],
                                           RECV_BUFFER_LEN );

    if( recvBytes < 0 )
    {
        LogError( ( "Failed to receive onboarding response." ) );
        return status;
    }

    LogDebug( ( "Received raw response: %d bytes.", recvBytes ) );
    LogDebug( ( "\r\n%.*s", recvBytes, PART ) );
    strcat( completeResponse,
            strstr( PART, "Express\r\n\r\n\"" ) + strlen( "Express\r\n\r\n\"" ) );
    memset( PART, ( int8_t ) '\0', sizeof( PART ) );

    while( recvBytes == RECV_BUFFER_LEN )
    {
        recvBytes = TLS_FreeRTOS_recv( &xNetworkContext,
                                       &PART[ 0 ],
                                       RECV_BUFFER_LEN );

        if( recvBytes < 0 )
        {
            LogError( ( "Failed to receive onboarding response." ) );
            return status;
        }

        LogDebug( ( "Received raw response: %d bytes.", strlen( PART ) ) );
        LogDebug( ( "\r\n%.*s", strlen( PART ), PART ) );
        strcat( completeResponse, PART );
        memset( PART, ( int8_t ) '\0', sizeof( PART ) );
    }

    LogInfo( ( " Onboarding response is received." ) );

    /* Disconnect onboarding TLS connection. */
    TLS_FreeRTOS_Disconnect( &xNetworkContext );

    /* Re-initialize MQTT connection information with onboarding information. */
    status = nceReinitConnParams( completeResponse,
                                  pThingName,
                                  pEndpoint,
                                  pExampleTopic,
                                  pRootCA,
                                  pClientCert,
                                  pPrvKey );
    return status;
}

/*-----------------------------------------------------------*/

TlsTransportStatus_t nce_connect( NetworkContext_t * pxNetworkContext )
{
    TlsTransportStatus_t xNetworkStatus = TLS_TRANSPORT_CONNECT_FAILURE;
    RetryUtilsStatus_t xRetryUtilsStatus = RetryUtilsSuccess;
    RetryUtilsParams_t xReconnectParams;
    NetworkCredentials_t tNetworkCredentials = { 0 };

    LogInfo( ( "Connecting to 1NCE server." ) );

    tNetworkCredentials.disableSni = democonfigDISABLE_SNI;
    /* Set the credentials for establishing a TLS connection. */
    tNetworkCredentials.pRootCa = ( const unsigned char * ) democonfigROOT_CA_PEM;
    tNetworkCredentials.rootCaSize = sizeof( democonfigROOT_CA_PEM );
    tNetworkCredentials.pClientCert = ( const unsigned char * ) democonfigCLIENT_CERTIFICATE_PEM;
    tNetworkCredentials.clientCertSize = sizeof( democonfigCLIENT_CERTIFICATE_PEM );
    tNetworkCredentials.pPrivateKey = ( const unsigned char * ) democonfigCLIENT_PRIVATE_KEY_PEM;
    tNetworkCredentials.privateKeySize = sizeof( democonfigCLIENT_PRIVATE_KEY_PEM );

    /* Initialize reconnect attempts and interval. */
    RetryUtils_ParamsReset( &xReconnectParams );
    xReconnectParams.maxRetryAttempts = MAX_RETRY_ATTEMPTS;

    /* Attempt to connect to 1NCE server. If connection fails, retry after
     * a timeout. Timeout value will exponentially increase till maximum
     * attempts are reached.
     */
    do
    {
        LogInfo( ( "Creating a TLS connection to %s:%u.",
                   ONBOARDING_ENDPOINT,
                   ONBOARDING_PORT ) );
        /* Attempt to create a mutually authenticated TLS connection. */
        xNetworkStatus = TLS_FreeRTOS_Connect( pxNetworkContext,
                                               ONBOARDING_ENDPOINT,
                                               ONBOARDING_PORT,
                                               &tNetworkCredentials,
                                               nceTRANSPORT_SEND_RECV_TIMEOUT_MS,
                                               nceTRANSPORT_SEND_RECV_TIMEOUT_MS );

        if( xNetworkStatus != TLS_TRANSPORT_SUCCESS )
        {
            LogWarn( ( "Connection to 1NCE server failed. Retrying connection with backoff and jitter." ) );
            xRetryUtilsStatus = RetryUtils_BackoffAndSleep( &xReconnectParams );
        }

        if( xRetryUtilsStatus == RetryUtilsRetriesExhausted )
        {
            LogError( ( "Connection to 1NCE server failed, all attempts exhausted." ) );
            xNetworkStatus = TLS_TRANSPORT_CONNECT_FAILURE;
        }
    } while ( ( xNetworkStatus != TLS_TRANSPORT_SUCCESS ) && ( xRetryUtilsStatus == RetryUtilsSuccess ) );

    return xNetworkStatus;
}

/*-----------------------------------------------------------*/

static uint8_t nceReinitConnParams( char * completeResponse,
                                    char ** pThingName,
                                    char ** pEndpoint,
                                    char ** pExampleTopic,
                                    char ** pRootCA,
                                    char ** pClientCert,
                                    char ** pPrvKey )
{
    uint8_t status = EXIT_FAILURE;

    int i = 0;
    int32_t strSize = 0;
    int32_t offset = 0;
    char find[] = "\\n";
    char replaceWith[] = "\n";
    char endKey[] = "-----END RSA PRIVATE KEY-----";
    int32_t endKeyLen = sizeof( endKey );
    char endCert[] = "-----END CERTIFICATE-----";
    int32_t endCertLen = sizeof( endCert );
    char * location = NULL;

    memset( nceThingName, '\0', sizeof( nceThingName ) );
    memset( nceEndpoint, '\0', sizeof( nceEndpoint ) );
    memset( nceExampleTopic, '\0', sizeof( nceExampleTopic ) );
    memset( rootCA, '\0', sizeof( rootCA ) );
    memset( clientCert, '\0', sizeof( clientCert ) );
    memset( prvKey, '\0', sizeof( prvKey ) );

    if( NULL == completeResponse )
    {
        LogError( ( "input argument is null." ) );
        return status;
    }

    /* Get the first token. */
    char * token = strtok( completeResponse, "," );

    strSize = strlen( token );
    token[ strSize - 1 ] = '\0';

    if( sizeof( nceThingName ) < strSize )
    {
        LogError( ( "nceThingName array size is not enough to hold incoming thing name." ) );
        return status;
    }

    memcpy( nceThingName, token, strSize );
    LogDebug( ( "Thing name is: %s.", nceThingName ) );

    /* Walk through other tokens. */
    while( token != NULL )
    {
        token = strtok( NULL, "," );

        if( i == 0 )
        {
            strSize = strlen( token ) - 2;

            if( sizeof( nceEndpoint ) < ( strSize + 1 ) )
            {
                LogError( ( "nceEndpoint array size is not enough to hold incoming endpoint." ) );
                return status;
            }

            memcpy( nceEndpoint, token + 1, strSize );
            LogDebug( ( "IoTEndpoint is: %s.", nceEndpoint ) );
        }

        /*
         * if(i == 1) {
         *  memcpy(amazonRootCaUrl, token, strlen(token));
         * }
         */
        if( i == 2 )
        {
            /* Process root.pem. */
            memcpy( PART, token + 1, strlen( token ) - 1 );
            char * result = str_replace( PART, find, replaceWith );
            memcpy( PART, result, strlen( PART ) );
            vPortFree( result );
            offset = ( int32_t ) ( &PART[ 0 ] );
            location = strstr( PART, endCert );
            strSize = ( int32_t ) location + endCertLen - offset;
            PART[ strSize ] = '\0';
            nceNetworkCredentials.rootCaSize = strSize + 1;

            if( sizeof( rootCA ) < ( strSize + 1 ) )
            {
                LogError( ( "rootCA array size is not enough to hold incoming root CA." ) );
                return status;
            }

            memcpy( rootCA, PART, ( strSize + 1 ) );
            ( void ) memset( &PART, ( int8_t ) '\0', sizeof( PART ) );

            LogDebug( ( "\r\n%s", rootCA ) );
        }

        if( i == 3 )
        {
            /* Process client_cert.pem. */
            memcpy( PART, token + 1, strlen( token ) - 1 );
            char * result = str_replace( PART, find, replaceWith );
            memcpy( PART, result, strlen( PART ) );
            vPortFree( result );
            offset = ( int32_t ) ( &PART[ 0 ] );
            location = strstr( PART, endCert );
            strSize = ( int32_t ) location + endCertLen - offset;
            PART[ strSize ] = '\0';

            if( sizeof( clientCert ) < ( strSize + 1 ) )
            {
                LogError( ( "clientCert array size is not enough to hold incoming client certificate." ) );
                return status;
            }

            memcpy( clientCert, PART, ( strSize + 1 ) );
            ( void ) memset( &PART, ( int8_t ) '\0', sizeof( PART ) );

            LogDebug( ( "\r\n%s", clientCert ) );
        }

        if( i == 4 )
        {
            /* Process client_key.pem. */
            memcpy( PART, token + 1, strlen( token ) - 1 );
            char * result = str_replace( PART, find, replaceWith );
            memcpy( PART, result, strlen( PART ) );
            vPortFree( result );
            offset = ( int32_t ) ( &PART[ 0 ] );
            location = strstr( PART, endKey );
            strSize = ( int32_t ) location + endKeyLen - offset;
            PART[ strSize ] = '\0';

            if( sizeof( prvKey ) < ( strSize + 1 ) )
            {
                LogError( ( "prvKey size is not enough to hold incoming client private key." ) );
                return status;
            }

            memcpy( prvKey, PART, ( strSize + 1 ) );
            ( void ) memset( &PART, ( int8_t ) '\0', sizeof( PART ) );

            LogDebug( ( "\r\n%s", prvKey ) );
        }

        i++;
    }

    /* Initialize MQTT topic. */
    strSize = strlen( nceThingName ) + strlen( "/example/topic" ) + 1;

    if( sizeof( nceExampleTopic ) < strSize )
    {
        LogError( ( "nceExampleTopic size is not enough to hold new example topic." ) );
        return status;
    }

    sprintf( nceExampleTopic, "%s/example/topic", nceThingName );

    *pThingName = nceThingName;
    *pEndpoint = nceEndpoint;
    *pExampleTopic = nceExampleTopic;
    *pRootCA = rootCA;
    *pClientCert = clientCert;
    *pPrvKey = prvKey;

    LogInfo( ( "Connection Info is re-initialized." ) );

    status = EXIT_SUCCESS;
    return status;
}

/*-----------------------------------------------------------*/

char * str_replace( char * orig,
                    char * rep,
                    char * with )
{
    char * result = NULL;
    char * ins = NULL;
    char * tmp = NULL;
    int lenRep = 0;
    int lenWith = 0;
    int lenFront = 0;
    int count = 0;

    if( !orig || !rep )
    {
        return NULL;
    }

    lenRep = strlen( rep );

    if( lenRep == 0 )
    {
        return NULL;
    }

    if( !with )
    {
        with = "";
    }

    lenWith = strlen( with );
    ins = orig;

    for( count = 0; tmp = strstr( ins, rep ); ++count )
    {
        ins = tmp + lenRep;
    }

    tmp = result = pvPortMalloc( strlen( orig ) + ( lenWith - lenRep ) * count + 1 );

    if( !result )
    {
        return NULL;
    }

    while( count-- )
    {
        ins = strstr( orig, rep );
        lenFront = ins - orig;
        tmp = strncpy( tmp, orig, lenFront ) + lenFront;
        tmp = strcpy( tmp, with ) + lenWith;
        orig += lenFront + lenRep;
    }

    strcpy( tmp, orig );
    return result;
}

/*-----------------------------------------------------------*/
