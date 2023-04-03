/*
   Battery Control:   with Arduino Mega 2560

    Author:  Christian Jung
    Date:    06.11.2022
    Version: 0.31

    https://creativecommons.org/licenses/by-nc-nd/4.0/
  
    - get data about mqtt:                                                     (see mqtt.h, mqtt)
       Solar:
         - sma/P_AC                 --> variable: Battery/solar
         - sma/TIME
       evu_import
         - Smartmeter/1-0:16.7.0    -> variable: Battery/evu_import
       loading power from Wallbox
         - Wallbox/power            if wallbox loading, than switch off loading battery, 
      publish data with mqtt: 
         - Battery/online
         - powertarget, power, store, el_vout, stored_vbat, ae_status, el_status  ->should be changed to one topic with json-format
         - controll Battery current for load and discharge
         - run ethernet UDP-client with export from some Battery status variable     (see sml_client, sml_client)
          -> replaced with mqtt, but not removed from this version...
             have to change receiver client to mqtt first
         - connect eltek power supply via CAN for loading                            (see eltek.h, eltek)
            read out Temp,Vin, Vout, Iout, Pout
            set vout, iout and calculate power for loading battery
         - connect aeconversion INV500-90EU RS485 via RS485/UART                     (see aeconversioh.h, aeconversion)
            switch off MPP-Tracking, 
            set Voltage and current for discharge
    - display some information on a smal OLED Display  via I2C                  (see lcd.h, lcd)
            
    - used Pins:
          LCD :   I2C                             0,96" OLED Display with I2C
                  SDA          Pin 20 
                  SCL          Pin 21 
          CAN :   CAN0_INT     Pin  2 (INT4)      SPI MCP2515 CAN Bus Modul 
                  SPI_CS_PIN   Pin  9 (PWM)
                  SPI SCK      Pin 52
                      SI       Pin 51
                      SO       Pin 50
          SD-Card:             Pin 53

          Ethernet    SCK      Pin D13
                      MISO     Pin D12
                      MOSI ~   Pin D11 
                      SS*      Pin D10
                               Pin A0
                               Pin A1
                               Pin D4
                                                  
          RS485:                         RS485 Pinout:  
                  RX1          Pin 19 -  RO      (Pin 4)
                  TX1          Pin 18 -  DI      (Pin 1)
                               Pin  3 -  DE/RE_n (Pin 2/3)   for communication aeconversion
                                  A  -> CAT5(Pin 6) green
                                  B  -> CAT5(Pin 3) green/white
                               
          Relais:  CEL_RELAIS    Pin  7   for Eltek-line Power supply
                   CEL_CHARGE    Pin  6   charge enable for bms (AIDO2-green )
                   AE_DISCHARGE  Pin  5   discharge signal for bms (AIDO1- yellow))   not implemented yet                 


   todo:  
      - add filter for loading, wenn erkannt wird das ein Verbraucher in kurzen Abständen viel Strom zieht(z.Bsp getaktete Herdplatte), dann den eigenen Ladestrom während den Abständen nicht hochfahren
      - um 0:00 zähler von aeconversion auf 0 stellen.
      - get bms-information via CAN and send it via mqtt
                   
    Arduino Power consumption:
            with Relais Eltek line power: 3.9Wh
            AECONVERSIon needs 45mA from Battery, also if current set to 0 
 */
#define SERIAL_DEBUG true
#define SERIAL_DEBUG_SEPARATOR    " "
#include <SerialDebug.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <avr/wdt.h>

#include "lcd.h"
#include "eltek.h"
#include "sml_client.h"
#include "mqtt.h"
#include "aeconversion.h"
#include "sd_card.h"

#define CBA_MINBAT_SOLAR 150         // min Solarpower for charging Battery, if smaller than will not be charging Battery
#define CBA_MIN_SOLAR 100            // min Solarpower, if smaller than disable power line for Eltek
#define CBA_MIN_HPOWER 70            // minium house power consumption, for plausible test
#define CBA_MAX_CPOWER 1800          // maximum charge Power
#define CBA_MIN_CPOWER 25            // minimum charge Power
#define CBA_MAX_DPOWER 500           // maximum discharge Power
#define CBA_MIN_DPOWER 15            // minimum discharge Power

#define CBA_LOAD_OFFSET 15           // Battery load= Import + CBA_LOAD_OFFSET    (import is negativ if loading)

//int period = 1000;


int bat_target;              // target battery power
int bat_actual;              // actual battery power
int  bat_status = 0;         // acutal battery status
//                0 = unknown
//               -1 = empty
//                1 = enabled
//                2 = load_reduced
//                3 = load
bool update_batcalculate;    // if set, than new calculate from batterycurrent
bool bat_full;               // batterie is full
bool bat_empty;              // batterie is empty
bool reset;
int bat_update;
int solar;                   // solar power, get from mqtt
int evu_import;              // EVU import power, get from mqtt
int wallbox;                 // Wallbox loading poiwer, get from mqtt

unsigned long time_loop;
unsigned int time_count;
bool reset_cnt; 

void setup() {
    uint8_t mcusr_copy;
    wdt_enable(WDTO_8S);
    mcusr_copy = MCUSR;
    MCUSR = 0x00;        //Clear register

    SERIAL_DEBUG_SETUP(115200);
    //Serial.begin(115200);
    while (!Serial) {}             // wait for serial port to connect. Needed for native USB port only

    DEBUG("MCUSR : ",mcusr_copy);
    if(mcusr_copy & (1<<WDRF)){
        DEBUG("Wadog reset : ",mcusr_copy);  // a watchdog reset occurred
    }
    if(mcusr_copy & (1<<BORF)){
        DEBUG("brownout  reset : ",mcusr_copy);   //  a brownout reset occurred
    }
    if(mcusr_copy & (1<<EXTRF)){
        DEBUG("exernal reset : ",mcusr_copy);  //  an external reset occurred
    }
    if(mcusr_copy & (1<<PORF)){
        DEBUG("Power on Reset : ",mcusr_copy);//  a power on reset occurred
    }

    setup_lcd();
    sml_client_setup();    
    mqtt_setup();
    eltek_setup();
    ae_setup();
    //sdcard_setup();
    update_batcalculate=true;
    bat_target=0;
    solar=-1;
    lcd_page1();
    time_loop=millis();
    time_count=0;
    bat_full=false;
    bat_empty=false;
    reset=true;
    reset_cnt = true;
    bat_update=0;
    digitalWrite(13,LOW);
}


void set_bat(){
//charge  
    if ((!bat_full) and (solar>CBA_MINBAT_SOLAR) and (ae_Ibat<=0.2) and (bat_target<0) and (wallbox<100)){  //battery is not full, discharge disabled, enough power from solar available and wallbox is not loading-> charge battery
        bat_full=eltek_setpower(-bat_target);
        DEBUG("eltek load, Import, bat_target,bat_actual",evu_import,bat_target,bat_actual,"el_pout(100mw), el_vout, el_iout, target el_iout, el_ws",el_pout,el_vout,el_iout,el_target_iout,el_ws/3600);    //display eltek
        if (bat_empty and el_vout>(int(AE_VMIN+AE_VMIN_HYST)*100)) {
             bat_empty=false;
        }
    }else if ((solar<CBA_MIN_SOLAR) or bat_full){         // solar power to small or battery full?     
        eltek_disable(true);
    }else {                             //>CBA_MIN_SOLAR(=100) and <CBA_MINBAT_SOLAR(=150)
        eltek_setpower(0);
        // DEBUG("eltek_setpower(0) solar,ae_Ibat,bat_target=",solar,ae_Ibat,bat_target);      
    }
//discharge    
    if ((!bat_empty) and(el_iout<=2) and (bat_target>=0) ) {         // discharge battery if battery not empty, el_iout should be <200mA      
        bat_empty= not ae_set_power(bat_target);
        if (bat_status < 1)
            bat_status = 1;
        DEBUG("ae discharge Import, error_import",evu_import,evu_error,"bat_target,ae_Pnetz,ae_Pbat, ae_status, ae_Energy",bat_target,ae_Pnetz,ae_Pbat,ae_status,ae_Energy);        //ae conversion
        if ((int(ae_Vbat*100))<(CEL_BAT_VMAX-CEL_BAT_HYST_LOAD)) {
            el_cv_loading=false;
            bat_full=false;
        }
    }else if (!bat_empty){      
        bat_empty= not ae_set_power(0);
        DEBUG("ae_set_power(0),el_iout,bat_target",el_iout,bat_target);
    }else{
        bat_empty= not ae_set_power(0);     //bms scheint bei disable nicht abzuschalten??!
        DEBUG("Battery is empty");
        ae_disable();
        bat_status = -1;
    }     
}


void loop() {   
    if (reset&&(time_count%(1000/CEL_LOOP_TIME)==0)) {
        ae_set_power(0);
        eltek_disable(true);                                            // switch off eltek from line
        bat_target=0;
        if (time_count/(20*1000/CEL_LOOP_TIME)>=1){                     // reset 20s
            reset=false;
            DEBUG("End reset",time_count);
        }              
    }else if ((time_count%(6*1000/CEL_LOOP_TIME)==0)|| (bat_update>0)){   // Battery update each 6s for ae,    6s for eltek
        int new_bat_target;         
        bat_actual= -el_pout*0.1 +ae_Pnetz;     
        new_bat_target=-el_pout*0.1 +ae_Pnetz+evu_import;              // import<0 if export...  
        
        if (new_bat_target<-CBA_LOAD_OFFSET)                           // calculate reserve solar-power
            new_bat_target= new_bat_target+CBA_LOAD_OFFSET;
        else if (new_bat_target<0)
            new_bat_target=0;
     
        if ((ae_Pnetz<10) && (new_bat_target<0)) {                     // check constrains for charging, aeconversion have to be disabled!!
            if (-new_bat_target<CBA_MIN_CPOWER)                        //                  min charging
                new_bat_target=0;        
            else if (-new_bat_target>CBA_MAX_CPOWER)                   //                  max charging
                new_bat_target=-CBA_MAX_CPOWER;                
        }else if (ae_Pnetz>10){                                        // check constrains for discharging
            if (new_bat_target<CBA_MIN_DPOWER)                         //                  min discharging
                new_bat_target=0;
            else if (new_bat_target>CBA_MAX_DPOWER)                    //                  max discharging
                new_bat_target=CBA_MAX_DPOWER;
        }
        if ((solar>CBA_MINBAT_SOLAR) and new_bat_target<(-solar+CBA_MINBAT_SOLAR)) {      //solar charging could be never bigger as solar-CBA_MINBAT_SOLAR
            new_bat_target=-solar+CBA_MINBAT_SOLAR;
        }        
        bat_target= new_bat_target;   
    }        
    if (evu_error) {
      bat_target=0;
    }
    if (!reset&&(time_count%(2000/CEL_LOOP_TIME)==0)){                 // update each 2s
        set_bat(); 
    }
    if ((utc < 1000) and not reset_cnt) {           // at 0:00  reset counter from el_ws  
        el_ws = 0;
        reset_cnt = true;
    }else if(utc>1000){
        reset_cnt = false;
    }       
    sml_sendbat(time_count);   
    eltek_loop();
    display_page();
    mqtt_loop();                                            // max 100ms for mqtt messages
    while ((millis()-time_loop)<CEL_LOOP_TIME);             // loop with 250ms
    time_loop=millis();
    time_count++;
    wdt_reset();
}
