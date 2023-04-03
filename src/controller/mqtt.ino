
#include <SerialDebug.h>
#include <Ethernet.h>
#include <PubSubClient.h>
// #include <ArduinoJson.h>       //https://arduinojson.org/

#define CS_ETHERNET 10          // Pin CS for Ethernet Card


byte mac[] = { 0xC0, 0x01, 0xC0, 0x1A, 0xBA, 0xBE};      // MAC address and IP address for your controller below.
byte ip[] = {192, 168, 178, 72};                         // The IP address will be dependent on your local network:

#define MQTT_LOOP_TIME  100         // get 100ms to read the mqtt subscribe messages

EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

unsigned long sma_online;         // time from last message sma
unsigned long evu_online;         // time from last message evu


int payload2int( byte* payload, unsigned int plength){
    payload[plength] = '\0';                    // Make payload a string by NULL terminating it.
    int val = atoi((char *)payload);
    return val;     
}

void callback(char* topic, byte* payload, unsigned int plength) {
  String myload = String((char *)payload);
  if (!strcmp(topic, PSOLAR)){
      solar = payload2int(payload, plength);
      sma_online = millis();
      //DEBUG(topic,solar);   
  }else if (!strcmp(topic, EVUIMPORT)){
      evu_import = payload2int(payload, plength);
      evu_online = millis();
      //DEBUG("evu_import", topic, evu_import);
  }else if (!strcmp(topic, STIME)){
      payload[plength] = '\0';
      utc = atol(payload) % 86400;    // stime in total seconds per day (utc time)
      //DEBUG(topic,utc);
  }else if (!strcmp(topic, WALLBOX)){
      wallbox = payload2int(payload, plength);   
  }
}


void mqtt_setup(){
    Ethernet.init(CS_ETHERNET);        // Most Arduino shields    to configure the CS pin 
    Ethernet.begin(mac, ip);       // start the Ethernet connection:   
    delay(1000); 
      
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        DEBUG("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
//        lcd_error(1);
        while (true) 
            delay(1);     // do nothing, no point running without Ethernet hardware
    } else if (Ethernet.hardwareStatus() == EthernetW5100) {
        DEBUG("W5100 Ethernet controller detected.");    
    } else {
        DEBUG ("unknown Ethernet controller detected.");
    } 
    // give the Ethernet shield a second to initialize:
    DEBUG("W5100 Ethernet connecting...");

    mqttClient.setServer("192.168.178.28", 1883);
    mqttClient.setCallback(callback);
    sma_online = 0;
    evu_online = 0;
}




void mqtt_loop(){
//    byte outmsg[]={0xff,0xfe};
    unsigned long mqtt_loop=millis();
    while ((millis()-mqtt_loop)<MQTT_LOOP_TIME){    
        if (not mqttClient.loop()){     //call loop  
            DEBUG("mqtt lost connection", mqttClient.state() );
            if (mqttClient.state() != 0) {
                mqttClient.connect("Battery", "Battery/online", 0, true, "offline");   
                DEBUG("mqtt State after new connect", mqttClient.state() );
                mqttClient.subscribe(PSOLAR);
                mqttClient.subscribe(STIME);
                mqttClient.subscribe(EVUIMPORT);
                mqttClient.subscribe(WALLBOX);
                //mqttClient.subscribe("Smartmeter/Time");
                //mqttClient.subscribe("Smartmeter/Date");
            }else{
                DEBUG("mqtt error, state==0 ");   
            }
        }
    }
    if (time_count%(2000/CEL_LOOP_TIME) == 0){
        char tbuffer[20]; 
        mqttClient.publish("Battery/online","online", true);
        sprintf(tbuffer, "%5d", bat_target);    
        mqttClient.publish("Battery/powertarget", tbuffer); 
        sprintf(tbuffer, "%5d", bat_actual);        
        mqttClient.publish("Battery/power", tbuffer);
        sprintf(tbuffer, "%6d", round(el_ws/3600));            
        mqttClient.publish("Battery/store", tbuffer);
        sprintf(tbuffer, "%6d", round(ae_Energy));         
        mqttClient.publish("Battery/deliver", tbuffer);
        sprintf(tbuffer, "%6d", el_vout);         
        mqttClient.publish("Battery/stored_vbat", tbuffer); 
        sprintf(tbuffer, "%6d", round(ae_Vbat*100));          
        mqttClient.publish("Battery/deliver_Vbat", tbuffer);
        sprintf(tbuffer, "%3d", ae_status);        
        mqttClient.publish("Battery/deliver_state", tbuffer);
        sprintf(tbuffer, "%3d", el_status);            
        mqttClient.publish("Battery/store_state", tbuffer);     
    }
    if ((millis() - sma_online) > 20000) {
        solar = -1;
    }
    if ((millis() - evu_online) > 20000) 
        evu_error = true;
    else 
        evu_error = false;    
}
