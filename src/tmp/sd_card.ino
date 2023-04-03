#include <SerialDebug.h>
#include <SD.h>


Sd2Card card;
SdVolume volume;
SdFile root;

File logFile;

void sdcard_setup() {
    pinMode(CS_SDCARD, OUTPUT);  
    pinMode(CS_ETHERNET, OUTPUT);      
    selSD();    
    SD.begin(CS_SDCARD);     
/*
    DEBUG("Card type: ",card.type());
    volume.init(card);
    DEBUG("Clusters: ",volume.clusterCount());
    DEBUG("Volume type is: FAT%d",volume.fatType());
    */
    selETH();
}


void selSD() {      // waehlt die SD-Karte aus 
    digitalWrite(CS_ETHERNET, HIGH);
    digitalWrite(CS_SDCARD, LOW);
}

void selETH() {     // waehlt den Ethernetcontroller aus
    digitalWrite(CS_SDCARD, HIGH);
    digitalWrite(CS_ETHERNET, LOW);
}


void write_sdcard(){  

  
    logFile=SD.open("battery.log", FILE_WRITE); 
    //DEBUG("fileopen = ",test);

    logFile.close();
    
}
