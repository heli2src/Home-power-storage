/*
                              connect via Serial1 -> rs485 to AEConversion INV500-90EU RS485
                                            - switch off MPP-Tracking, set min Voltage AE_VMIN and current=0
                                            - wait AE_CWaitTime
                                            - set Powerout:  ae_set_power(value in W)

        Used Pins:
                     Pin 3  ->   AE_ENABLE_PIN   -> read_n/write for RS485 DE/RE = Data enable/Readout enable not Pin 2/3
                     Pin 18 ->   Serial1 TX      -> connect with RS485 DI Pin 1  = Data in
                     Pin 19 ->   Serial1 RX      -> connect with RS485 RO Pin 4  = Data Read out



        ae_status= -2     : Bat Voltage < AE_VMIN
                   -1     : not connected
                    0     : not used....
                    1     : received type, disable MPP Tracking
                    2/12  : measure
                    3/13  : check limits                                 after AE_CWaitTime goto 14
                    4     : set current to 0 next state=2
                    14    : set current : next state = 12


        to do:
            AECONVERSIon braucht 45mA, auch wenn Strom auf 0 gestellt ist  --> Relais einbauen? 
              dann braucht aber das wiedereinschalten sehr lange, 
              sinnvoll wäre ein Relais also nur wenn die Battery geladen wird. -> der gesparte Stromverbrauch ist zu gering
            connect/disconnect AE from Battery (enable-signal to bms), see ae_disable()
            ae_error_client /(count from timeout) ausgeben

aktuelle Messdaten abfragen:
#<ID>0<CR> Answer: <LF>*<ID>0___0_45.7_1.23____55_230.1_0.21____50_45___635_z>CR> (Example)
der Reihenfolge nach:
0 Status
45.7 Eingangsspannung V (oft sehr ungenau!)
1.23 Eingangsstrom A
55 Eingangsleistung W
230.1 Netzspannung V
0.21 Ausgangsstrom A
50 Ausgangsleistung W
45 Gerätetemperatur °C
635 Tagesenergie Wh
*/
//23 33 32 39 20 35 30 30 2D 39 30 20 33 00 00 00 00 00
//#  3   2  9    5   0  0  - 9  0     3   
#define SERIAL_DEBUG_SEPARATOR   " "
#include <SerialDebug.h>
#include <SPI.h>

const int ae_id PROGMEM  = 32;                // id from aeconversion rs485-bus

char ae_buffer[AE_BUFFER_SIZE];

String ae_rxbuffer = "";
bool ae_newRX;
int ae_hstatus;                              // hardware status

int ae_error_client;
int ae_error_buffer;
int ae_error_cnt;

int ae_Temp;
float ae_Vnetz,ae_Inetz;
unsigned long ae_WaitTime;


void ae_disable() {                         // Battery voltage < AE_VMIN
     ae_status=-2; 
     digitalWrite(AE_DISCHARGE_PIN, LOW);        // diable discharge, disconnect AE from Battery 
}


void ae_serialEvent1() {
  //DEBUG("ae_serialEvent1");
  while (Serial1.available()>0) {
    char inChar = (char)Serial1.read();
    //Serial.print(inChar,HEX);
    //if ((ae_rxbuffer.length()<AE_BUFFER_SIZE) and (inChar!='\n')    
    if ((ae_rxbuffer.length()<AE_BUFFER_SIZE) and (inChar>31) and (inChar<128))
            ae_rxbuffer += inChar;
    if (inChar == '\r')  {       //or (inChar == '\n'))
        //ae_rxbuffer += '\0';
        ae_newRX = true;
    }
  } 
}

void ae_receive(void) {
    unsigned long timeout=millis();  
    ae_rxbuffer = "";
    ae_newRX=false;
    while (not ae_newRX) {
        ae_serialEvent1();
        if (millis()-timeout> AE_TIMEOUT){
            if (ae_rxbuffer.length()==0){
                ae_error_client++;
                DEBUG("aeconversion.ae_receive() timeout ", "counter= ",ae_error_client);
                return false;
            } else{
              DEBUG("aeconversion.ae_receive() timeout but ae_rxbuffer.length =",ae_rxbuffer.length());
              ae_newRX= true;
            }            
        }
    }
}

void ae_send() {
    digitalWrite(AE_ENABLE_PIN, HIGH);      // TX
    delay(5);                               // wait 1ms
    DEBUG("ae_send:", ae_buffer);    
    Serial1.print(ae_buffer);
    Serial1.flush();    
    digitalWrite(AE_ENABLE_PIN, LOW);       // RX
}


bool ae_query() {                 // send ae_buffer and receive ae_rxbuffer, if no data receive after timeout -> result=false
    ae_rxbuffer="";
    ae_serialEvent1();                      // clear RX_Buffer
    DEBUG("ae_query discard:", ae_rxbuffer);
    digitalWrite(AE_ENABLE_PIN, HIGH);      // TX
    delay(5);                               // wait 1ms
    DEBUG("ae_query send:", ae_buffer);    
    Serial1.print(ae_buffer);
    Serial1.flush();    
    digitalWrite(AE_ENABLE_PIN, LOW);       // RX
    ae_receive();
    ae_receive();
//    unsigned long timeout=millis();
//    ae_rxbuffer = "";
//    ae_newRX=false;
//    while (not ae_newRX) {
//        ae_serialEvent1();
//        if (millis()-timeout> AE_TIMEOUT){
//            if (ae_rxbuffer.length()==0){
//                ae_error_client++;
//                DEBUG("aeconversion.ae_query() timeout ", "counter= ",ae_error_client);
//                return false;
//            } else{
//              DEBUG("aeconversion.ae_query() timeout but ae_rxbuffer.length =",ae_rxbuffer.length());
//              ae_newRX= true;
//            }            
//        }
//    }
    ae_error_client=0;
    //ae_newRX=false;
    DEBUG("ae_query:  receive RS485: ", ae_rxbuffer, "lenght =",ae_rxbuffer.length());
    return true;
}

void ae_measure(){
    sprintf(ae_buffer, "#%d0\r",ae_id);         // measurements : status V I-in  P-in V-out  I-out  P-out T   WH
    ae_Vbat=-1;
    ae_hstatus=-1;
    ae_Ibat=-1;
    ae_Pbat=-1;
    ae_Vnetz=-1;
    ae_Pnetz=-1;
    ae_Temp=-1;
    ae_Energy=-1;   
    if (ae_query() and (ae_rxbuffer.length()>=50)) {
        ae_hstatus=ae_rxbuffer.substring(6,8).toInt();
        ae_Vbat=ae_rxbuffer.substring(9,14).toFloat();
        ae_Ibat=ae_rxbuffer.substring(15,20).toFloat();
        ae_Pbat=ae_rxbuffer.substring(21,26).toInt();
        ae_Vnetz=ae_rxbuffer.substring(27,32).toFloat();
        ae_Inetz=ae_rxbuffer.substring(33,38).toFloat();
        ae_Pnetz=ae_rxbuffer.substring(39,44).toInt();
        ae_Temp=ae_rxbuffer.substring(45,48).toInt();
        if (ae_rxbuffer.length()>=56) {
            ae_Energy=ae_rxbuffer.substring(49,55).toInt();
        }
        DEBUG("AE Vbat: ",ae_Vbat, "Ibat: ",ae_Ibat,"Pbat: ",ae_Pbat,"Temp: ",ae_Temp);
        DEBUG("AE Vnetz: ",ae_Vnetz,"Inetz: ",ae_Inetz, "Pnetz: ",ae_Pnetz,"Energy: ", ae_Energy);
        ae_status++;
    }
    else {
        ae_error_cnt++;
        DEBUG("ae_measure not correct, errorcnt=",ae_error_cnt, ": get",ae_rxbuffer, "length=",ae_rxbuffer.length());
        if (ae_error_cnt > AE_ERROR_MAX){
            DEBUG("ae_measure errocnt reached, set ae_status=-1");        
            ae_status=-1;
        }
    }
}

void ae_mode_connect(){                                // connect to AEConversion 500-90: set ae_status -1 = not connected
                                                       //                                               1 = found solar inverter typ
                                                       //                                               2 = set current mode
    sprintf(ae_buffer, "#%d9\r", ae_id);               // query solar inverter typ, answer should be '*329 500-90 3\r'
    //digitalWrite(AE_DISCHARGE_PIN, HIGH);        // enable discharge    
    if (ae_query()){ 
        if (ae_rxbuffer.indexOf(AE_TYP) != -1){           
            ae_status=1;
            ae_error_cnt=0;          
            DEBUG("read: ",ae_rxbuffer, "found:", AE_TYP, "set Status=",ae_status);
        }else{
            ae_status=-1;
            DEBUG("failed  Typ ",AE_TYP, "not found, set Status=-1, found:",ae_rxbuffer);
        }
    }
    else {
        DEBUG("ae_mode_connect, no answer, Status=-1",ae_rxbuffer);
        ae_status=-1;
    }
    static char outstr[5];
    dtostrf(AE_VMIN,2, 1, outstr);
    sprintf(ae_buffer, "#%dB_2_%s\r",ae_id,outstr);      // disable MPP Tracking  #<ID>B_m_uu.u<CR>, m=0(->MPP-Modus, =2->Spannungs- und Stromvorgabe)
    if (ae_status==1 and ae_query()){                    // answer  <LF>*<ID>B m uu.u<CR>
        DEBUG("disable MPP Tracking ok");
        DEBUG("now send: ",ae_buffer);
        sprintf(ae_buffer, "#%dB 2 %s",ae_id,outstr);
        if (ae_rxbuffer.compareTo(ae_buffer) == 7) {
            DEBUG("send ae_buffer", ae_buffer);
            sprintf(ae_buffer,"#%dS_00.0\r",ae_id);                              // set current to 0
            DEBUG("ae_mode_connect: set current ", ae_buffer);
            if (ae_query() and ae_rxbuffer.compareTo(ae_buffer)==7) {
                ae_status=2;
                DEBUG("set current to 0=ok, status=",ae_status);
                ae_WaitTime=millis();
            }else {
                ae_status=-1;
ae_status=2;
                DEBUG("ae_mode_connect: wrong answer from set current =", ae_rxbuffer,"should be:",ae_buffer,"set Status=", ae_status);
            }
        }else {                                                  // wrong answer
            ae_status=-1;
ae_status=2;            
            DEBUG("ae_mode_connect: wrong answer", ae_rxbuffer, "should be:", ae_buffer,"set Status=", ae_status);            
        }
    }
    else if (ae_status==1) {                                     // no connection
        DEBUG("ae_mode_connect: no connection, read=", ae_buffer," set Status=-1", );
        ae_status=-1;
    }
}

bool ae_setcurrent_to0(){
    sprintf(ae_buffer,"#%dS_00.0\r",ae_id); 
    return(ae_query());     
}


bool ae_set_powerbasic(int power){
boolean result; 
    sprintf(ae_buffer, "#%d9\r", ae_id);
    ae_send();
    static char outstr[5];
    dtostrf(AE_VMIN,2, 1, outstr);
    sprintf(ae_buffer, "#%dB_2_%s\r",ae_id,outstr);      // disable MPP Tracking  #<ID>B_m_uu.u<CR>, m=0(->MPP-Modus, =2->Spannungs- und Stromvorgabe)
    ae_send(); 
    float current=float(power)/54.0 /AE_ETA;             // calculate current
    if (current>=AE_MAXCURRENT)
        current=AE_MAXCURRENT;                              // current to high -> set to maximum
    else if (current<0)
        current=0;
    dtostrf(current,2, 1, outstr);
    if (current>=0 and current<10.0)
       sprintf(ae_buffer,"#%dS_0%s\r",ae_id,outstr);       // set current
    else 
       sprintf(ae_buffer,"#%dS_%s\r",ae_id,outstr);       
    ae_send();
}

bool ae_set_power(int power){
boolean result; 
int temp; 
    //DEBUG("set_power begin: ae_status =",ae_status);
    result=true;
//    digitalWrite(AE_DISCHARGE_PIN, HIGH);                  // enable discharge, connect AE to Battery 
    if (ae_status<-1) {                                  // something is wrong, or battery is low
         power=0;
//         temp=ae_status;
         ae_mode_connect();                              // try to connect and set current to 0      
         ae_setcurrent_to0();  
         ae_Ibat=0.0;          
         ae_measure();                          // if connect ok, than ae_status=3
         if (ae_Vbat<(AE_VMIN+AE_VMIN_HYST)) {
             DEBUG("aeconversion: ae_Vbat<=AE_VMIN+AE_VMIN_HYST ->disabled",ae_Vbat," <",(AE_VMIN+AE_VMIN_HYST));
             ae_error_cnt++;
             if (ae_error_cnt > AE_ERROR_MAX)
                 ae_status=-2; 
         }
//         ae_status=temp;
    }
    if ((ae_status==-1) or (ae_status==0)) ae_mode_connect();
    if ((ae_status==2) or (ae_status==12)) {                 //Status=measure
        ae_measure();                                        //measure and if ok inc status(goto check), else status=0
    }
    if ((ae_status==3)or (ae_status==13)) {                  //Status=check limits
        if (ae_Vbat<=AE_VMIN) {                              // Battery voltage ok?
            ae_status--;                                     // measure again
            ae_error_cnt++;
            DEBUG("aeconversion: ae_Vbat<=AE_VMIN, error_cnt=", ae_error_cnt);            
            if (ae_error_cnt > AE_ERROR_MAX){   
                DEBUG("aeconversion: ae_Vbat<=AE_VMIN !! ->switch off ",ae_Vbat);
                ae_disable();
                ae_status=-2;
                result=false;
                power=0; 
            }           
 //       }else if ((ae_Vbat<=AE_VMAX)){             
        }else if ((ae_Vnetz>AE_VNETZ_MIN and ae_Vnetz<AE_VNETZ_MAX)and (ae_Vbat<=AE_VMAX)){   // check some values
            if ((ae_status==3) and ((ae_WaitTime-millis())>AE_CWaitTime))
                ae_status+=11;                                                                // finish wait time after start
            else if (ae_status==13){
                if ((ae_WaitTime-millis())>AE_CWaitTime)
                    ae_status++;                                                              // next state: set new current
                else
                    ae_status--;                                          // wait ..
            }else if (ae_setcurrent_to0())
                ae_status--;                                              // next state: measure again until AE_CWwaitTime
            else {
                DEBUG("ae_set_power: Vnetz and Vbat ok, but anything else is wrong, inc ae_error_cnt");
                ae_error_cnt++;
                if (ae_error_cnt > AE_ERROR_MAX){
                    ae_status=-1;                                              // error
                    DEBUG("ae_set_power: ae_error_cnt > AE_ERROR_MAX -> set status=-1  ",ae_error_cnt);
                }
            }
        }else{                      
            ae_error_cnt++; 
            ae_status--;                                     // measure again             
            DEBUG("ae_set_power: something is wrong: measure again errorcnt=", ae_error_cnt," Vnetz=", ae_Vnetz, "Vbat=", ae_Vbat);   
            if (ae_error_cnt > AE_ERROR_MAX){
                ae_setcurrent_to0();                                         // something is wrong -> set current to 0              
                if (ae_status!=-2) ae_status=-1;
                DEBUG("ae_set_power: ae_error_cnt (=", ae_error_cnt, ") > AE_ERROR_MAX -> set status=  ",ae_status);
            }
        }
    }
    if ((ae_status==4)or (ae_status==14)) {               // status==4 --> set current to 0
/*        if (ae_Pbat>15)                                 // status==14 --> set current to power
            ae_Eta=float(ae_Pnetz)/float(ae_Pbat);
        else
            ae_Eta=1.0;*/
        float current=float(power)/ae_Vbat /AE_ETA;             // calculate current
        if (current>=AE_MAXCURRENT)
            current=AE_MAXCURRENT;                              // current to high -> set to maximum
        else if (current<0)
            current=0;
        if (ae_status==4) current=0;
        static char outstr[5];
        dtostrf(current,2, 1, outstr);
        if (current>=0 and current<10.0)
            sprintf(ae_buffer,"#%dS_0%s\r",ae_id,outstr);       // set current
        else 
            sprintf(ae_buffer,"#%dS_%s\r",ae_id,outstr);
        if (ae_query() and ae_rxbuffer.compareTo(ae_buffer)==7) {
            ae_status-=2;                                           // next state : measure again
            ae_error_cnt = 0;
        }else { 
            DEBUG("ae_set_power: current not ok, but continue, buffer=",ae_buffer," ae_rxbuffer=",ae_rxbuffer);
           //ae_status=-1; 
           ae_status-=2;
        }
        ae_WaitTime=millis();
    }
    //DEBUG("set_power end: ae_status =",ae_status);
    return(result);
}


void ae_setup()  {
    pinMode(AE_DISCHARGE_PIN, OUTPUT);
    ae_disable();
    Serial1.begin(9600);
    ae_rxbuffer.reserve(AE_BUFFER_SIZE);
    ae_error_client = 0;
    ae_error_buffer = 0;
    int ae_error_cnt;
    ae_status=-1;
    ae_newRX = false;
    pinMode(AE_ENABLE_PIN, OUTPUT);
    digitalWrite(AE_ENABLE_PIN, LOW);       // set to read ->RX
}
