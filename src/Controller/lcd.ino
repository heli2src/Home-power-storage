#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
#include <math.h>

#define I2C_ADDRESS 0x3C

SSD1306AsciiWire oled;

unsigned long lcdtime_change;
int lcd_page;

char *dtostrf(double val, signed char width, unsigned char prec, char *s);

void setup_lcd() {
    Wire.begin();
    Wire.setClock(400000L);
    oled.begin(&Adafruit128x64, I2C_ADDRESS);
    oled.setFont(Adafruit5x7); 
    oled.invertDisplay(true);
    oled.clear();  
    oled.setContrast(255);
    oled.set2X();
    oled.println("Solar 0.31");
    oled.println("3.04.2023");
    oled.println(" (c)Heli2 ");
    oled.set2X();    
    //oled.println("init ");   
    lcd_page=1;
    lcdtime_change=millis();   
    
}


void lcd_error(int code) {
    oled.set2X();   
    if (code==1) {      
      oled.println("Ethernet shield");
      oled.println("not found!!");
    }
    else if (code==2) {
        oled.println("CANBUS Shield:");
        oled.println("init failed");            
    }
}

void lcd_page1() {
    oled.invertDisplay(false);
    oled.clear();  
    oled.set2X();  
    oled.setRow(0);  
    oled.setCol(0); 
    oled.println("Pin:"); 
    oled.setRow(2);  
    oled.setCol(0); 
    oled.println("Sol:");   
    oled.setRow(4);  
    oled.setCol(0); 
    oled.println("BatT:"); 
    oled.setRow(6);  
    oled.setCol(0); 
    oled.println("BatA:"); 
}


void lcd_page2() {                  // eltek page   Temperatur    Power
                                    //              Vout          
                                    //              Iout
                                    //              Status
    oled.invertDisplay(false);
    oled.clear();  
    oled.set2X();  
    oled.setRow(0);  
    oled.setCol(0); 
    oled.println("T:   P: "); 
    oled.setRow(2);  
    oled.setCol(0); 
    oled.println("U: ");   
    oled.setRow(4);      
    oled.println("I: ");       
    oled.setRow(6);  
    oled.setCol(0); 
//    oled.println("uID:");         // unknown ID
    oled.println("Sta:"); 
}

void lcd_page3() {                  // aec page
    oled.invertDisplay(false);
    oled.clear();  
    oled.set2X();  
    oled.setRow(0);  
    oled.setCol(0); 
    oled.println("T:    P: "); 
    oled.setRow(2);  
    oled.setCol(0); 
    oled.println("U: ");   
    oled.setRow(4);      
    oled.println("I: ");       
    oled.setRow(6);  
    oled.setCol(0); 
//    oled.println("uID:");         // unknown ID
    oled.println("Sta:"); 
}


void lcd_var_page1(){               // allgemein 
    char disp_buffer[21]; 

    oled.setRow(0);  
    oled.setCol(55); 
    if (evu_error) {    
        oled.println("n.c."); 
    }else{  
        sprintf(disp_buffer, "%5dW", evu_import);  //  
        oled.println(disp_buffer);
    }
    oled.setRow(2);  

    if (solar == -1) {
        oled.setCol(70);       
        oled.println("n.c."); 
    }else{      
        oled.setCol(55);       
        sprintf(disp_buffer, "%5dW", solar);  //
        oled.println(disp_buffer);
    }
    oled.setRow(4);  
    if (reset) {
         oled.setCol(65);       
         oled.println("reset");     
    }
    else {
        oled.setCol(55);       
        sprintf(disp_buffer, "%5dW", bat_target);  //
        oled.println(disp_buffer);  
    }
    
    sprintf(disp_buffer, "%5dW", bat_actual);  //   
    oled.setRow(6);  
    oled.setCol(55); 
    oled.println(disp_buffer);  

//    sprintf(disp_buffer, "%6d", error_connect);  //
//    oled.setRow(6);  
//    oled.setCol(46); 
//    oled.println(disp_buffer);  
     
}


void lcd_var_page2(){             // eltek var page   Temperatur    Power
                                  //                  Vout          Target Vout
                                  //                  Iout          Target Iout
                                  //                  Status        Energy Wh
    char disp_buffer[20]; 
    char str_temp[7];    
     
    sprintf(disp_buffer, "%2d", el_temp);  //  
    oled.setRow(0);  
    oled.setCol(25); 
    oled.println(disp_buffer);

    sprintf(disp_buffer,"%3ld", round(el_pout/10));
    oled.setRow(0);  
    oled.setCol(80); 
    oled.println(disp_buffer);

    dtostrf(float(el_vout*0.01), 3, 1, str_temp);
    sprintf(disp_buffer,"%s ", str_temp);    
    oled.setRow(2);  
    oled.setCol(25); 
    oled.println(disp_buffer);       

    dtostrf(float(el_target_vout*0.01), 3, 1, str_temp);
    sprintf(disp_buffer,"%s ", str_temp);    
    oled.setRow(2);  
    oled.setCol(80); 
    oled.println(disp_buffer);       


    if (bat_full) {
        oled.setRow(4);  
        oled.setCol(25);
        oled.println("Full");            
    }else{
        dtostrf(float(el_iout*0.1), 4, 1, str_temp);
        sprintf(disp_buffer,"%s ", str_temp);    
        oled.setRow(4);  
        oled.setCol(25); 
        oled.println(disp_buffer);      
    }

    dtostrf(float(el_target_iout*0.1), 4, 1, str_temp);
    sprintf(disp_buffer,"%s ", str_temp);    
    oled.setRow(4);  
    oled.setCol(80); 
    oled.println(disp_buffer);       

/*    sprintf(disp_buffer, "%3d", unknown_id);     //  unknown_id from eltek
    oled.setRow(6);  
    oled.setCol(35); 
    oled.println(disp_buffer);  */
    
    sprintf(disp_buffer, "%3d", el_status);     //  status from eltek-software
    oled.setRow(6);  
    oled.setCol(30); 
    oled.println(disp_buffer); 
    
/*    sprintf(disp_buffer, "%3d", no_connection);  //  no_connection from eltek
    oled.setRow(6);  
    oled.setCol(90); 
    oled.println(disp_buffer);         
*/
    dtostrf((el_ws/3600), 5, 0, str_temp);        // display loading Energy in Wh
    sprintf(disp_buffer,"%s ", str_temp);  
    oled.setRow(6);  
    oled.setCol(70); 
    oled.println(disp_buffer);

}


void lcd_var_page3(){             // aec var page
    char disp_buffer[20]; 
    char str_temp[6];    
     
    sprintf(disp_buffer, "%2d", ae_Temp);             //ae_Temp
    oled.setRow(0);  
    oled.setCol(25); 
    oled.println(disp_buffer);
              
    sprintf(disp_buffer,"%3d", ae_Pbat);              // ae_Pbat
    oled.setRow(0);  
    oled.setCol(90); 
    oled.println(disp_buffer);

    dtostrf(ae_Vbat, 3, 1, str_temp);                 // ae_Vbat
    sprintf(disp_buffer,"%s ", str_temp);    
    oled.setRow(2);  
    oled.setCol(25); 
    oled.println(disp_buffer);       

    dtostrf(ae_Ibat, 3, 1, str_temp);                 // ae_Ibat
    sprintf(disp_buffer,"%s ", str_temp);    
    oled.setRow(4);  
    oled.setCol(25); 
    oled.println(disp_buffer);       

    dtostrf(ae_Energy, 4, 0, str_temp);                 // ae_Energy
    sprintf(disp_buffer,"%s ", str_temp);    
    oled.setRow(4);  
    oled.setCol(80); 
    oled.println(disp_buffer);          
   
    sprintf(disp_buffer, "%3d", ae_status);       //  status from aec-software
    oled.setRow(6);  
    oled.setCol(35); 
    oled.println(disp_buffer); 
    
    sprintf(disp_buffer, "%3d", ae_hstatus);      //  status from aec-hardware
    oled.setRow(6);  
    oled.setCol(90); 
    oled.println(disp_buffer);         
}



void display_page(){
    unsigned long lcdtime_now= millis();
    //DEBUG("display_page");
    if ((lcdtime_now-lcdtime_change)>LCD_PAGE_VISIBLE){
        lcdtime_change=millis(); 
        if (not reset) lcd_page++;
        //if ((lcd_page==2) and (el_status<0) and (ae_status>=0)) lcd_page=3;        // page 2=eltek
        //if ((lcd_page==3) and (ae_status<0) and (el_status>-1)) lcd_page=1;   // page 3=ae
 //lcd_page=2;       
        if (lcd_page>LCD_PAGE_MAX) 
            lcd_page=1; 
        if (lcd_page==1) {  
            lcd_page1();
            lcd_var_page1();
        } else if (lcd_page==2) {  
            lcd_page2(); 
            lcd_var_page2();                  
        } else if (lcd_page==3) {  
            lcd_page3(); 
            lcd_var_page3();                  
        }                   
    } else {
        if (lcd_page==1)  
            lcd_var_page1();
        else if (lcd_page==2)   
            lcd_var_page2();             
        else if (lcd_page==3)   
            lcd_var_page3();           
    }
}
