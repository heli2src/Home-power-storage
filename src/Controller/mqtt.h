



// defines:
#define CS_ETHERNET 10                  // Pin CS for Ethernet Card
#define MQTT_SERVER "192.168.178.28"    // = Broker
#define MQTT_LOOP_TIME  100             // get 100ms to read the mqtt subscribe messages

#define PSOLAR "sma/P_AC"               // mqtt topic actual power solar
#define STIME  "sma/TIME"               // mqtt topic actual date and time in seconds 
#define EVUIMPORT "Smartmeter/1-0:16.7.0"
#define WALLBOX "Wallbox/power"         // actual loading power from the Wallbox

unsigned long utc;                      // time in seconds (utc time)            
bool evu_error;

//Function prototypes
void mqtt_setup();
void mqtt_loop();
