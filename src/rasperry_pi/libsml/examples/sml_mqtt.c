// Copyright 2011 Juri Glass, Mathias Runge, Nadim El Sayed
// DAI-Labor, TU-Berlin
//
// This file is part of libSML.
// Thanks to Thomas Binder and Axel (tuxedo) for providing code how to
// print OBIS data (see transport_receiver()).
// https://community.openhab.org/t/using-a-power-meter-sml-with-openhab/21923
// SML Protokoll see: https://wiki.volkszaehler.org/software/obis
//
// add-on:   write obis-file and mqtt-client			Author: C. Jung	30.8.2020
//           https://www.eclipse.org/paho/files/mqttdoc/MQTTClient/html/index.html
// 			 https://github.com/eclipse/paho.mqtt.c
// puplish values as Smartmeter   -> to check in mosquitto:   subscribe Smartmeter/#
//
//  File:                                           Comment                                   Puplish:
//  129-129:199.130.3*255(EMH)
//  1-0:0.0.9*255(06 48 4d 58 71 84 c5 6a e8 3f )	Geräteeinzelidentifikation
//  1-0:1.8.0*255(4876.8*kWh)						Zählerstand Bezug						-> mqtt: Smartmeter/1-0:1.8.0   4876.8*kWh
//  1-0:2.8.0*255(17274.1*kWh)						Zählerstand Lieferung					-> mqtt: Smartmeter/1-0:2.8.0   17274.1*kWh
//  1-0:1.8.1*255(4875.8*kWh)						Zählerstand Bezug Tarif 1				-> mqtt: Smartmeter/1-0:1.8.1    4875.8*kWh
//  1-0:2.8.1*255(17273.1*kWh)						Zählerstand Lieferung Tarif 1			-> mqtt: Smartmeter/1-0:2.8.1    17273.1*kWh
//  1-0:1.8.2*255(1.0*kWh)							Zählerstand Bezug Tarif 2 				-> mqtt: Smartmeter/1-0:1.8.2    1.0*kWh
//  1-0:2.8.2*255(1.0*kWh)							Zählerstand LieferungTarif 2			-> mqtt: Smartmeter/1-0:2.8.2    1.0*kWh
//  1-0:16.7.0*255(3.6*W)							Leistung (Momentan)					    -> mqtt: Smartmeter/1-0:16.7.0   3.6*W
//  1-0:21.7.0*255(3.6*W)
//  129-129:199.130.5*255(df0 95 01 0c 88 94 30 c2 21 e1 7a 35 d1 c0 0e 47 c7 8c 87 8c 2b b0 f5 18 e3 99 d2 87 0c 60 c4 18 6c
//  60 38 39 e3 6a c6 fb 87 5f 0b a9 3a 00 99 )				Public Key 
//  																						   mqtt: Smartmeter/Date 	20.02.2020
//  																						   mqtt: Smartmeter/Time 	20:23:05
//
//
// libSML is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// libSML is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with libSML.  If not, see <http://www.gnu.org/licenses/>.


#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <errno.h>
#include <termios.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <sys/ioctl.h>
#include <time.h>

#include <sml/sml_file.h>
#include <sml/sml_transport.h>
#include <sml/sml_value.h>

#include "MQTTClient.h"
#include "unit.h"

#define ADDRESS     "tcp://localhost:1883"			// IP from Broker 
#define CLIENTID 	"tcp://localhost"				// Broker and Client are on the same host
#define QOS         0
#define TIMEOUT     500L							//timeout after 500ms

// globals
int sflag = false; // flag to process only a single OBIS data stream
int vflag = false; // verbose flag


int serial_port_open(const char* device) {
	int bits;
	struct termios config;
	memset(&config, 0, sizeof(config));

	if (!strcmp(device, "-"))
		return 0; // read stdin when "-" is given for the device

#ifdef O_NONBLOCK
	int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
#else
	int fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
#endif
	if (fd < 0) {
		fprintf(stderr, "error: open(%s): %s\n", device, strerror(errno));
		return -1;
	}

	// set RTS
	ioctl(fd, TIOCMGET, &bits);
	bits |= TIOCM_RTS;
	ioctl(fd, TIOCMSET, &bits);

	tcgetattr(fd, &config);

	// set 8-N-1
	config.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR
			| ICRNL | IXON);
	config.c_oflag &= ~OPOST;
	config.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	config.c_cflag &= ~(CSIZE | PARENB | PARODD | CSTOPB);
	config.c_cflag |= CS8;

	// set speed to 9600 baud
	cfsetispeed(&config, B9600);
	cfsetospeed(&config, B9600);

	tcsetattr(fd, TCSANOW, &config);
	return fd;
}

void transport_receiver(unsigned char *buffer, size_t buffer_len) {
	int i;
	char mqtt_topic [30];
	char mqtt_value [20];
	// the buffer contains the whole message, with transport escape sequences.
	// these escape sequences are stripped here.
	sml_file *file = sml_file_parse(buffer + 8, buffer_len - 16);
	// the sml file is parsed now

	// this prints some information about the file
	if (vflag)
		sml_file_print(file);

	// read here some values ...
	if (vflag)
		printf("OBIS data\n");
	FILE *f = fopen("/mnt/RAMDisk/sml_obis.txt", "w");	

	
	// create MQTT client
	MQTTClient client;
	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	MQTTClient_message pubmsg = MQTTClient_message_initializer;
	MQTTClient_deliveryToken token;	
    MQTTClient_create(&client, ADDRESS, CLIENTID,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;	
    int rc;	
	if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to mqtt-broker connect, return code %d\n", rc);
        //exit(EXIT_FAILURE);
    }	
	pubmsg.qos = QOS;
	pubmsg.retained = 0;	
	// mqqt public System/Date and System/Time	
	time_t t;
	struct tm * ts;
	t = time(NULL);
	ts = localtime(&t);
	sprintf(mqtt_value,"%2d.%02d.%d",ts->tm_mday,ts->tm_mon+1,ts->tm_year+1900);
	pubmsg.payload = mqtt_value;
	pubmsg.payloadlen = (int)strlen(mqtt_value);
	MQTTClient_publishMessage(client, "Smartmeter/Date", &pubmsg, &token);	
	MQTTClient_waitForCompletion(client, token, TIMEOUT);
	
	sprintf(mqtt_value,"%2d:%02d:%02d",ts->tm_hour,ts->tm_min,ts->tm_sec);
	pubmsg.payload = mqtt_value;
	pubmsg.payloadlen = (int)strlen(mqtt_value);
	MQTTClient_publishMessage(client, "Smartmeter/Time", &pubmsg, &token);	
	MQTTClient_waitForCompletion(client, token, TIMEOUT);
	
	for (i = 0; i < file->messages_len; i++) {
		sml_message *message = file->messages[i];
		if (*message->message_body->tag == SML_MESSAGE_GET_LIST_RESPONSE) {
			sml_list *entry;
			sml_get_list_response *body;
			body = (sml_get_list_response *) message->message_body->data;
			for (entry = body->val_list; entry != NULL; entry = entry->next) {
				if (!entry->value) { // do not crash on null value
					fprintf(stderr, "Error in data stream. entry->value should not be NULL. Skipping this.\n");
					continue;
				}
				if (entry->value->type == SML_TYPE_OCTET_STRING) {
					char *str;
					fprintf(f,"%d-%d:%d.%d.%d*%d(%s)\n",
						entry->obj_name->str[0], entry->obj_name->str[1],
						entry->obj_name->str[2], entry->obj_name->str[3],
						entry->obj_name->str[4], entry->obj_name->str[5],
						sml_value_to_strhex(entry->value, &str, true));
					free(str);
				} else if (entry->value->type == SML_TYPE_BOOLEAN) {
					fprintf(f,"%d-%d:%d.%d.%d*%d#%s#\n",
						entry->obj_name->str[0], entry->obj_name->str[1],
						entry->obj_name->str[2], entry->obj_name->str[3],
						entry->obj_name->str[4], entry->obj_name->str[5],
						entry->value->data.boolean ? "true" : "false");
				} else if (((entry->value->type & SML_TYPE_FIELD) == SML_TYPE_INTEGER) ||
						((entry->value->type & SML_TYPE_FIELD) == SML_TYPE_UNSIGNED)) {
					double value = sml_value_to_double(entry->value);
					int scaler = (entry->scaler) ? *entry->scaler : 0;
					int prec = -scaler;
					if (prec < 0)
						prec = 0;
					value = value * pow(10, scaler);
					char *unit = "";
					char compare[] = "Wh";					
					if (entry->unit &&  // do not crash on null (unit is optional)
						(unit = dlms_get_unit((unsigned char) *entry->unit)) != NULL){
						if (strcmp(unit, compare)==0) {
							value=value/1000;
							unit="kWh";
						}				
					}
					
					fprintf(f,"%d-%d:%d.%d.%d*%d(%.*f*",									//write OBIS-Kennzahl to file
						entry->obj_name->str[0], entry->obj_name->str[1],
						entry->obj_name->str[2], entry->obj_name->str[3],
						entry->obj_name->str[4], entry->obj_name->str[5], prec, value);
					fprintf(f,"%s)\n", unit);	
					
					sprintf(mqtt_topic,"Smartmeter/%d-%d:%d.%d.%d",							//generate OBIS-Kennzahl for mqtt-client
						entry->obj_name->str[0], entry->obj_name->str[1],
						entry->obj_name->str[2], entry->obj_name->str[3],
						entry->obj_name->str[4]);
						
					if (entry->obj_name->str[2]==16) {										// copy 16 to 21 and write it to file,
						fprintf(f,"1-0:21.7.0*255(%.*f*",prec,value);						//      necessary for SolarView@Fritz!Box
						fprintf(f,"%s)\n", unit);					
					}						
					// flush the stdout puffer, that pipes work without waiting
					fflush(stdout);
					
					sprintf(mqtt_value,"%.*f*%s",prec,value,unit);							// generate the value from the OBIS-Kennzahl
					pubmsg.payload = mqtt_value;
					pubmsg.payloadlen = (int)strlen(mqtt_value);					
					MQTTClient_publishMessage(client, mqtt_topic, &pubmsg, &token);	
					
					//printf("\nWaiting for up to %d seconds for publication of %s\n"
					//		"on topic %s for client with ClientID: %s\n",
					//		(int)(TIMEOUT/1000), mqtt_value, mqtt_topic, CLIENTID);
					MQTTClient_waitForCompletion(client, token, TIMEOUT);
					
				}
			}
			if (sflag)
				exit(0); // processed first message - exit
		}
	}
	// free the malloc'd memory
	sml_file_free(file);
	fclose(f);
	MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);	
}

int main(int argc, char *argv[]) {
	// this example assumes that a EDL21 meter sending SML messages via a
	// serial device. Adjust as needed.
	int c;

	while ((c = getopt(argc, argv, "+hsv")) != -1) {
		switch (c) {
		case 'h':
			printf("usage: %s [-h] [-s] [-v] device\n", argv[0]);
			printf("device - serial device of connected power meter e.g. /dev/cu.usbserial, or - for stdin\n");
			printf("-h - help\n");
			printf("-s - process only one OBIS data stream (single)\n");
			printf("-v - verbose\n");
			exit(0); // exit here
			break;
		case 's':
			sflag = true;
			break;
		case 'v':
			vflag = true;
			break;
		case '?':
			// get a not specified switch, error message is printed by getopt()
			printf("Use %s -h for help.\n", argv[0]);
			exit(1); // exit here
			break;
		default:
			break;
		}
	}

	if (argc - optind != 1) {
		printf("error: Arguments mismatch.\nUse %s -h for help.\n", argv[0]);
		exit(1); // exit here
	}

	// open serial port
	int fd = serial_port_open(argv[optind]);
	if (fd < 0) {
		// error message is printed by serial_port_open()
		exit(1);
	}

	// listen on the serial device, this call is blocking.
	printf("Start sml_server \n");	
	sml_transport_listen(fd, &transport_receiver);
	close(fd);
	
	printf("close sml_server \n");		

	return 0;
}
