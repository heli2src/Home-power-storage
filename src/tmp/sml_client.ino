/*
    
    run ethernet UDP-client connection to sml_server:
        - read out import power from smartmeter


    tx: '1-0:16.7.0'         - for import power, for other channels see sml-server
    rx: '12:59:49|1566.0*W'  - time and power

    tx: 'Bat:'               - sending Battery load/discharge data
                             - PAC;dayload;monthload;yearsload;sumload;'
                               -130;0,1;50;423;4330;0,0;132;787;15173

    Author:  Christian Jung
    Date:    15.04.2019

    timeout:    
                socket.cpp:socketSend    return -2
                socket.cpp:socketSendUDP return -2    

    todo:
        check with crc
        check time
    
 */
#include <SerialDebug.h>
 
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <utility/w5100.h>  
#include <Wire.h>

int sml_errorcnt;
bool sml_error_actual;

byte server[]= {192, 168, 178, 28};                      // Enter the IP address of the server you're connecting to:
int port=14550;

int sml_aestatus_old;
int sml_elstatus_old;

EthernetUDP Udp;                //Define UDP Object
char packetBuffer[UDP_TX_PACKET_MAX_SIZE];   //buffer to hold incoming packet,

char importBuffer[10];

void sml_client_setup(){
    Ethernet.init(CS_ETHERNET);    // Most Arduino shields    to configure the CS pin 
    Ethernet.begin(mac, ip);       // start the Ethernet connection:  
    delay(100);    

    W5100.setRetransmissionTime(0x07D0);    //see  http://forum.arduino.cc/index.php?topic=49401.0
    W5100.setRetransmissionCount(3);        //
    delay(1000); 
      
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        DEBUG("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
        lcd_error(1);
        while (true) 
            delay(1);     // do nothing, no point running without Ethernet hardware
    } else if (Ethernet.hardwareStatus() == EthernetW5100) {
        DEBUG("W5100 Ethernet controller detected.");    
    } else {
        DEBUG ("unknown Ethernet controller detected.");} 
    // give the Ethernet shield a second to initialize:
    DEBUG("W5100 Ethernet connecting...");
    Udp.begin(port);
    
    sml_errorcnt=0;
    sml_error_actual=false;
    sml_aestatus_old=-10;
    sml_elstatus_old=-10;
}

void sml_send_167(){
    Udp.beginPacket(server, port);
    Udp.write("1-0:16.7.0*");         //get import channel
    Udp.endPacket();
}

void sml_sendbat(int time_count){                  // send Battery load/discharge values
    char send_buffer[20]; 
    char str_temp[7];    
    if ((time_count%(30*1000/CEL_LOOP_TIME)==0) || (ae_status!=sml_aestatus_old) || (el_status!=sml_elstatus_old)){
        //DEBUG("sml_sendbat");      
        Udp.beginPacket(server, port);
        Udp.write("Bat:");      // key for logging start
        sprintf(send_buffer, "Solar=%4d;", solar);Udp.write(send_buffer,11);        
        sprintf(send_buffer, "P=%6d;", evu_import);Udp.write(send_buffer,9);
        sprintf(send_buffer, "Bat=%6d;", bat_target);Udp.write(send_buffer,11);
    
        sprintf(send_buffer, "ae=%3d;", ae_status);Udp.write(send_buffer,7);      
        sprintf(send_buffer, "Pnetz=%6d;", ae_Pnetz);Udp.write(send_buffer,13);
        sprintf(send_buffer, "Pbat=%6d;", ae_Pbat);Udp.write(send_buffer,12);
        dtostrf(ae_Vbat, 4, 1, str_temp);sprintf(send_buffer,"Vbat=%s;", str_temp);        // ae_Vbat   
        Udp.write(send_buffer,10);    
        dtostrf(ae_Ibat, 4, 1, str_temp);sprintf(send_buffer,"Ibat=%s;", str_temp);        // ae_Ibat
        Udp.write(send_buffer,10);       
        sprintf(send_buffer, "temp=%2d;", ae_Temp);Udp.write(send_buffer,8);               // ae_Temp    
        dtostrf(ae_Energy, 5, 0, str_temp);sprintf(send_buffer,"ae_Wh=%s;", str_temp);     // ae_Energy
        Udp.write(send_buffer,12); 
                    
        sprintf(send_buffer, "el_stat=%3d;", el_status);    // el_status
        Udp.write(send_buffer,12);
        sprintf(send_buffer, "temp=%3d;", el_temp);         // el Temperatur
        Udp.write(send_buffer,9);
        sprintf(send_buffer, "pout=%4d;", el_pout);         // el_pout
        Udp.write(send_buffer,10);    
        dtostrf(float(el_target_vout*0.01), 4, 1, str_temp);
        sprintf(send_buffer,"ta_vout=%s;", str_temp);          // target vout
        Udp.write(send_buffer,13);
        dtostrf(float(el_vout*0.01), 4, 1, str_temp);
        sprintf(send_buffer,"vout=%s; ", str_temp);            // vout
        Udp.write(send_buffer,10);
        dtostrf(float(el_target_iout*0.1), 4, 1, str_temp);
        sprintf(send_buffer,"ta_iout=%s;", str_temp);          //target iout
        Udp.write(send_buffer,13);    
        dtostrf(float(el_iout*0.1), 4, 1, str_temp);
        sprintf(send_buffer,"iout=%s;", str_temp);            // iout
        Udp.write(send_buffer,10);                    
        dtostrf((el_ws/3600), 6, 1, str_temp);                // loading Energy in Wh
        sprintf(send_buffer,"el_Wh=%s;", str_temp);       
        Udp.write(send_buffer,15);             

                                
        Udp.endPacket();
        sml_aestatus_old=ae_status;
        sml_elstatus_old=el_status;
        // DEBUG("sml_sendbat end");            
    }
}

int sml_receive(){                                                 // get time and power e.q. '12:59:49|1566.0*W' 
    int result=0;    
    int packetSize = Udp.parsePacket();               
    if (Udp.available()) {
        //DEBUG("packetSize=",packetSize);         
        if (packetSize>UDP_TX_PACKET_MAX_SIZE) sml_error_actual=true;
        else if (packetSize>0) {       
            Udp.read(packetBuffer,packetSize);
            int i,start,end;
            for(i=0;i<packetSize;i++){                                
                if (packetBuffer[i]=='|') start=i+1;
                if (packetBuffer[i]=='*') end=i;    
                if (i<8) sml_time[i]=packetBuffer[i];
            }
            if ((end-start>10)or (start==0) or (end==0) or (end<start)){ 
                sml_error_actual=true;
            }else{
                for (i=start;i<end;i++){
                   importBuffer[i-start]=packetBuffer[i];
                }
                importBuffer[i]=0; 
                result=(int)(atof(importBuffer)+.5);
                //Serial.println(result);
                sml_error_actual=false;
                }
        } 
    }else sml_error_actual=true;
    return result;  
}
