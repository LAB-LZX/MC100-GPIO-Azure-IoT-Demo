/*

 MBYZ.cpp
 Version 0.7.1

 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include "iothub.h"
#include "iothub_device_client.h"
#include "iothub_client_options.h"
#include "iothub_message.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "azure_c_shared_utility/tickcounter.h"

#include "iothubtransportmqtt.h"

#ifdef SET_TRUSTED_CERT_IN_SAMPLES
#include "certs.h"
#endif // SET_TRUSTED_CERT_IN_SAMPLES

#include "ArduinoJson.h"


const char *files[] = { "/sys/gpio_board_IO/inputs/input0/value",
                        "/sys/gpio_board_IO/inputs/input1/value",
                        "/sys/gpio_board_IO/inputs/input2/value",
                        "/sys/gpio_board_IO/inputs/input3/value",
                        "/sys/gpio_board_IO/inputs/input4/value",
                        "/sys/gpio_board_IO/inputs/input5/value",
                        "/sys/gpio_board_IO/inputs/input6/value",
                        "/sys/gpio_board_IO/inputs/input7/value",
                        "/sys/gpio_board_IO/inputs/inputCounter0/value",
                        "/sys/gpio_board_IO/inputs/inputCounter1/value",
                        "/sys/gpio_board_IO/inputs/inputCounter2/value",
                        "/sys/gpio_board_IO/inputs/inputCounter3/value",
                        "/sys/gpio_board_IO/inputs/inputCounter4/value",
                        "/sys/gpio_board_IO/inputs/inputCounter5/value",
                        "/sys/gpio_board_IO/inputs/inputCounter6/value",
                        "/sys/gpio_board_IO/inputs/inputCounter7/value",
                        "/sys/gpio_board_IO/outputs/output0/value",
                        "/sys/gpio_board_IO/outputs/output1/value",
                        "/sys/gpio_board_IO/outputs/output2/value",
                        "/sys/gpio_board_IO/outputs/output3/value",
                        "/sys/gpio_board_IO/outputs/output4/value",
                        "/sys/gpio_board_IO/outputs/output5/value",
                        "/sys/gpio_board_IO/outputs/output6/value",
                        "/sys/gpio_board_IO/outputs/output7/value",
                        "/sys/gpio_board_ADC/voltage/volt0/value",
                        "/sys/gpio_board_ADC/voltage/volt1/value",
                        "/sys/gpio_board_ADC/ampere/amp0/value",
                        "/sys/gpio_board_ADC/ampere/amp1/value",
//                        "/sys/mc100_leds/led1/mode",
//                        "/sys/mc100_leds/led2/mode",
                        "\0"};

static int *values;

static bool g_continueRunning = true;
int g_interval = 10000;  // 10 sec send interval initially
static size_t g_message_count_send_confirmations = 0;

DynamicJsonDocument doc(8192);
const char *keys[] = {
    "gpio_board_IO/outputs/output0/value",
    "gpio_board_IO/outputs/output1/value",
    "gpio_board_IO/outputs/output2/value",
    "gpio_board_IO/outputs/output3/value",
    "gpio_board_IO/outputs/output4/value",
    "gpio_board_IO/outputs/output5/value",
    "gpio_board_IO/outputs/output6/value",
    "gpio_board_IO/outputs/output7/value",
    "\0"};

static int processMessage(const unsigned char *message) {
    doc.clear();

    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        printf("deserializeJson() failed: %s\n", error.c_str());
        return(-1);
    }

    int n = (int)(sizeof(keys) / sizeof(unsigned long)) - 1;
    
    for(int i = 0; i < n; i++) {
        if(doc.containsKey(keys[i])) {
            //printf("%s : %i \n", keys[i], (int) doc[keys[i]]);

            char fnbuf[256];
            sprintf(fnbuf, "/sys/%s", keys[i]);
            FILE *f = fopen(fnbuf, "w");
            
            char vbuf[256];
            sprintf(vbuf, "%i\n", (int) doc[keys[i]]);
            //fwrite(vbuf , 1 , sizeof(vbuf), f);
            printf("%s:%s:%i\n", fnbuf, vbuf, fwrite(vbuf , 1 , strlen(vbuf), f));
            
            fclose(f);
        }
    }

    return 0;
}

static IOTHUBMESSAGE_DISPOSITION_RESULT receive_msg_callback(IOTHUB_MESSAGE_HANDLE message, void* user_context) {
    (void)user_context;
    const char* messageId;
    const char* correlationId;
    
    // Message properties
    if ((messageId = IoTHubMessage_GetMessageId(message)) == NULL) {
        messageId = "<unavailable>";
    }
    
    if ((correlationId = IoTHubMessage_GetCorrelationId(message)) == NULL) {
        correlationId = "<unavailable>";
    }
    
    IOTHUBMESSAGE_CONTENT_TYPE content_type = IoTHubMessage_GetContentType(message);
    
    if (content_type == IOTHUBMESSAGE_BYTEARRAY) {
        const unsigned char* buff_msg;
        size_t buff_len;
        
        if (IoTHubMessage_GetByteArray(message, &buff_msg, &buff_len) != IOTHUB_MESSAGE_OK) {
            (void)printf("Failure retrieving byte array message\r\n");
        }
        else {
            (void)printf("Received Binary message\r\nMessage ID: %s\r\n Correlation ID: %s\r\n Data: <<<%.*s>>> & Size=%d\r\n", messageId, correlationId, (int)buff_len, buff_msg, (int)buff_len);
            processMessage(buff_msg);
        }
    }
    else {
        const char* string_msg = IoTHubMessage_GetString(message);
        if (string_msg == NULL) {
            (void)printf("Failure retrieving byte array message\r\n");
        }
        else {
            (void)printf("Received String Message\r\nMessage ID: %s\r\n Correlation ID: %s\r\n Data: <<<%s>>>\r\n", messageId, correlationId, string_msg);
        }
    }
    return IOTHUBMESSAGE_ACCEPTED;
}


static int device_method_callback(const char* method_name, const unsigned char* payload, size_t size, unsigned char** response, size_t* resp_size, void* userContextCallback) {
    const char* SetTelemetryIntervalMethod = "SetTelemetryInterval";
    const char* device_id = (const char*)userContextCallback;
    char* end = NULL;
    int newInterval;
    
    int status = 501;
    const char* RESPONSE_STRING = "{ \"Response\": \"Unknown method requested.\" }";
    
    (void)printf("\r\nDevice Method called for device %s\r\n", device_id);
    (void)printf("Device Method name:    %s\r\n", method_name);
    (void)printf("Device Method payload: %.*s\r\n", (int)size, (const char*)payload);
    
    if (strcmp(method_name, SetTelemetryIntervalMethod) == 0) {
        if (payload) {
            newInterval = (int)strtol((char*)payload, &end, 10);
            
            // Interval must be greater than zero.
            if (newInterval > 0) {
                // expect sec and covert to ms
                g_interval = 1000 * (int)strtol((char*)payload, &end, 10);
                status = 200;
                RESPONSE_STRING = "{ \"Response\": \"Telemetry reporting interval updated.\" }";
            }
            else {
                status = 500;
                RESPONSE_STRING = "{ \"Response\": \"Invalid telemetry reporting interval.\" }";
            }
        }
    }

    if (strcmp(method_name, "EnableOutput") == 0) {
        if (payload) {
            int channel = atoi((const char*)(payload + 1));
            
            if (channel >= 0 && channel <= 7) {
                char fnbuf[256];
                sprintf(fnbuf, "/sys/gpio_board_IO/outputs/output%i/value", channel);
                FILE *f = fopen(fnbuf, "w");
                printf("%s:1 -> %i", fnbuf, fwrite("1\n" , 1 , 2, f));
                fclose(f);
                status = 200;
                RESPONSE_STRING = "{ \"Response\": \"Channel enabled.\" }";
            }
            else {
                status = 500;
                RESPONSE_STRING = "{ \"Response\": \"Invalid channel.\" }";
            }
        }
    }

    
    if (strcmp(method_name, "DisableOutput") == 0) {
        if (payload) {
            int channel = atoi((const char *)(payload + 1));

            if (channel >= 0 && channel <= 7) {
                char fnbuf[256];
                sprintf(fnbuf, "/sys/gpio_board_IO/outputs/output%i/value", channel);
                FILE *f = fopen(fnbuf, "w");
                printf("%s:0 -> %i", fnbuf, fwrite("0\n" , 1 , 2, f));
                fclose(f);
                status = 200;
                RESPONSE_STRING = "{ \"Response\": \"Channel disabled.\" }";
            }
            else {
                status = 500;
                RESPONSE_STRING = "{ \"Response\": \"Invalid channel.\" }";
            }
        }
    }

        
    if (strcmp(method_name, "Reload") == 0) {
        int n = (int)(sizeof(files) / sizeof(unsigned long)) - 1;
        for(int i = 0; i < n; i++) {
            values[i] = -1;
        }
        RESPONSE_STRING = "{ \"Response\": \"reloaded.\" }";
    }

    
    (void)printf("\r\nResponse status: %d\r\n", status);
    (void)printf("Response payload: %s\r\n\r\n", RESPONSE_STRING);
    
    *resp_size = strlen(RESPONSE_STRING);
    if ((*response = (unsigned char *) malloc(*resp_size)) == NULL) {
        status = -1;
    }
    else {
        memcpy(*response, RESPONSE_STRING, *resp_size);
    }
    
    return status;
}


static void connection_status_callback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* user_context) {
    (void)reason;
    (void)user_context;
    // This sample DOES NOT take into consideration network outages.
    if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED) {
        (void)printf("The device client is connected to iothub\r\n");
    }
    else {
        (void)printf("The device client has been disconnected\r\n");
    }
}

static void send_confirm_callback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback) {
    (void)userContextCallback;
    // When a message is sent this callback will get envoked
    g_message_count_send_confirmations++;
    (void)printf("Confirmation callback received for message %lu with result %s\r\n", (unsigned long)g_message_count_send_confirmations, MU_ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
}


int main(int argc, char **argv) {
    if(argc != 2) {
        printf("usage: %s  <connectionString>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int n = (int)(sizeof(files) / sizeof(unsigned long)) - 1;
    
    FILE* *fp  = (FILE **) malloc(n * sizeof(FILE *));
    values = (int *) malloc(n * sizeof(int));
    
    for(int i = 0; i < n; i++) {
        fp[i] = fopen(files[i], "r");
        if(fp[i] == NULL) exit(EXIT_FAILURE);
        
        values[i] = -1;
    }

    IOTHUB_CLIENT_TRANSPORT_PROVIDER protocol;
    
    IOTHUB_MESSAGE_HANDLE message_handle;
    char telemetry_msg_buffer[2048];
    
    int messagecount = 0;

    protocol = MQTT_Protocol;

    IOTHUB_DEVICE_CLIENT_HANDLE device_handle;
    
    // Used to initialize IoTHub SDK subsystem
    (void)IoTHub_Init();
    
    (void)printf("Creating IoTHub handle\r\n");
    // Create the iothub handle here
    device_handle = IoTHubDeviceClient_CreateFromConnectionString(argv[1], protocol);
    if (device_handle == NULL)
    {
        (void)printf("Failure creating Iothub device.  Check your connection string.\r\n");
    }
    else
    {
        // Setting message callback to get C2D messages
        (void)IoTHubDeviceClient_SetMessageCallback(device_handle, receive_msg_callback, NULL);
        // Setting method callback to handle a SetTelemetryInterval method to control
        //   how often telemetry messages are sent from the simulated device.
        (void)IoTHubDeviceClient_SetDeviceMethodCallback(device_handle, device_method_callback, NULL);
        // Setting connection status callback to get indication of connection to iothub
        (void)IoTHubDeviceClient_SetConnectionStatusCallback(device_handle, connection_status_callback, NULL);
        
        // Set any option that are neccessary.
        // For available options please see the iothub_sdk_options.md documentation
        
        // Setting Log Tracing.
        bool traceOn = true;
        (void)IoTHubDeviceClient_SetOption(device_handle, OPTION_LOG_TRACE, &traceOn);
        
        // Setting the frequency of DoWork calls by the underlying process thread.
        // The value ms_delay is a delay between DoWork calls, in milliseconds.
        // ms_delay can only be between 1 and 100 milliseconds.
        // Without the SetOption, the delay defaults to 1 ms.
        tickcounter_ms_t ms_delay = 10;
        (void)IoTHubDeviceClient_SetOption(device_handle, OPTION_DO_WORK_FREQUENCY_IN_MS, &ms_delay);
        
        
#ifdef SET_TRUSTED_CERT_IN_SAMPLES
        // Setting the Trusted Certificate.  This is only necessary on system with without
        // built in certificate stores.
        (void)IoTHubDeviceClient_SetOption(device_handle, OPTION_TRUSTED_CERT, certificates);
#endif // SET_TRUSTED_CERT_IN_SAMPLES
        
#if defined SAMPLE_MQTT || defined SAMPLE_MQTT_OVER_WEBSOCKETS
        //Setting the auto URL Encoder (recommended for MQTT). Please use this option unless
        //you are URL Encoding inputs yourself.
        //ONLY valid for use with MQTT
        //bool urlEncodeOn = true;
        //(void)IoTHubDeviceClient_SetOption(device_handle, OPTION_AUTO_URL_ENCODE_DECODE, &urlEncodeOn);
#endif
        
    
        while(g_continueRunning) {
            sprintf(telemetry_msg_buffer, "{");

            int cnt = 0;
            for(int i = 0; i < n; i++) {
                rewind(fp[i]);
            
                char * line = NULL;
                size_t len = 0;
                ssize_t r;
        
                if((r = getline(&line, &len, fp[i])) != -1) {
                    if(line[r - 1] == '\n') line[r - 1] = '\0';
                    
                    int v = atoi(line);
                    
                    if(values[i] != v) {
                        values[i] = v;
                        if(cnt != 0) sprintf(telemetry_msg_buffer + strlen(telemetry_msg_buffer), ",");
                        sprintf(telemetry_msg_buffer + strlen(telemetry_msg_buffer), "\"%s\":%s", &files[i][5], line);
                        cnt++;
                    }
                }
                if(line) free(line);
            }
            sprintf(telemetry_msg_buffer + strlen(telemetry_msg_buffer), "}");
        
            message_handle = IoTHubMessage_CreateFromString(telemetry_msg_buffer);
        
            // Set Message property
            (void)IoTHubMessage_SetMessageId(message_handle, "MSG_ID");
            (void)IoTHubMessage_SetCorrelationId(message_handle, "CORE_ID");
            (void)IoTHubMessage_SetContentTypeSystemProperty(message_handle, "application%2fjson");
            (void)IoTHubMessage_SetContentEncodingSystemProperty(message_handle, "utf-8");
        
            // Add custom properties to message
            (void)IoTHubMessage_SetProperty(message_handle, "property_key", "property_value");
        
            (void)printf("\r\nSending message %d to IoTHub\r\nMessage: %s\r\n", (int)(messagecount + 1), telemetry_msg_buffer);
            IoTHubDeviceClient_SendEventAsync(device_handle, message_handle, send_confirm_callback, NULL);
        
            // The message is copied to the sdk so the we can destroy it
            IoTHubMessage_Destroy(message_handle);
            messagecount = messagecount + 1;
        
            ThreadAPI_Sleep(g_interval);
        }
  
        IoTHubDeviceClient_Destroy(device_handle);
    }

    IoTHub_Deinit();

    for(int i = 0; i < n; i++) {
        fclose(fp[i]);
    }
    
    free(fp);
    exit(EXIT_SUCCESS);
}
