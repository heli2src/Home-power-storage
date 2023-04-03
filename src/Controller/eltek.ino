// from rectifier : (requests for logins)
//    05014400 + ser nr + 00 00 from rectifier  : HELLO where are you ! rectifier sends 05014400 and 6 bytes serial number and followed by 00 00 (login request)
//    0500xxyy + 1B + ser nr + 00 is send during normal voltage every second. xxyy is the last 2 bytes from the serial number
//    after either of these send 05004804 every 5 seconds ! to keep logged in. rectifier does not send login requests so after 10 second the numbers stop until 05014400 is sent
// 
// from rectifier : (status messages)
//    0501400C + status data : walkin and below 43 volts (error) and during walk-out (input voltage low)
//    05014010 + status data : walkin busy 
//    05014004 + status data : normal voltage reached
//    05014008 + status data : current-limiting active
//         with status data=# 01 02 03 04 05 06 07 08 where 
//                          # 01 is the intake temperature in Celcius
//                          # 02 03 (LSB/MSB) output current in deci amps (* 0.1 Amp)
//                          # 04 05 (LSB/MSB) output voltage Low byte in centivolts (* 0.01 Volt)
//                          # 06 07 (LSB/MSB) line input voltage in AC volts
//                          # 08 is the output temperature in Celcius ??????ffffffffffffffffdddddddddddddddddddddd
//
// send TO rectifier (act as controller)
// 05004804 + ser nr + 00 00 from controller : send 05004804 and 6 bytes ser number followed by 00 00 
// 05FF4004 + 8 bytes AA BB CC DD EE FF GG HH   controller sends current and voltage limits (last 4 is 5 sec walk-in, for 60 second walk-in use 05FF4005)
// 05FF4005 + 8 bytes AA BB CC DD EE FF GG HH   controller sends current and voltage limits (last 5 is 60 sec walk-in, for 5 second walk-in use 05FF4004)
//          with BBAA (in HEX) is the maximum current setting   A2 01 = 41.8 Amps (1820 Watts divided by 43.5 Volt which is the lowest voltage setting)    
//          # DDCC (in HEX) is the system voltage measurement  # (no idea what it really does but set this the same as the output voltage setting)
//          # FFEE (in HEX) is the output voltage in centivolts (0.01V)   43.4 Volt is the lowest voltage, 57.6 Volt which is the highest voltage setting
//          # HHGG (in HEX) is the overvoltage protection setting
//
// If you then send the rectifier
// CanID 0x0501BFFC + data 0x08, 0x04, 0x00,
//    it will send you back:
//    CanID 0x0501BFFC + data 0x0E, 0x04, 0x00, 0xYY, 0xZZ, 0x00, 0x00
//    where each bit of YY relates to the following WARNINGS
//       80 = Current limit
//       40 = low temp
//       20 = High temp
//       10 = Low Mains
//       08 = High Mains
//       04 = Mod Fail Secondary
//       02 = Mod Fail Primary
//       01 = OVS Lock Out
//       and each bit of the of ZZ relates to the following WARNINGS
//       80 = Inner Volt
//       40 = Fan3 Speed low
//       20 = Sub Mod1 fail
//       10 = Fan2 Speed low
//       08 = Fan1 Speed low
//       04 = Mod Fail Secondary
//       02 = Module fail
//       01 = Internal Voltage
//
// permanent default voltage change:
// serial number of your unit is 123456789012
// send 0x05004804 0x12 0x34 0x56 0x78 0x90 0x12 0x00 0x00 (to log in to the rectifier)
// send 0x05009C00 0x29 0x15 0x00 0x80 0x16 (to set the permanent default voltage =  57.5 Volt, The default voltage is determined by the last 2 bytes =0x1680 ) 
//
//   el_status =  -4 : Relay error,     not yet implemented!!
//                -3 : switch off eltek, or something is wrong: line power off, Relay for charging off ->     CEL_RELAIS_PIN=0, CEL_CHARGE_PIN=0
//                -2 : wait time ( =CEL_SWAIT)to switch power line on (for filtering short power on settings) CEL_RELAIS_PIN=0, CEL_CHARGE_PIN=0
//                -1 : line power on, after power line on, or if power<=0 or no communication possible        CEL_RELAIS_PIN=1, CEL_CHARGE_PIN=0
//                 0 : communication ok, line voltage ok, vout ok         -->change to 1                      CEL_RELAIS_PIN=1, CEL_CHARGE_PIN=1
//                 1 : set vout to battery voltage, set current=0         -->change to 2         
//                 2 : set to minimum current (=CEL_MINCURRENT), to avoid high current spikes
//                10 : vout ok, current set to power/vout   :  Constant Current loading, Voltage increase to CEL_BAT_VMAX_CV
//                20 : vout ok, Voltage set to CEL_BAT_VMAX_CV :  Constant Voltage loading
//
//  Bug:
//    not known but some points to do!!
//
// to do:
//      Berechnung status in loop machen !!
//      Beim laden Batterieunterbrechung erkennen (Strom ist =0.1!!) Status entsprechend ändern.
//      relai error erkennen, Line power off -> vout==0 else error (wenn zeit abgelaufen)
//
// Power consumption:
//      Eltek Powersupply idle (battery current=0):   7W (line power)   -> switch off with Relais, but than 90mA at battery side
//          if Powersupply off and Eltek connect to battery directly (without relais), than battery current= 90ma (~4.5W) into eltek!!
//          this can destroy the the Eltek power supply!!
//
//
#define SERIAL_DEBUG_SEPARATOR    " "
#include <SerialDebug.h>
#include <mcp_can.h>
#include <mcp_can_dfs.h>
#include <SPI.h>
#include <math.h>

                               
MCP_CAN CAN(SPI_CS_PIN);                               // Set CS pin for CANBUS shield


long unsigned int rxId;
unsigned char serialnr[8];      //={0x11, 0x48, 0x71, 0x00, 0x00, 0x17, 0x00, 0x00};
int msgreceived;
unsigned char el_temp,el_vin;

int el_status;
int unknown_id;
int no_connection;              // counter, will increase if now answer from eltek
bool el_vout_wait;
unsigned long el_time_vwait;        // timevalue for increasing voltage   (=CEL_WAIT)
unsigned long el_time_wait;         // timevalue for switching from status==-2 to -1 (=CEL_SWAIT)
unsigned long el_time_loop;
unsigned long el_current_timeout;  


void eltek_setup() {                                                                  // Initialisation routine
START_INIT:
    pinMode(CEL_CHARGE_PIN, OUTPUT);
    pinMode(CEL_RELAIS_PIN, OUTPUT);
    eltek_disable(true); 
    if(CAN.begin(MCP_ANY, CAN_125KBPS, MCP_8MHZ) == CAN_OK) {                         // init can bus : baudrate = 125k !!
        DEBUG("CAN BUS Shield init ok!");}
    else {
        lcd_error(2);    
        DEBUG("CAN BUS Shield init fail");
        DEBUG("Init CAN BUS Shield again");
        delay(100);
        goto START_INIT;}
    CAN.setMode(MCP_NORMAL);                     // Set operation mode to normal so the MCP2515 sends acks to received data.

    pinMode(CAN0_INT, INPUT);                   // Configuring pin for /INT input
    unknown_id=0; 
    delay(10);   
    el_ws=0;
    el_cv_loading=false;
    el_current_timeout=0;
}

void el_vwait() {
// el_vout changed, so wait some time to avoid changing el_vout again
//
  if ((el_vout_wait)&&((millis()-el_time_vwait)>CEL_WAIT))
      el_vout_wait=false;
  else if (not el_vout_wait){
      el_vout_wait=true;  
      el_time_vwait=millis();   
  }
}

void eltek_set_vout(int inc){                      // check and set Vout, if ok than  el_status=el_status+inc
/*    if (el_vout < CEL_VOUT_MIN) {                  // Vout to low ->something is wrong --> disable eltek
        eltek_disable(true);
        DEBUG("el_vout < CEL_VOUT_MIN -> Something is throng",ev_vout,elstatus);
    }else */
    if (el_vout< CEL_BAT_VMIN) {
        el_target_vout=CEL_BAT_VMIN;             // set eltek voltage out to minimum 
        el_status+=inc;
    }else if ((el_vout<el_target_vout)&& (el_vout<CEL_BAT_VMAX_CV)) {
        el_target_vout=el_vout+CEL_DELTA_VOUT;
        el_status=10;
        if (el_target_vout>=CEL_BAT_VMAX_CV) {
            el_target_vout=CEL_BAT_VMAX_CV;               // maximum Voltage reached -> switch to Constant Voltage
            el_status=20;  
        }    
        el_vwait();  
    }else if ((el_vout>= el_target_vout)&& (el_vout<CEL_BAT_VMAX_CV)) {    //increment vout til reached CEL_BAT_VMAX_CV (CC-mode)
        el_target_vout=el_vout+CEL_DELTA_VOUT;
        if (el_target_vout>CEL_BAT_VMAX_CV)
            el_target_vout=CEL_BAT_VMAX_CV;               // maximum Voltage reached -> switch to Constant Voltage
            el_status=20;
        el_vwait();
    }else if (el_vout<el_target_vout){
        el_status+=inc;  
    }
}


void eltek_enable(){                    
// only call from function eltek_setpower()
// check statsu and connect eltek Powersupply to batterie if ok, 
// set vout to battery voltage and current to minimum
    if (el_status<=-2)
        eltek_disable(true);
    else if ((el_status==-1)&&(el_vin>CEL_MIN_VIN)) {  // if line voltage ok -> status=0
        el_status=0;
        el_target_iout=0;
        el_target_vout=CEL_VOUT_MIN;
        digitalWrite(CEL_CHARGE_PIN, HIGH);                // enable BMS charging
        // DEBUG(" eltek_enable(): set CEL_CHARGE_PIN=1");
        el_vwait();                                    // enable wait 
    }else if (el_vout_wait){                           // wait some seconds until change status
        el_vwait();
    }else if (el_status==0) {                          // set Vout from eltek to battery voltage, and current to 0 
        el_target_iout=0;
        eltek_set_vout(1);                             // check and set vout, if ok than el_status=1
    }else if (el_status==1) {                          // voltage out is ok, now set current to minimum
        el_target_iout=CEL_MINCURRENT;      
        eltek_set_vout(1);                             // check vout, if ok than el_status=2
    }else if ((el_status==2) && (el_iout>(CEL_MINCURRENT-4))) {    // vout ok, minimum current ok 
       el_status=10;                                               // eltek connect to battery, ready for loading cc-mode
    }else if (el_status==2) {
        DEBUG("eltek_enable: Status 2 ok, but Current not ok!! =",el_iout); 
        el_target_iout=CEL_MINCURRENT;
        eltek_set_vout(0);  
    }else
        DEBUG("eltek_enable: Status not ok =",el_status);       
    DEBUG("el_status = ",el_status, el_vout_wait,"el_vout=",el_vout,"el_iout=",el_iout,"el_target_vout=",el_target_vout);    
}


void eltek_disable(bool switchoff){
    digitalWrite(CEL_CHARGE_PIN, LOW);
    // DEBUG(" eltek_disable(): CEL_CHARGE_PIN=0");
    if (switchoff) {
        digitalWrite(CEL_RELAIS_PIN, LOW);        //      diable line voltage, relais 220v switch off
        el_status=-3;
    }else
        el_status=-1;
    el_target_vout=CEL_VOUT_MIN;
    el_target_iout=0;
    el_current_timeout=0;
    el_vout_wait=false;
    if ((no_connection>0) or switchoff) {
        el_iout= 0;
        el_vout= 0;
        el_pout= 0;
        el_temp=0;
    }
}


bool eltek_setpower(int power){               // return with true: battery full: el_target_vout==C_EL_VMAX && el_iout<CEL_MINCURRENT   (aber erst nach einiger Zeit nach einstellung!!)
     int iout;
     bool result;
     result=false;
     
     //DEBUG("eltek_setpower: el_status=",el_status);
     // Filter for enable Power supply
     if ((el_status==-3) and (power > CEL_MINPOWER)){
         el_time_wait=millis();
         el_status=-2;
         DEBUG("Switch to el_status=-2, now wait for filter end");
     }else if ((el_status==-2)and (power <=CEL_MINPOWER)){
         el_status=-3; 
         DEBUG("Switch back to  el_status=-3, not enough power");            
     }else if ((el_status==-2)and (power > CEL_MINPOWER) and ((millis()-el_time_wait)>(CEL_SWAIT*1000))){
         DEBUG("Switch el_status=-1, enough power, for more than ", CEL_SWAIT);             
         el_status=-1; 
         DEBUG(" eltek_setpower(): CEL_CHARGE_PIN=0, CEL_RELAIS_PIN=1");
         digitalWrite(CEL_CHARGE_PIN, LOW);         // switch Eltek connection to Battery off
         digitalWrite(CEL_RELAIS_PIN, HIGH);        // enable line voltage, relay 220v switch on            
     }else if (el_status==-2){
         unsigned long twait=millis()-el_time_wait;
         bool test= ((millis()-el_time_wait)>(unsigned long)(CEL_SWAIT*1000));
         DEBUG("Waiting for start eltek", twait, CEL_SWAIT , power, test);
     }

     // set current and el_vout
     if (power<=0) {
         el_target_iout=0;
     }else if ((el_status>-2) and (el_status<10)){
         eltek_enable();         
     }else if (el_status>=10){                                                     //calculate maxcurrent
         eltek_set_vout(0);                   // check and set vout
         iout=  round(float(power) / float(el_vout/100)*10);  
         if (iout<CEL_MINCURRENT)  
             iout=0;            
         else if ((el_iout<(0.5*CEL_MINCURRENT)) and (iout > 4*CEL_MINCURRENT))      //to avoid currentspike, start with minimum current
             iout=CEL_MINCURRENT;
         else if (iout>CEL_MAXCURRENT)
             iout=CEL_MAXCURRENT;   
         else if (iout<CEL_MINCURRENT)  
             iout=0;

         // check load current, if the current smaller as adjusted inside the time window (el_current_timeout)
         if ((el_vout>CEL_BAT_VMAX) and (iout>=CEL_MINCURRENT) and (el_iout<2)and (el_current_timeout==0)){
             el_current_timeout=millis();
             DEBUG("el_vout>CEL_BAT_VMAX, set el_current_timeout",el_vout);                  
         }else if (el_current_timeout==0){
             // DEBUG("eltek: el_vout,target iout, el_iout, el_current_timeout",el_vout,iout,el_iout,el_current_timeout);            
             el_current_timeout=0;
         }else{
             DEBUG("eltek_setpower(): wait for timeout: el_current_timeout, actual, target",(millis()-el_current_timeout),CEL_CURRENT_TIMEOUT);
             // DEBUG((millis()-el_current_timeout)> ((unsigned long)CEL_CURRENT_TIMEOUT));
         }
         if ((el_current_timeout!=0) and ((millis()-el_current_timeout)> ((unsigned long)CEL_CURRENT_TIMEOUT))){
             eltek_disable(true);   
             DEBUG("eltek_setpower(): el_vout>CEL_BAT_VMAX",el_vout);     
             DEBUG("disable eltek"); 
             result=true;                                         
         }
         if (el_cv_loading or ((el_vout>= CEL_BAT_VMAX_CV) and (iout>CEL_MAXCC_CURRENT))) {      //Batterie almost full, reduce current to avoid overvoltage above CEL_BAT_VMAX_CV
             if (iout>CEL_MAXCC_CURRENT) 
                 iout=CEL_MAXCC_CURRENT;
             el_cv_loading=true;                // = Batterie almost full, reduce current
             if (el_vout>=CEL_BAT_VMAX) {       
                 el_target_iout=0;              // = now Batterie is full
                 result=true;  
                 DEBUG("Battery full, el_vout=CEL_BAT_VMAX",el_vout);                
             }
         }
         el_target_iout=iout;          
     }
//     DEBUG("el_target_iout",el_target_iout);
     return (result);
}



void eltek_loop() {                                            // main program (LOOP)
    //DEBUG("eltek_loop");
    unsigned char len = 0;
    unsigned char buf[8] ;
    if(CAN_MSGAVAIL == CAN.checkReceive()){                    // check if data coming
        no_connection=0;
        CAN.readMsgBuf(&rxId,&len, buf);                       // read data,  len: data length, buf: data buf
        if (len>8)
           DEBUG("!!!!!! eltek_loop: CAN buffer overflow !!!!!!!!!!!!!!!!!!!!");
           INT32U canId=rxId & 0x3fffffff;
//          DEBUG("\t0x");                                // send Tab
//          DEBUG(canId,HEX);                             // output CAN Id to serial monitor
//          DEBUG("\t");          
        if(canId==0x05014400) {                          //this is the request from the Flatpack rectifier during walk-in (start-up) or normal operation when no log-in response has been received for a while
            for(int i = 0; i<8; i++){
                serialnr[i]=buf[i]; }                   // transfer the message buffer to the serial number variable
            CAN.sendMsgBuf(0x05004804, 1, 8, serialnr); //send message to log in (DO NOT ASSIGN AN ID use 00)
            msgreceived++;
           // unsigned char stmp7[8] = {lowByte(el_target_iout), highByte(el_target_iout), 0x44, 0x16, 0x44, 0x16, 0x3E, 0x17};    // set rectifier to maxcurrent 57,0V (16 44) and OVP to 59.5V (17 3E) qnd long walk-in 4005 or short walk in 4004
           // CAN0.sendMsgBuf(0x05FF4005, 1, 8, stmp7);                                                                     //(last 4 in header is for 5 sec walkin, 5 is for 60 second walkin)
        }
        else if(canId== (INT32U) (0x05000000+256*buf[5]+buf[6])) {         // if CANID = 0500xxyy where xxyy the last 2 digits of the serial nr
            for(int i = 0; i<6; i++) {                                                
                serialnr[i]=buf[i+1];                                      // transfer the buffer to the serial number variable (neccesary when switching the CAN-bus to another rectifier while on)
            }            
            unsigned char serialnr[8] = {buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], 0x00, 0x00};          //this is the serial number of the unit which is covered in the request (unit announces its serial number)
            CAN.sendMsgBuf(0x05004804, 1, 8, serialnr);                                                        //send message to log in (DO NOT ASSIGN AN ID) use 00
            msgreceived++;            
            unsigned char stmp7[8] = {lowByte(el_target_iout), highByte(el_target_iout), lowByte(el_target_vout), highByte(el_target_vout), 0x44, 0x16, 0x3E, 0x17};  // set rectifier to maxcurrent and OVP, qnd long walk-in 4005 or short walk in 4004
            CAN.sendMsgBuf(0x05FF4005, 1, 8, stmp7);                                    //(last 4 in header is for 5 sec walkin, 5 is for 60 second walkin)            
//            DEBUG("eltek_loop: Output Voltage set to : ",(stmp7[3]*256+stmp7[2])*0.01);
//            DEBUG("eltek_loop: max. Current set to : ",(stmp7[1]*256+stmp7[0])*0.1);            
       }
       else if ((canId==0x05014004)or(canId==0x05014008)or(canId==0x05014010)or(canId=0x0501400C)) {     
           // DEBUG("CanId =",canId, " Buffer =");   
           // for(int i = 0; i<len; i++) {                                                   // print the data
           //  if( buf[i] < 0x10){ Serial.print("0");} Serial.print(buf[i],HEX);            // send a leading zero if only one digit
           //      Serial.print(" ");                                                       // space to seperate bytes
           // }
           el_temp=buf[0];                                           // first byte is temperature in celcius
           el_iout=(buf[2]*256+buf[1]);                              // third (msb) and second byte are current in 0.1A (deciamp) calculated to show directly in Amp
           el_vout=(buf[4]*256+buf[3]);                              // fifth (msb) and fourth byte are voltage in 0.01V (centivolt) calculated to show directly in Volts dc
           if (el_iout>=CEL_CURRENT_OFFSET)
              el_iout=el_iout-CEL_CURRENT_OFFSET;     
/*           if ((el_iout<5) and (el_status>=10)){
            DEBUG("el_iout < 0.5A!!");
           }*/
           if (el_status>=10) {
//            DEBUG("el_iout=",el_iout,"target iout",el_target_iout,"el_status",el_status, "el_temp=", el_temp);               
           }
           el_pout=(int(el_vout)*0.1) * (el_iout*0.1);               // Power is calculated from output voltage and current. Output is in 100mWatts  
           if (el_iout<=2)
               el_pout=0;
           el_vin=buf[5];                                               // sixth byte is line voltage in volts ac        
           //buf[7]*256+buf[6]                                          // ??
                                                        
//           DEBUG("Messages received = ",msgreceived); 
           if  ((el_status>0)and(el_status<10)) {
               DEBUG("OutputVoltage =",el_vout*0.01," Current =",el_iout*0.1," OutputPower =",el_pout," max target Vout =",el_target_vout*0.01);
               DEBUG("max Current =",el_target_iout*0.1," InputVoltage =",el_vin," Temperature =",el_temp );  
           }
// check if status from eltek ok:
//           if (el_vin< CEL_MIN_VIN) {                     // line voltage not ok
//               el_status= -1;
//               eltek_disable(false);   
//           }
            
           if (unknown_id>0){
               DEBUG("Unknown IDs = ",unknown_id);  
           } 
//send request for new update
           msgreceived++;                                               // record number of messages received
           if(msgreceived>20) {
               msgreceived=1;
               CAN.sendMsgBuf(0x05004804, 1, 8, serialnr);             //send message to log in every 40 messages (DO NOT USE ID NR, USE 00) this because during walk-in the 0500xxyy is not send and the rectifier "logs out" because of no received log-in messages from controller
           }   
        } else {
            unknown_id++;
            DEBUG("Unknown CanId  ");      
            for(int i = 0; i<len; i++) {                                                  // print the data
                if( buf[i] < 0x10){ Serial.print("0");} Serial.print(buf[i],HEX);         // send a leading zero if only one digit
                Serial.print(" ");
            }                                                                             // space to seperate bytes
            Serial.println(" ");
        }
    }else if (el_status>=-1){
        no_connection++;
        if (no_connection>5){
            DEBUG("No connection to Eltek power supply ",no_connection);  
            el_status=-1;            
            eltek_disable(false); 
        }
    }  
    el_ws=el_ws+ float(el_pout)*0.1 * (millis()-el_time_loop)/1000;             // calculate total power in Ws
    el_time_loop=millis();    
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
