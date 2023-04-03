
#define CAN0_INT 2                 // Set INT to pin 2
#define SPI_CS_PIN  9              // Cip Select from CAN SPI

#define CEL_MIN_VIN         210                   // minimum line voltage 
#define CEL_MINPOWER        50                    // minimum load power to enable eltek power supply
#define CEL_MINCURRENT      12                    // minimum battery load current (*100mA) = 1.2A
#define CEL_MAXCURRENT      350                   // maximum battery load current (*100mA) = 28.5A
#define CEL_MAXCC_CURRENT   120                   // maximum battery load current if in constant current mode (*100mA) = 12A
#define CEL_VOUT_MIN        4620                  // minimum Eltek Vout (*10mV) = 14*3,3V
#define CEL_DELTA_VOUT      10                    // increment Eltek Vout (*10mV) with 100mv
#define CEL_BAT_VMIN        4760                  // minimum battery voltage(*10mV = 14*3,4
#define CEL_BAT_VMAX_CV     5600                  // maximum battery voltage(*10mV), if reached switch to CV loading  = 14*4,0V
//#define CEL_BAT_VMAX_CV     5320                  // 14*3.8  for Sommer                                                 
#define CEL_BAT_VMAX        CEL_BAT_VMAX_CV+42    // absolut maximum battery voltage battery cell voltage +42mV *14 cells
#define CEL_BAT_HYST_LOAD   140                   // hysteresis for loading enable after battery full   = 14* 0.1V =1.4V
#define CEL_CURRENT_OFFSET  2                     // current Correcttion value for calculate Power

#define CEL_WAIT   20000                   // wait in ms after increase vout and current
#define CEL_SWAIT  30                     // filter time to wait after target power > 0, to enable/disable eltek
#define CEL_LOOP_TIME 250                 // call loop every 250ms

#define CEL_RELAIS_PIN 7                      // Pin Relais for on/off Powersuplly from line
#define CEL_CHARGE_PIN 6                      // Pin to BMS for charging enable
#define CEL_CURRENT_TIMEOUT 60*1000       // time window for checking CEL_BAT_VMAX


int el_pout;                       // measured power out, 1 count= 100mW
float el_ws;                       // loading Energy in Ws
int el_iout;                       // 1 count= 0.1A
unsigned int el_vout;              // 1 count= 10mV
unsigned int el_target_vout;       // 1 count= 10mV
int el_target_iout;                // 
bool el_cv_loading;

//Function prototypes
void eltek_setup();
void eltek_disable();
void eltek_loop();
bool eltek_setpower(int power);
