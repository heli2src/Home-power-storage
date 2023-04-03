
// Variable
//int  sml_import;
char sml_time[] = "00:00:00";
//bool sml_error;              // values from smartmeter import not ok

// defines:
#define CS_ETHERNET  10          // Pin CS for Ethernet Card

//#define SM_HPOWER  50           // minimum home own consumption in Watt, for plausible test
#define SM_THERROR 3            // threshold read errors for set sml_error, 

//Function prototypes
void sml_client_setup();
void sml_get_data();
void sml_sendbat(int time_count);
