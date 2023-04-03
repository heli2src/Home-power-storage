#define AE_TYP "500-90"
#define AE_ERROR_MAX 5                        // max read failure, if reached > switch to status = -2
#define AE_DISCHARGE_PIN 5                    // Pin to BMS for discharging not implemented yet!!
#define AE_ENABLE_PIN 3                       // Set EN to pin 3, 1-> write tx, 0= read tx
#define AE_TIMEOUT 600                        // in ms
#define AE_BUFFER_SIZE 65

#define AE_VNETZ_MIN 200
#define AE_VNETZ_MAX 250

#define AE_MAXCURRENT  11.0                   // maximum Current
#define AE_VMIN  50.0                         // mimimum Batterie Voltage = 50.0/14Cells ->3.57V/cell, if smaller ae_status set to -2
#define AE_VMIN_HYST 0.5                      // if Battery Voltage < AE_VMIN +AE_VMIN_HYST  -> ae_status set to -2

#define AE_VMAX  60.0                         // maximum Batterie Voltage = 14Cells *4,2 -> 58,8V
#define AE_ETA   0.94                         // efficiency factor Batterie current -> line current

#define AE_CWaitTime 15000                    // 15s wait after each set new current


int ae_Pnetz;
int ae_Pbat;
int ae_status;                               // Sofware status fsm
int ae_Energy;
float ae_Ibat;
float ae_Vbat;

//Function prototypes
void ae_setup(); 
bool ae_set_power(int power); 
