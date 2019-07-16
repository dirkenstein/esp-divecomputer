

#ifdef ARDUINO_ARCH_ESP8266
#include <New_Yield.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <TimeLib.h>
#include <NTP.h>
#include <DS3232RTC.h>
#include <Adafruit_BME280.h>
#include <SC16IS750.h>
#include <NDIRZ16.h>

#include <Syslog.h>
#include "toneAC-esp.h"
#include <ArduinoJson.h>
#include "Base64a.h"

#include "FS.h"
#include <stdarg.h>
//#include <mcheck.h>

extern "C" {
  #include <cont.h>
  extern cont_t * g_pcont;

}

#endif 

#define DEBUG 
//#define TRACE
#include <dirkutil.h>

#include <U8g2lib.h>
#include <EEPROM.h>

#include <Wire.h>
#include <Adafruit_ADS1015.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#include <MS5803_14.h> 


#include <PCF8574.h>
//#undef USE_EXPANDER
#ifdef USE_EXPANDER
   PCF8574 PCF;
   bool gotpcf = false;
#endif
#include <PID_v1.h>
#include <PID_AutoTune_v0.h>

#define PRESSURE 

//#define HUD
#define HUD_BLINK
#define RELAY

#define LOGGING
#define DECOMP

#define CO2

#ifdef PRESSURE
#define PRESSURE_ADDR 0x77
MS_5803 psens = MS_5803(512);
bool gotms5803 =false;
#endif

#ifdef DECOMP
#include <VPMB.h>
double depthRead();
double pO2Read();
VPMB vpm(depthRead, pO2Read);


#endif

#ifdef CO2
SC16IS750 rmtSerial = SC16IS750(SC16IS750_PROTOCOL_I2C,SC16IS750_ADDRESS_AA);
NDIRZ16 co2sens = NDIRZ16(&rmtSerial);
bool gotco2 = false;
#define SC16IS750_ADDR (SC16IS750_ADDRESS_AA/2)
unsigned long co2start = 0;
int co2ppm = 0;
int co2state = 0;
char co2dispbuf[8];
#endif


#define ADS_ADDR ADS1015_ADDRESS

#define ADS_MV_PER_BIT_23  0.1875F //GAIN_TWOTHIRDS +/- 6.144V 
#define ADS_MV_PER_BIT_1  0.125F //GAIN_ONE +/- 4.096V
#define ADS_MV_PER_BIT_2 0.0625F //GAIN_TWO   +/- 2.048V 
#define ADS_MV_PER_BIT_4 0.03125F  // GAIN_FOUR  +/- 1.024V
#define ADS_MV_PER_BIT_8 0.015625F  // GAIN_EIGHT  +/- 0.512V 
#define ADS_MV_PER_BIT_16 0.0078125F  // GAIN_SIXTEEN  +/- 0.256V
#define ADS_MV_PER_BIT ADS_MV_PER_BIT_23
//double adsMvPerBit = ADS_MV_PER_BIT;
//double adsVoltRanges[] = {6.1440F, 4.096F, 2.048F, 1.024F, 0.512F, 0.256F};
//double adsMvPerBitRanges [] = {ADS_MV_PER_BIT_23, ADS_MV_PER_BIT_1, ADS_MV_PER_BIT_2, 
//                  ADS_MV_PER_BIT_4, ADS_MV_PER_BIT_8, ADS_MV_PER_BIT_16};
//long adsGains [] = {GAIN_TWOTHIRDS,GAIN_ONE, GAIN_TWO, GAIN_FOUR, GAIN_EIGHT, GAIN_SIXTEEN};
//double adsImps [] {1.0E7F, 6.0E6F, 6.0E6F, 3.0E6F,1.0E6F, 1.0E6F};
//double adsImp = adsImps[0];

#ifdef PRESSURE
float dbuf;
float pbuf;
float tbuf;
char ddispbuf[7];

bool saltwater = true; 

#define FWATER_D     1.019716f
#define SWATER_D     0.9945f
#endif

#ifdef DECOMP
float laststop, lasttime, stoptime, stopstart, ndtime;
char stopdispbuf[7];
char stoptimedisp[7];
char ndispbuf[7];
bool dostop = false;
int stopnum =0;

int nstops = 0;
bool dive_running = false;

#endif

float vbuf[3];
float po2buf[3];

float scales[] = {0.209F/12.0F, 0.209F/12.3F, 0.209F/12.16F};
float setpoints[] = {0.7F, 1.3F};
uint8_t currSetpoint = 0;


char vdispbuf [3][10];
char po2dispbuf [3][8];
int8_t setstate[3];  // 0 = on setpoint -3 fault -2 critical low -1 under +1 over +2 critical high  +3 overscale
char setdispbuf [2][6];
char caldispbuf[3][10];
char fo2dispbuf[3][8];

float setmin = 0.6F;
float setmax = 1.5F;
float absmax = 1.6F; 
float absmin = 0.18F;

float batt;
#ifdef LOGGING
char battBuf[10];
char tempBuf[10];
char pdispbuf[10];
#endif
bool gotAds = false; 

uint8_t currMenu = 1, lastMenu = 1;
uint8_t autopower = 0;
uint8_t autopower_ofs = 0;
uint8_t scalofs = 0;
uint8_t custofs = 0;
uint8_t spofs = 0;
uint8_t orienofs = 0;
uint8_t didsleep = 0;
uint8_t didsleep_ofs = 0;

uint8_t currCal = 0;
float calFrac [] = {0.209F, 1.00F, 0.1F};
const char cal1 [] PROGMEM = "Cal Air";
const char cal2 [] PROGMEM = "Cal O2";
const char cal3 [] PROGMEM = "Cal Custom";

const char * const calTxt[] U8X8_PROGMEM = {cal1, cal2, cal3};

unsigned long lastTone;
unsigned long lastToneIntvl;
int lastToneFreq;
bool silent = false;

unsigned long lastPulse;
#define DRAWBUF_SIZE 43

char drawbuf[DRAWBUF_SIZE];
char titlbuf[20];
#ifdef ARDUINO_ARCH_ESP8266
#define MENUBUF_SIZE 512
#endif
//char menubuf[MENUBUF_SIZE];
char * menubuf = NULL;
int menubuf_size = 0;

//u8g2_cb_t * orientation = U8G2_R0;

uint8_t orientation = 0;
const u8g2_cb_t * orientations [] = {U8G2_R0, U8G2_R1, U8G2_R2, U8G2_R3};
#define NUM_AUTOSCALE_RANGES 6


#ifdef ARDUINO_ARCH_ESP8266
//#define DISP_EN D1
#define DISP_EN_PCF 0
#define DISP_RST 33  //1
#define DISP_CS 34  //2
#define DISP_DC 35  //3
#define DISP_SEL 36  //4
#define DISP_NXT 37 //5
#define DISP_PRV 38 //6

//#define RELAY_PIN D2
#define RELAY_PIN 0
#define TONE_PIN D1
#define TONE_PIN_2 D2
#define SDA_PIN D3 
#define SCL_PIN D4
#define SCK D5
#define MISO D6
#define MOSI D7
#define PWR_PIN D0
//#define PWR_PIN 39

#define CLOCK_STRETCH 4000
#define I2C_SPEED 100000
#endif

static const char ver [] PROGMEM  = "OxyGauge 1.0.20";


#ifdef ARDUINO_ARCH_ESP8266
U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI disp(U8G2_R0, /* cs=*/ DISP_CS, /* dc=*/ DISP_DC, /* reset=*/ DISP_RST);  // Enable U8G2_16BIT in u8g2.h
#define FIRST_PAGE(a) disp.clearBuffer(a)

#define NEXT_PAGE(a) ( disp.sendBuffer(a), false)
#ifdef HUD
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C disp2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
#define HUD_FIRST_PAGE(a) disp2.clearBuffer(a)

#define HUD_NEXT_PAGE(a) ( disp2.sendBuffer(a), false)
#endif 
#endif



#define EXPANDER_ADDR 0x20

#ifdef HUD_BLINK
#define HUD_ADDR 0x8
#define RED 0x2
#define GREEN 0x1
#define ORN 0x3
bool got_hud = false;

#define BLINK_CYC 5000
#define BLINK_INT 400
#define BLINK_DUR 150

#endif 

#ifdef RELAY

#ifdef ARDUINO_ARCH_ESP8266
#ifdef RELAY_PCF
   PCF8574 PCFr;
   bool gotpcf2 = false;
   #define EXPANDER2_ADDR 0x22
#endif

   bool got_relay = false;
   #define RELAY_ADDR 0x30

#endif 

#define WINDOW_SIZE 5000


double setpointv = 0.0;
double inputval;
double outputval;
uint8_t relayOn = 0;
unsigned long last_relay_reset = 0;
uint8_t relay_did_window = 0;
bool forcerelay = false;
double forcedvalue = 4000.0;
char sensel[4];
char po2seldisp[8];

unsigned long windowStartTime;
//double kp=2,ki=0.5,kd=2;
double kp=2,ki=5,kd=1;

PID myPID(&inputval, &outputval, &setpointv, kp, ki, kd, DIRECT);
PID_ATune aTune(&inputval, &outputval);
boolean tuning = false;
  byte ATuneModeRemember=2;
double aTuneStep=100, aTuneNoise=1, aTuneStartValue=100;
unsigned int aTuneLookBack=20;

#endif

#ifdef ARDUINO_ARCH_ESP8266
DS3232RTC dsRTC;
bool gotrtc = false;

Adafruit_BME280 bme = Adafruit_BME280(); // I2C
#define BME_ADDRESS 0x76

bool dontsave = false;

NTP myNTP(&dsRTC);

#endif


Adafruit_ADS1115 ads; 

#define battery_width 14
#define battery_height 9
static const unsigned char battery0_bits[] U8X8_PROGMEM = {
   0xff, 0x0f, 0x01, 0x08, 0x01, 0x38, 0x01, 0x28, 0x01, 0x28, 0x01, 0x28,
   0x01, 0x38, 0x01, 0x08, 0xff, 0x0f };

static const unsigned char battery3_bits[] U8X8_PROGMEM = {
   0xff, 0x0f, 0x01, 0x08, 0x05, 0x38, 0x05, 0x28, 0x05, 0x28, 0x05, 0x28,
   0x05, 0x38, 0x01, 0x08, 0xff, 0x0f };

static const unsigned char battery4_bits[] U8X8_PROGMEM = {
   0xff, 0x0f, 0x01, 0x08, 0x0d, 0x38, 0x0d, 0x28, 0x0d, 0x28, 0x0d, 0x28,
   0x0d, 0x38, 0x01, 0x08, 0xff, 0x0f };

static const unsigned char battery5_bits[] U8X8_PROGMEM = {
   0xff, 0x0f, 0x01, 0x08, 0x1d, 0x38, 0x1d, 0x28, 0x1d, 0x28, 0x1d, 0x28,
   0x1d, 0x38, 0x01, 0x08, 0xff, 0x0f };

static const unsigned char battery6_bits[] U8X8_PROGMEM = {
   0xff, 0x0f, 0x01, 0x08, 0x3d, 0x38, 0x3d, 0x28, 0x3d, 0x28, 0x3d, 0x28,
   0x3d, 0x38, 0x01, 0x08, 0xff, 0x0f };

static const unsigned char battery7_bits[] U8X8_PROGMEM = {
   0xff, 0x0f, 0x01, 0x08, 0x7d, 0x38, 0x7d, 0x28, 0x7d, 0x28, 0x7d, 0x28,
   0x7d, 0x38, 0x01, 0x08, 0xff, 0x0f };

static const unsigned char battery8_bits[] U8X8_PROGMEM = {
   0xff, 0x0f, 0x01, 0x08, 0xfd, 0x38, 0xfd, 0x28, 0xfd, 0x28, 0xfd, 0x28,
   0xfd, 0x38, 0x01, 0x08, 0xff, 0x0f };

static const unsigned char battery9_bits[] U8X8_PROGMEM = {
   0xff, 0x0f, 0x01, 0x08, 0xfd, 0x39, 0xfd, 0x29, 0xfd, 0x29, 0xfd, 0x29,
   0xfd, 0x39, 0x01, 0x08, 0xff, 0x0f };

static const unsigned char batteryf_bits[] U8X8_PROGMEM = {
   0xff, 0x0f, 0x01, 0x08, 0xfd, 0x3b, 0xfd, 0x2b, 0xfd, 0x2b, 0xfd, 0x2b,
   0xfd, 0x3b, 0x01, 0x08, 0xff, 0x0f };

#define OxyTitle_width 118
#define OxyTitle_height 22
static const unsigned char OxyTitle_bits[] U8X8_PROGMEM = {
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x78, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 
  0x00, 0xC0, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x70, 0x00, 0xE0, 0x01, 0x00, 0x7E, 0x00, 0x00, 0xC0, 0x6F, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0xF8, 0x03, 0x80, 0xFF, 0x00, 
  0x00, 0xE0, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x74, 0x00, 0x3C, 0x02, 
  0x80, 0xCF, 0x00, 0x00, 0xF0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 
  0x00, 0x0E, 0x03, 0xC0, 0xC3, 0x00, 0x00, 0x78, 0x20, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x38, 0x00, 0x06, 0x02, 0xE0, 0xC1, 0x18, 0x00, 0x38, 0x70, 
  0x30, 0x00, 0xC1, 0x00, 0x00, 0x38, 0x00, 0x07, 0x03, 0xF0, 0x40, 0x9C, 
  0x9C, 0x1C, 0x38, 0x3E, 0x98, 0xF1, 0xE0, 0x01, 0x1C, 0x80, 0x83, 0x01, 
  0x70, 0x60, 0x5E, 0xCE, 0x0C, 0x1C, 0x2F, 0xCC, 0x39, 0xB1, 0x01, 0x0C, 
  0x80, 0x81, 0x01, 0x70, 0x60, 0x3A, 0xE7, 0x0E, 0x9F, 0x33, 0xEE, 0x9C, 
  0xDB, 0x00, 0x0E, 0x80, 0xC1, 0x00, 0x30, 0x30, 0x18, 0x73, 0x86, 0xCD, 
  0x39, 0x73, 0xCE, 0x6D, 0x00, 0x0E, 0xC0, 0xE1, 0x00, 0x30, 0x38, 0x98, 
  0x7B, 0xE5, 0xCC, 0x1C, 0x7B, 0xE6, 0x1D, 0x00, 0x0E, 0x80, 0x71, 0x00, 
  0x30, 0x1C, 0xDC, 0xBF, 0x3C, 0xD6, 0x9E, 0xEF, 0xB7, 0x0E, 0x01, 0x06, 
  0x82, 0x39, 0x00, 0x20, 0x0F, 0x7A, 0x57, 0x1C, 0xCF, 0x7B, 0xFF, 0xDE, 
  0xFD, 0x00, 0x06, 0x83, 0x1F, 0x00, 0xE0, 0x03, 0x3B, 0x7A, 0x00, 0x87, 
  0x39, 0x62, 0xCC, 0x78, 0x00, 0x04, 0x01, 0x0F, 0x00, 0x00, 0x00, 0x00, 
  0x38, 0x80, 0x03, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x1C, 0xC0, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0xE0, 0x00, 0x00, 0x00, 0x18, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x70, 0x00, 
  0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x06, 0x38, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, };
static unsigned char const  * const batteries[] PROGMEM = {battery0_bits, battery3_bits, battery4_bits, 
                    battery5_bits, battery6_bits, battery7_bits, battery8_bits,
                    battery9_bits, batteryf_bits};
uint8_t battIdx = 0;

        

#ifdef ARDUINO_ARCH_ESP8266
            
#ifdef DECOMP
uint8_t * newLogo = NULL;
unsigned long newLogoSize = 0;
int newLogo_width = OxyTitle_width;
int newLogo_height = OxyTitle_height;
#endif

#define wifi_width 14
#define wifi_height 9
static const unsigned char wifi0_bits[]  U8X8_PROGMEM = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x03, 0x00 };

static const unsigned char wifi1_bits[]  U8X8_PROGMEM = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x18, 0x00, 0x18, 0x00, 0x1b, 0x00 };

static const unsigned char wifi2_bits[] U8X8_PROGMEM = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0xc0, 0x00,
   0xd8, 0x00, 0xd8, 0x00, 0xdb, 0x00 };

static const unsigned char wifi3_bits[] U8X8_PROGMEM = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x06, 0xc0, 0x06, 0xc0, 0x06,
   0xd8, 0x06, 0xd8, 0x06, 0xdb, 0x06 };
static const unsigned char wifi4_bits[] U8X8_PROGMEM = {
   0x00, 0x30, 0x00, 0x30, 0x00, 0x36, 0x00, 0x36, 0xc0, 0x36, 0xc0, 0x36,
   0xd8, 0x36, 0xd8, 0x36, 0xdb, 0x36 };

static unsigned char const  * const wifis[] PROGMEM = {wifi0_bits, wifi1_bits, wifi2_bits, 
                    wifi3_bits,  wifi4_bits};
uint8_t wifiIdx = 0;
         
#endif

               
#define fatx_width 16
#define fatx_height 19
static const unsigned char fatx_bits[] U8X8_PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x3C, 0x3C, 0x7E, 0x7F, 0xFF, 0xFF, 
  0xFE, 0x7F, 0xFC, 0x3F, 0xF8, 0x1F, 0xF8, 0x0F, 0xF8, 0x1F, 0xFC, 0x3F, 
  0xFF, 0x7F, 0xFF, 0xFF, 0x7E, 0x7E, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x00, 
  0x00, 0x00,
  };
#define uparrow_width 16
#define uparrow_height 19
static const unsigned char uparrow_bits[] U8X8_PROGMEM = {
  0x00, 0x00, 0x80, 0x00, 0xC0, 0x01, 0xE0, 0x03, 
  0xF0, 0x07, 0xF8, 0x0F, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 
  0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 
  0x80, 0x01, 0x80, 0x01, 0x00, 0x00, 
  };
#define downarrow_width 16
#define downarrow_height 19
static const unsigned char downarrow_bits[] U8X8_PROGMEM = {
  0x00, 0x00, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01,
  0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 
  0x80, 0x01, 0xF8, 0x0F, 0xF0, 0x07, 0xE0, 0x03, 0xC0, 0x01, 0x80, 0x00, 
  0x00, 0x00, 
  };
#define check_width 16
#define check_height 19
static const unsigned char check_bits[] U8X8_PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x0C, 
  0x00, 0x04, 0x00, 0x06, 0x00, 0x03, 0x00, 0x03, 0x80, 0x01, 0xC0, 0x00, 
  0xC6, 0x00, 0x6E, 0x00, 0x38, 0x00, 0x38, 0x00, 0x10, 0x00, 0x00, 0x00,  
  0x00, 0x00,0x00, 0x00,};

static const char titleOri[] PROGMEM = "Orientation";
static const char titleCust[] PROGMEM =  "Custom Calibration";
static const char titleCalConf[] PROGMEM = "Confirm Calibration";
static const char titleOptions[] PROGMEM = "Options";
static const char titleSetpoints[] PROGMEM ="Setpoints";
static const char titleRemote[] PROGMEM ="Remote";
static const char titleGasList[] PROGMEM ="Gaslist";


#ifdef ARDUINO_ARCH_ESP8266
static const char menuOptions[] PROGMEM = "PO2 Display\nGaslist\nSetpoints\nCalibration\nOrientation\nRemote\nVPM Test Dive\nRelay\nSilent\nInfo\nOff";
static const char menuRemote[] PROGMEM = "WiFi\nWiFi AP\nDisconnect WiFi\nScan\nDelete Dive Logs";
static const char menuRelay[] PROGMEM = "On\nOff\nPID\nAutoTune\nExit";
static const char titleVPM[] PROGMEM ="VPM-B Test";
static const char titleRelay[] PROGMEM ="Relay";
static const char titleWiFiSel[] PROGMEM ="Network Selection";


#endif
static const char menuOri[] PROGMEM = "Normal\nFlipped\nVertical\nVertical Flipped";

#ifdef ARDUINO_ARCH_ESP8266

bool got_wifi = false;

const char * syslogHost = NULL; //"192.168.29.107";

#endif

void sleepNow() {
#ifdef ARDUINO_ARCH_AVR
   attachInterrupt(0,wakeUpNow, LOW); 
   LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); 
   detachInterrupt(0); 
#endif
#ifdef ARDUINO_ARCH_ESP8266
  ESP.deepSleep(1e8); // 20e6 is 20 seconds

#endif
}


void getPO2(void) {
  for (int c = 0; c < 3; c++){
    if (gotAds) {
      vbuf[c] = ads.readADC_Differential_3(c) * ADS_MV_PER_BIT_16;
      po2buf[c] = vbuf[c] * scales[c];
      po2buf[c] = fabs(po2buf[c]);
    } else {
      vbuf[c] = 0.0F;
      po2buf[c] = 0.0F;
    }
  }
}

#ifdef PRESSURE
#define SEALEVELPRESSURE_HPA_STD (1013.25f)

int test_ready = 0;
unsigned long last_test = 0;
unsigned long curr_test = 0;
unsigned long test_started = 0;

/* typedef struct _segment { 
  double rate; //asc or desc
  double level;
  double stime; //including leading asc/desc
} dive_segment;
*/
//const int nsegs = 3;

//dive_segment segments[nsegs] = {{28.0, 80.5, 10.0},{10.0, 37.0, 15.0 },{10.0, 0.0, 0.0}}; 
int curr_segment =0;
double seg_start_time = 0.0;
double asc_start_time = 0.0;
double asc_start_depth = 0.0;
int curr_ascent = 0;
double depthRead() {
  //double rate;
  last_test = curr_test;
  curr_test = millis();
  //DBG_PRINT("test_started: ");
  //DBG_PRINT("%s\n",test_started);
  //DBG_PRINT("curr: ");
  //DBG_PRINT("%d\n",curr_test);
  
  //DBG_PRINT(elapsed);
  //  DBG_PRINT(" Samps: ");
//
  //  DBG_PRINT("%d\n",elapsed_samps);

  //double ret = 0.0;
  if(gotms5803 && !test_ready) {
   psens.readSensor();

    pbuf = psens.pressure();
    tbuf = psens.temperature();
    
    dbuf  = ( pbuf - SEALEVELPRESSURE_HPA_STD) * (saltwater?SWATER_D :FWATER_D) / 100.0f;
    test_started = curr_test;
    curr_segment = 0;
    curr_ascent = 0;
    seg_start_time = 0.0;
    asc_start_time = 0.0;
  } else  if (!test_ready) {
      pbuf = 0.0;
      tbuf = 0.0;
      dbuf = 0.0;
      test_started= curr_test;
      curr_segment = 0;
      curr_ascent = 0;
      seg_start_time = 0.0;
      asc_start_time = 0.0;
      asc_start_depth = 0.0;
  }  else {
      unsigned long elapsed = curr_test - test_started;
      double elapsed_mins = ((double)elapsed) /60000.0;
      dive_profile * profs = vpm.getTestDiveProfiles();
      int nsegs = vpm.getNumberOfTestDiveProfiles();

      //dive_segment * seg = &(segments[curr_segment]);
      dive_profile *seg = &(profs[curr_segment]);
      switch (seg->profile_code) {
         case 1:
           dbuf = seg->starting_depth + (elapsed_mins - seg_start_time)* seg->rate;
           if ((dbuf >=  seg->ending_depth && seg->rate > 0.0 )
              || (dbuf <= seg->ending_depth && seg->rate < 0.0)) {
                dbuf = seg->ending_depth;
                curr_segment++;
                if(curr_segment >= nsegs) {
                  ERR_PRINT("Ran out of dive sim segments");
                  test_ready = 0;
                  return 0.0;
                }
                seg_start_time = elapsed_mins;
                asc_start_time = seg_start_time;
                DBG_PRINT("New dive seg %d\n", curr_segment);

           }
         break;
         case 2:
          dbuf = seg->depth;
          if(elapsed_mins > seg->run_time_at_end_of_segment) {
            //if ((rate >= 0.0 && dbuf >= seg->level) || (rate <  0.0 && dbuf < seg->level)) dbuf =seg->level;
            curr_segment++;
            if(curr_segment >= nsegs) {
              ERR_PRINT("Ran out of dive sim segments");
              test_ready = 0;
              return 0.0;
            }
            seg_start_time = elapsed_mins;
            asc_start_time = seg_start_time;

            DBG_PRINT("New dive seg %d\n", curr_segment);
          } 
          break;
        case 99:
            {
            int nascents = seg->number_of_ascent_parameter_changes;
            ascent_summary * asc = &(seg->ascents[curr_ascent]);
            if (asc_start_depth == 0.0) asc_start_depth = asc->starting_depth;
            if (dostop) {
              dbuf = laststop;
              asc_start_depth = laststop;
              asc_start_time = elapsed_mins;
            } else {
              dbuf = asc_start_depth + (elapsed_mins - asc_start_time)* asc->rate;
            }
            //set gasmix here...
            if (curr_ascent < nascents -1) {
               if (dbuf <= seg->ascents[curr_ascent + 1].starting_depth) {
                  curr_ascent++;
                  asc = &(seg->ascents[curr_ascent]);
                  dbuf = asc->starting_depth;
                  asc_start_time = elapsed_mins;
                  asc_start_depth = asc->starting_depth;

               }
            } else {
              if (dbuf <= 0.0) {

                test_ready = 0;
                dbuf = 0.0;
                //lprintf("C,done\n"
              }
            }
           }
           break;
         default:
          break;
      }
      //if(seg->ending_depth < seg_start_depth) rate = -seg->rate;
      //else rate = seg->rate;
      
      /* if (((rate >= 0.0 && dbuf >= seg->level) || (rate <  0.0 && dbuf < seg->level))
          && (elapsed_mins - seg_start_time ) < seg->stime) dbuf = seg->level;
      if (seg->stime == 0.0 && dbuf <= 0.0) {
        test_ready = 0;
        return 0.0;
      } else if (seg->stime > 0.0) {
        if(elapsed_mins - seg_start_time > seg->stime) {
            if ((rate >= 0.0 && dbuf >= seg->level) || (rate <  0.0 && dbuf < seg->level)) dbuf =seg->level;
            curr_segment++;
            if(curr_segment >= nsegs) {
              ERR_PRINT("Ran out of dive sim segments");
              test_ready = 0;
              return 0.0;
            }
            seg_start_time = elapsed_mins;
            seg_start_depth = dbuf;
            DBG_PRINT("New dive seg %d, start %f, depth %f\n", curr_segment, seg_start_time, seg_start_depth);
        } 
      }*/
          //if (dbuf >= bottom && elapsed_samps < ((bottom_time*60.0)/samp_intvl)) dbuf = bottom;
          // else if (elapsed_samps > ((bottom_time*60.0)/samp_intvl))
          //  dbuf = bottom - ((elapsed_samps - (bottom_time*60)/samp_intvl) * (asc_rate/(60.0/samp_intvl)));
    //if (test_ready) testcount++;
   }
   return dbuf;
}

double pO2Read(void) {

  return inputval;
}

void getDepth(void) {
    if(!gotms5803 && !test_ready) {
      dbuf=0.0;
      pbuf = 0.0;
      tbuf= 0.0;
      strcpy(ddispbuf, "Xm");
#ifdef LOGGING
        strcpy(tempBuf, "X");
#endif  
      return;
    }
    TRC_PRINT("getDepth\n");
    //Serial.flush();
    depthRead();
    dtostrf(dbuf, 3, 1, ddispbuf);
    dotrim (ddispbuf);
  
    strcat(ddispbuf, "m");
#ifdef LOGGING
    dtostrf(tbuf, 3, 1, tempBuf);
    dtostrf(pbuf, 8,0, pdispbuf);
    dotrim (pdispbuf);

#endif
}

#endif 

#ifdef DECOMP

bool doingND = false;
void getND(void) {
  if (!doingND) {
      
    doingND = true;
    if (nstops == 0) {
      ndtime = vpm.getNDTime();
    } else {
      ndtime = 0.0;
    }
    dtostrf(ndtime, 3, 0, ndispbuf);
    dotrim (ndispbuf);
    doingND = false;
  }
}

bool doStops = false;
void getStops(void) {
  TRC_PRINT("stops\n");
  if(!doStops) {
    doStops = true;
    nstops = vpm.getActualDecoStops();
    laststop = vpm.getNextStopDepth(dbuf);
    lasttime = vpm.getNextStopTime(dbuf);
    stopnum = vpm.getNextStopNum(dbuf);
    double diveTime = vpm.getDiveTime();

    //dtostrf(laststop, 3, 0, stopdispbuf);
    //dtostrf(lasttime, 3, 2, stoptimedisp);
   
    //dotrim (stopdispbuf);
    //strcat(stopdispbuf, "m");

    double stopremtime;
    if((dive_running && nstops > 0 && (fabs(dbuf -laststop) <0.3)) || dostop) {
      
      if (!dostop) stopstart = diveTime;
       dostop = true;
      stopremtime = lasttime - (diveTime - stopstart);
      if (stopremtime <= 0.0) {
        dostop = false;
        stopstart = 0.0;
        stopremtime = lasttime;
      }
    } else {
      dostop = false;
      stopstart = 0.0;
      stopremtime = lasttime;
    }
    
    dtostrf(stopremtime, 3, 1, stoptimedisp);

    doStops = false;
  }
  TRC_PRINT("stops done\n");

}
#endif

void getBatt() {
#ifdef ARDUINO_ARCH_ESP8266
   batt = ((float)analogRead(A0)) *4.20F/1024.0;
       //DBG_PRINT("%f\n",batt);
#ifdef LOGGING
    dtostrf(batt, 4, 2, battBuf);
#endif
   batt = batt - 2.75F;
   if (batt < 0.0) batt = 0.0f;
   batt = ( batt / (4.20f -2.75f )) * 100.0;
#endif
    //DBG_PRINT("%f\n",batt);

    //floatConv(battBuf, 10, batt, 0,0);
    battIdx = (int)(batt / 11.1f);
    if (battIdx > 8) battIdx = 8;
        //DBG_PRINT("%d\n",battIdx);

}

void convPO2() {    
   //int sz, sz2;
  for (int c = 0; c < 3; c++){
    dtostrf(vbuf[c], 7, 2, vdispbuf[c]);

    //sz = floatConv(vdispbuf[c], 10, vbuf[c], 2, 1);
    strcat(vdispbuf[c], "mV");

    //sz2 = floatConv(po2dispbuf[c], 10, po2buf[c], 2, 0);
    dtostrf(po2buf[c], 5, 2, po2dispbuf[c]);
    dotrim(po2dispbuf[c]);
    if (gotAds == 0 || vbuf[c] < 1.5f) {
      setstate [c] = -3;
    } else if(vbuf[c] > 200.0f) {
            setstate [c] = 3;

    } else if (po2buf[c] > absmax) {

      setstate [c] = 2;
    } else  if (po2buf[c] < absmin){
            setstate [c] = -2;
    } else {
      if ((po2buf[c] - setpoints[currSetpoint]) > 0.1F)
        setstate [c] = 1;
       else if ((setpoints[currSetpoint] - po2buf[c]) > 0.1F) 
        setstate[c] = -1;
       else 
        setstate[c] = 0; 
    }
  }
}


void convFO2() {    
   //int sz;
  for (int c = 0; c < 3; c++){
    dtostrf(vbuf[c], 7, 2, vdispbuf[c]);

    //sz = floatConv(vdispbuf[c], 10, vbuf[c], 2, 1);
    strcat(vdispbuf[c], "mV");
    dtostrf(po2buf[c]*100.0, 6, 1, fo2dispbuf[c]);
    //dotrim(fo2dispbuf[c]);

    //sz = floatConv(fo2dispbuf[c], 10, po2buf[c]*100.0, 1,0);
    strcat(fo2dispbuf[c], "%");
  }
}

void buildSP(void) {
   // int sz;
  for (int c = 0; c < 2; c++){
        dtostrf(setpoints[c], 4, 1, setdispbuf[c]);
        dotrim(setdispbuf[c]);
    //sz = floatConv(setdispbuf[c], 10, setpoints[c], 1, 0);
  }
#ifdef RELAY
  setpointv=setpoints[currSetpoint];
#endif
}



void buildCal(void) {
   //int sz;
  for (int c = 0; c < 3; c++){
    dtostrf((calFrac[c] * 100.0f), 6, 1, caldispbuf[c]);

    //sz = floatConv(caldispbuf[c], 10, calFrac[c] * 100, 1, 0);
  }
}

void titleDisp(){
    //DBG_PRINT("%s\n","tstart");

  disp.setFontRefHeightExtendedText();
  disp.setFontMode(0);
  disp.setDrawColor(1);
  disp.setFontPosTop();
  disp.setFontDirection(0);
  disp.setIntensity(0xff);
  //disp.setFont(u8g2_font_inb24_mr);
  //disp.setFont(u8g2_font_inr19_mr); //DBG_PRINT("%s\n","tmid");

  disp.drawRBox(5, 5, 245, 50, 8);
  disp.setDrawColor(0);
  if(newLogo) {
    //for (int z = 0; z < newLogoSize; z++) {
    //  DBG_PRINT(newLogo[z], HEX);
     // DBG_PRINT(",");
    //  if (z % 8 == 0) {
    //    DBG_PRINT("\n");
    //  }
    disp.drawXBM((256 - newLogo_width) / 2, (64 - newLogo_height) / 2, newLogo_width, newLogo_height, newLogo);
    free(newLogo);
    newLogo = NULL;
  } else {
      disp.drawXBMP((256 - OxyTitle_width) / 2, (64 - OxyTitle_height) / 2, OxyTitle_width, OxyTitle_height, OxyTitle_bits);
  }
  //disp.drawStr(16, (64/2) - (24/2), ver);
  //DBG_PRINT("%s\n","tend");
}

#ifdef HUD
void hudtDisp(){
  
  disp2.setFontRefHeightExtendedText();
  disp2.setFontMode(0);
  disp2.setDrawColor(1);
  disp2.setFontPosTop();
  disp2.setFontDirection(0);
  //disp.setIntensity(0xff);
  //disp2.setFont(u8g2_font_inr19_mr);
    disp2.setFont(u8g2_font_t0_11b_mr);  
   //disp2.drawStr((128/2) - ((3*19)/2), (32/2) - (19/2), "HUD");
   disp2.drawStr((128/2) - ((3*11)/2), (32/2) - (11/2), "HUD");

}

int huddisp(){
  int w, h, xofs, tm = 0, t, colr;
  const unsigned char * bmp;
  float sdisp;
  char hdisbuf[3];
  
  disp2.setFontRefHeightExtendedText();
  disp2.setFontMode(0);
  disp2.setDrawColor(1);
  disp2.setFontPosTop();
  disp2.setFontDirection(0);
  //disp.setIntensity(0xff);
  //disp2.setFont(u8g2_font_inr19_mr);
  disp2.setFont(u8g2_font_t0_11b_mr);  

  //disp2.drawStr((128/2) - ((3*19)/2), (32/2) - (19/2), "HUD");
  for (int c = 0; c < 3; c++) {
    getbmp(c, &bmp, &w, &h, &colr);
    disp2.setDrawColor(colr);

    //int xp = 0;
    //if (vert) xp = (wid/2) - (w/2);
    disp2.drawXBMP((c * (128/3)) + 5, (32/2) - (h/2), w, h, bmp);
    sdisp = (po2buf[c] - setpoints[currSetpoint])*10.0f;
    dtostrf(sdisp, 2, 0, hdisbuf);
    disp2.setDrawColor(1);

    disp2.drawStr((c * (128/3)) + 6 + w,  (32/2) - (11/2), hdisbuf);
    //t = disp.getMenuEvent();
    //if (t != 0) tm = t;
  }
  if(costate > 1) {
      char hco2buf [20];
      snprintf (hco2buf, 20, "CO2%d", co2state);
      disp2.setDrawColor(0);
      disp2.drawStr((128/2) - ((3*19)/2), (32/2) - (19/2), hco2buf);
      disp2.setDrawColor(1);

  }
  return tm;
}

#endif 

void drawBatt(int wid, int y) {
#ifdef ARDUINO_ARCH_ESP8266
    disp.drawXBMP(wid - ( battery_width + 5), y +1, battery_width, battery_height, (const unsigned char*)pgm_read_dword(&(batteries[battIdx])));
#endif
}



void getbmp(int c, const unsigned char ** bmp, int * w, int *h, int * clr) {
  
      *bmp = fatx_bits;
      *w = fatx_width;
      *h = fatx_height;
      
    if (setstate[c] == 0) {
      *clr = 1;
      *bmp = check_bits;
      *w =check_width;
      *h = check_height;
    } else if (setstate[c] == -1) {
      *clr = 1;
      *bmp = uparrow_bits;
      *w = uparrow_width;
      *h = uparrow_height;
    } else if (setstate[c] == 1) {
      *clr = 1;
       *bmp = downarrow_bits;
      *w = downarrow_width;
      *h = downarrow_height;
    }  else if (setstate[c] == -2) {
      *clr = 0;
      *bmp = uparrow_bits;
      *w = uparrow_width;
      *h = uparrow_height;
    } else if (setstate[c] == 2) {
      *clr = 0;
       *bmp = downarrow_bits;
      *w = downarrow_width;
      *h = downarrow_height;
    }else if (setstate[c] == -3 || setstate[c] == 3) {
      *clr = 0;
      *bmp = fatx_bits;
      *w = fatx_width;
      *h = fatx_height;
    }
}

int po2disp(int vert) {
  int w, h, t, tm =0;
  int wid, ht, yofs = 0, colr;
  const unsigned char * bmp;
  if (vert) {
    wid = 64;
    ht = 256;
  } else {
    wid = 256;
    ht = 64;
  }
    //disp.clear();
   //TRC_PRINT("%s\n","disp1");

  //disp.setFont(u8x8_font_chroma48medium8_r);  
  disp.setFontRefHeightExtendedText();
  disp.setFontMode(0);
  disp.setDrawColor(0);
  disp.setFontPosTop();
  disp.setFontDirection(0);
  disp.setFont(u8g2_font_t0_11b_mr);  
  //DBG_PRINT("%s\n","disp2");


  //disp.setCursor(0,8);
  /* all graphics commands have to appear within the loop body. */    

  if ((millis() / 1000) % 2 == 0) disp.drawStr(2,yofs, dive_running ? " PO2d" : " PO2.");
  else  disp.drawStr(2,yofs, " PO2 ");
  //TRC_PRINT("%s\n","disp3");

  disp.setDrawColor(1);
  drawBatt(wid,yofs);
#ifdef ARDUINO_ARCH_ESP8266
   drawWifi(wid, yofs);
#endif
        //DBG_PRINT("%s\n","dispposba");

  if (!vert) {
#ifdef PRESSURE
#ifdef DECOMP
      if(!dostop) {
        if (nstops > 0) snprintf(drawbuf, DRAWBUF_SIZE, "%3.1fm S%d N%3.0fm", dbuf, nstops, laststop);
        else snprintf(drawbuf, DRAWBUF_SIZE, "%s ND%s", ddispbuf, ndispbuf);
        disp.drawStr ((wid/6) -2, yofs, drawbuf);
      } else {
          snprintf(drawbuf, DRAWBUF_SIZE, "%3.1fm ", dbuf);
          int strwid = disp.getStrWidth(drawbuf);
          disp.drawStr ((wid/6) -2, yofs, drawbuf);
          snprintf(drawbuf, DRAWBUF_SIZE, "S%d T%s", stopnum, stoptimedisp);

          disp.setDrawColor(0);
          disp.drawStr ((wid/6) -2 + strwid, yofs, drawbuf);
          disp.setDrawColor(1);
      }
#else
        snprintf(drawbuf, DRAWBUF_SIZE, "%3.1fm ", dbuf);

        disp.drawStr ((wid/6) -2 , yofs, drawbuf);
#endif
#endif
#ifdef RELAY
    if (co2state > 1) {
      
      snprintf(drawbuf, DRAWBUF_SIZE, "SP%d p%s CO2%d %c", currSetpoint + 1, setdispbuf[currSetpoint], co2state, relayOn ? '+' : ' ');
    } else {
      snprintf(drawbuf, DRAWBUF_SIZE, "SP%d p%s S%s %c", currSetpoint + 1, setdispbuf[currSetpoint], sensel, relayOn ? '+' : ' ');
     }
    disp.drawStr ((wid/6)+ 4 + (wid/3) , yofs, drawbuf);
#else
    if (co2state > 1) {
        snprintf(drawbuf, DRAWBUF_SIZE, "SP%d p%s CO2%d", currSetpoint + 1, setdispbuf[currSetpoint], co2state);
    } else {
      snprintf(drawbuf, DRAWBUF_SIZE, "SP%d p%s", currSetpoint + 1, setdispbuf[currSetpoint]);
    }
    disp.drawStr ((wid/6) +4 + (wid/3) +  (wid/6) + (wid/12), yofs, drawbuf);
#endif
  } else {
#ifdef PRESSURE
    yofs += 12;
#ifdef DECOMP
      if(!dostop) {
            if (nstops > 0)  { 
              snprintf(drawbuf, DRAWBUF_SIZE, "%s S%d", ddispbuf, nstops);  
              disp.drawStr (2 , yofs, drawbuf);
              yofs += 12;
              snprintf(drawbuf, DRAWBUF_SIZE, "NS %s", stopdispbuf);
              disp.drawStr (2 , yofs, drawbuf);
            } else {
              snprintf(drawbuf, DRAWBUF_SIZE, "%s", ddispbuf);  
              disp.drawStr (2 , yofs, drawbuf);
              yofs += 12;
              snprintf(drawbuf, DRAWBUF_SIZE, "ND %s", ndispbuf);
              disp.drawStr (2 , yofs, drawbuf);
            }
      } else {
        snprintf(drawbuf, DRAWBUF_SIZE, "%s", ddispbuf);  
        disp.drawStr (2 , yofs, drawbuf);
        yofs += 12;
        snprintf(drawbuf, DRAWBUF_SIZE, "S%d T%s", stopnum, stoptimedisp);
        disp.setDrawColor(0);
        disp.drawStr (2 , yofs, drawbuf);
         
        disp.setDrawColor(1);
      }
#else
    disp.drawStr (2, yofs, ddispbuf);
#endif
#endif

    yofs += 12;
    snprintf(drawbuf, DRAWBUF_SIZE, "SP%d p%s", currSetpoint + 1, setdispbuf[currSetpoint]);
    //disp.drawStr (2, yofs, drawbuf);
    //yofs += 12;
    //snprintf(drawbuf, DRAWBUF_SIZE, "PO2 %s", setdispbuf[currSetpoint]);
    disp.drawStr (2, yofs, drawbuf);
 #ifdef RELAY
    yofs += 12;
    if (co2state > 1) {
        snprintf(drawbuf, DRAWBUF_SIZE, "CO2%d %c", co2state, relayOn ? '+' : ' ');
    } else {
       snprintf(drawbuf, DRAWBUF_SIZE, "S%s %c", sensel, relayOn ? '+' : ' ');
    }
    disp.drawStr (2, yofs, drawbuf);
 #endif
  }
  yofs += 14;
  //TRC_PRINT("preEvent\n");
  t = disp.getMenuEvent();
   // TRC_PRINT("postEvent\n");

  if (t != 0) tm = t;

  //disp.setFont(u8g2_font_t0_11b_mf);  
  //disp.clearLine(2);
  //disp.setFont(u8g2_font_inr19_mr);

  int xofs;
  if (vert) xofs = 0;
  else xofs = 5;
  disp.drawHLine(0, yofs -1, wid);
  if (vert) {
    yofs += 3;
  } else {
    yofs += 5;
  }
  for (int c = 0; c < 3; c++){
    disp.setFont(u8g2_font_t0_11b_mr);  

    //disp.setFont(u8g2_font_7x14B_mr);
    disp.setDrawColor(1);
    disp.drawStr (xofs, yofs, vdispbuf[c]); 
    disp.setFont(u8g2_font_inr19_mn);
    getbmp(c, &bmp, &w, &h, &colr);
    disp.setDrawColor(colr);

    int xp = 0, yp=0;
    if (vert) {
      xp = (wid/2) - (w/2);
      yofs += 16;
      yp = 0;
    } else {
      yp = 19;
    }
    disp.drawXBMP(xofs + xp, yofs +yp, w, h, bmp);
    if (vert) yofs += h+2;
    if (setstate[c] == 2 || setstate[c] == -2) {
      disp.setDrawColor(0);
    } else {
       disp.setDrawColor(1);
    }
    if(!vert) {
       xp = w;
    } else {
      xp = 0;
    }
    disp.drawStr(xofs +xp, yofs + yp, po2dispbuf[c]); 
    if (!vert) {
      xofs += wid/3;
      disp.drawVLine(xofs -4, yofs -6, ht - (yofs - 6));
    }
    else {
      yofs += 27; //(ht/3) - (ht/8);
      disp.drawHLine(0, yofs - 3, wid);

    }
      //TRC_PRINT("preEvent2\n");

    t = disp.getMenuEvent();
          //TRC_PRINT("postEvent2\n");

    if (t != 0) tm = t;
  }
  return tm;
}


int caldisp() {
  const int wid = 256;
    const int ht = 64;

  int t, tm =0;
  int yofs = 0;
  int xofs = 0;

    //disp.clear();
  //disp.setFont(u8x8_font_chroma48medium8_r);  
  disp.setFontRefHeightExtendedText();
  disp.setFontMode(0);
  //disp.setDrawColor(0);
  disp.setFontPosTop();
  disp.setFontDirection(0);
  disp.setDrawColor(0);

  disp.setFont(u8g2_font_t0_11b_mr);  


  //disp.setCursor(0,8);
  disp.drawStr(2,yofs, " Cal ");
  disp.setDrawColor(1);

    /* all graphics commands have to appear within the loop body. */
  drawBatt(wid, yofs);    
#ifdef ARDUINO_ARCH_ESP8266
   drawWifi(wid, yofs);
#endif    
  disp.setDrawColor(1);
#ifdef ARDUINO_ARCH_ESP8266
  strcpy_P(drawbuf, (char*)pgm_read_dword(&(calTxt[currCal])));
#endif
  disp.drawStr (2 + (wid/6), yofs, drawbuf);
  snprintf(drawbuf, DRAWBUF_SIZE, "FO2 %s%%", caldispbuf[currCal]);
  disp.drawStr (2 + 3*(wid/6), yofs, drawbuf);
  t = disp.getMenuEvent();
  if (t != 0) tm = t;

  //disp.setFont(u8g2_font_t0_11b_mf);  
  //disp.clearLine(2);
  //disp.setFont(u8g2_font_inr19_mr);
  yofs += 14;
  disp.drawHLine(0, yofs -1, wid);
  xofs = 10;
  for (int c = 0; c < 3; c++){
    disp.setFont(u8g2_font_t0_11b_mr);  

    //disp.setFont(u8g2_font_7x14B_mr);
    disp.setDrawColor(1);
    disp.drawStr (xofs, yofs, vdispbuf[c]); 
    //disp.setFont(u8g2_font_inr16_mr);

   
    disp.drawStr(xofs, yofs + 18, fo2dispbuf[c]); 
    xofs += wid/3;
    disp.drawVLine(xofs -4, yofs -1, ht - (yofs - 6));
    t = disp.getMenuEvent();
    if (t != 0) tm = t;

  }
  return tm;
}

void setPointNext(void) {
  currSetpoint ++;
  if (currSetpoint > 1) currSetpoint = 0;
#ifdef RELAY
  setpointv = setpoints[currSetpoint];
#endif
}

void setPointPrev(void) {
  currSetpoint --;
  if (currSetpoint > 1) currSetpoint = 1;
#ifdef RELAY
  setpointv = setpoints[currSetpoint];
#endif
}


void calNext(void) {
  currCal ++;
  if (currCal > 2) currCal = 0;
}

void calPrev(void) {
  currCal --;
  if (currCal > 2) currCal = 2;
}

int majVote (void) {
  int a = setstate[0];
  int b = setstate[1];
  int c = setstate[2];
  if (a == b) return a;
  if (a == c) return a;
  if (b == c) return b;
  return -3;
}

#ifdef RELAY
float majAvg (void) {
  float sbuf[3];
  uint8_t sids[3];
  int nsens = 0;
  for (int c = 0; c <3; c++) {
    if(setstate[c] > -3 && setstate[c] < 3) {
      sbuf[nsens] = po2buf[c];
      sids [nsens] = c;
      nsens++;
    }
  }
  for (int c = 0; c < nsens; c++) {
    for (int d = c +1; d < nsens; d++) {
      if (sbuf[c] > sbuf[d]) {
        float t= sbuf[c];
        uint8_t sid= sids[c];
        sbuf[c] = sbuf[d];
        sbuf[d] = t;
        sids[c] = sids[d];
        sids[d] = sid;
      }
    }
  }
  sensel[3] = 0;
  sensel[1] = ',';
  if (nsens == 3) {
    float d1 = sbuf[1] - sbuf[0];
    float d2 = sbuf[2] - sbuf [1];
    if(d1 > d2) {
      if (sids[1] > sids[2]) {
        sensel[0] = sids[2] + '1';
        sensel[2] =sids[1] + '1';
      } else {
        sensel[0] = sids[1]  + '1';
        sensel[2] =sids[2] + '1';
      }
      return (sbuf[1] + sbuf[2])/2.0;
     } else {
       if (sids[0] > sids[1]) {
        sensel[0] = sids[1] + '1';
        sensel[2] =sids[0] + '1';
      } else {
        sensel[0] = sids[0]  + '1';
        sensel[2] =sids[1] + '1';
      }
      return (sbuf[0] + sbuf[1])/2.0;
     }
  } else if (nsens == 2) {
    if (sids[0] > sids[1]) {
        sensel[0] = sids[1] + '1';
        sensel[2] =sids[0] + '1';
      } else {
        sensel[0] = sids[0]  + '1';
        sensel[2] =sids[1] + '1';
      }
      return (sbuf[0] + sbuf[1])/2.0;
  }  else if (nsens == 1) {
    sensel[0] = sids[0]  + '1';
    sensel[2] ='X';
    return sbuf[0];
  } else {
    sensel[0] = 'X';
    sensel[2] ='X';
    return -1.0;
  }
}

void relay_set_output(uint32_t ov) {
    uint8_t buf [5];
    buf [0] = 0x2e;
    buf [1] = (ov >> 24 )& 0xff;
    buf [2] =(ov >> 16) & 0xff;
    buf [3] =  (ov >> 8)&0xff;
    buf [4] =  ov & 0xff;
    Wire.beginTransmission(RELAY_ADDR);
    Wire.write(buf, 5);
    Wire.endTransmission();
    TRC_PRINT("output: %d\n", ov);
    //delay(1);
}


void relay_set_window(uint32_t ov) {
    uint8_t buf [5];
    buf [0] = 0x2f;
    buf [1] = (ov >> 24 )& 0xff;
    buf [2] =(ov >> 16) & 0xff;
    buf [3] =  (ov >> 8)&0xff;
    buf [4] =  ov & 0xff;
    Wire.beginTransmission(RELAY_ADDR);
    Wire.write(buf, 5);
    Wire.endTransmission();
    //delay(1);
    relay_did_window = true;
}
void relay_rst() {
    TRC_PRINT("%s\n","relay_rst");

    uint8_t buf;
    buf  = 0xff;
    last_relay_reset = millis();
    relay_did_window = 0;
    Wire.beginTransmission(RELAY_ADDR);
    Wire.write((uint8_t)buf);
    Wire.endTransmission();
    //delay(1);
}

uint8_t relay_rdy() {
  TRC_PRINT("%s\n","relay_rdy");
  uint8_t  rdy = 0, avail = 0;
      Wire.beginTransmission(RELAY_ADDR);
    Wire.write((uint8_t)0x2d);
    Wire.endTransmission();
    //delay(6);
  Wire.requestFrom(RELAY_ADDR, 1);
      delay(6);

  avail = Wire.available();
  if (avail <= 0) {
        DBG_PRINT("%s\n","relay not rdy");
        //delay(1);

    return 0xff;
  } else {
      rdy = Wire.read();
  }
  if (avail > 1) ERR_PRINT ("WEIRD- got %d\n", avail);
     //delay(1);

    TRC_PRINT("relay_rdy done %d\n", rdy);
   //delay(1);
  return rdy;
}


void changeAutoTune()
{
 if(!tuning)
  {
    //Set the output to the desired starting frequency.
    outputval=aTuneStartValue;
    aTune.SetNoiseBand(aTuneNoise);
    aTune.SetOutputStep(aTuneStep);
    aTune.SetLookbackSec((int)aTuneLookBack);
    AutoTuneHelper(true);
    tuning = true;
  }
  else
  { //cancel autotune
    aTune.Cancel();
    tuning = false;
    AutoTuneHelper(false);
  }
}

void AutoTuneHelper(boolean start)
{
  if(start)
    ATuneModeRemember = myPID.GetMode();
  else
    myPID.SetMode(ATuneModeRemember);
}

void doPID (unsigned long curr, funclist_t * f, void * ctx) {
  float iv  = majAvg();
  if (iv > 0.0) {
    inputval = iv;
    dtostrf(inputval, 5, 2, po2seldisp);
    dotrim(po2seldisp);
    if (tuning) {
      byte val = (aTune.Runtime());
        if (val!=0)
        {
          tuning = false;
        }    
       if(!tuning)
        { //we're done, set the tuning parameters
          kp = aTune.GetKp();
          ki = aTune.GetKi();
          kd = aTune.GetKd();
          myPID.SetTunings(kp,ki,kd);
          AutoTuneHelper(false);
        }
    } else{
      myPID.Compute();
    }
  }
  if(iv > 0.0 || forcerelay) {
    if (got_relay) {
       //delay(12);
       TRC_PRINT("Output %d\n", (uint32_t)outputval);
       //delay(12);
       relayOn = ((((uint8_t)relay_rdy()) & 0x2) == 0x2);
       delay(18);
       if (!forcerelay) {
        relay_set_output((uint32_t)outputval);
       } else {
          relay_set_output((uint32_t)forcedvalue);
       }

    } 
#ifdef RELAY_PCF    
    else if (gotpcf2) {
        if (curr - windowStartTime > WINDOW_SIZE)
        { //time to shift the Relay Window
          windowStartTime += WINDOW_SIZE;
        }
        if (outputval > curr - windowStartTime) {
            PCFr.digitalWrite(RELAY_PIN, HIGH);
            relayOn = 1;
        } else { 
            PCFr.digitalWrite(RELAY_PIN, LOW);
            relayOn = 0;
        }
    }
#endif
  } else {
      if (got_relay) { 
            relay_set_output (0);
      } 
#ifdef RELAY_PCF    
      else if(gotpcf2) {
           PCFr.digitalWrite(RELAY_PIN, LOW);
      }
#endif
      relayOn = 0;
      strcpy (po2seldisp, "  xXx  ");
  }
}

#endif
// 0 = on setpoint -3 fault -2 critical low -1 under +1 over +2 critical high  +3 overscale
int setco2state (){
  if (co2ppm < 500) {
    return 0;
  }
  if (co2ppm < 5000){
    return 1;
  }
  if (co2ppm < 10000)
  {
    return 2;
  }
  return 3;
}

void doTone(unsigned long curr, funclist_t * f, void * ctx) {
  int toneFreq = 0;
  int toneDur = 0;
  int toneIntvl = 1000;
  int vote = majVote();
  
  if (vote == 0  && co2state == 0) {
     //OK
     toneFreq = 0;
     toneIntvl = 1000;
  } else if (vote == -1 &&  co2state < 2) {
      //Too low
      toneIntvl = 2000;
      toneFreq = 440;
      toneDur = 200;
  } else if ((vote == 1 && co2state < 3) || co2state == 2) {
      toneIntvl = 2000;
      toneFreq = 1000;
      toneDur = 200;

       //Too high
  } else if (vote == -2 || vote == 2 || vote == -3 ||
                vote == 3 || co2state == 3) {
      //Bad
      toneIntvl = 1000;
      toneFreq = 1000;
      toneDur = 500;
  }
  if ( curr - lastTone > lastToneIntvl) {
        if (curr - lastTone > (2 *lastToneIntvl)){ 
          lastTone = curr;
        } else {
          lastTone += toneIntvl;
         }
       lastToneIntvl = toneIntvl;
       lastToneFreq = toneFreq;
       if (toneFreq == 0 || silent) {
#ifdef ARDUINO_ARCH_ESP8266
          noToneAC(TONE_PIN, TONE_PIN_2);
#endif
        } else {
#ifdef ARDUINO_ARCH_ESP8266
          toneAC(TONE_PIN, TONE_PIN_2, toneFreq, toneDur);
#endif
        }
    }
}

void doPulse(unsigned long curr, funclist_t * f, void * ctx) {
        digitalWrite(PWR_PIN, LOW);
        delay(5);
        digitalWrite(PWR_PIN, HIGH);
        long heap = ESP.getFreeHeap();
        long ustack = cont_get_free_stack(g_pcont); //ESP.getFreeContStack().
        register uint32_t *sp asm("a1");
        long stack = 4 * (sp - g_pcont->stack);
        DBG_PRINT("Free Heap: %d\n",heap);
        DBG_PRINT("unmodified stack   = %4d\n", ustack);

        DBG_PRINT("current free stack = %4d\n", stack);
        //lprintf("\"memory\": {\"heap\":%d, \"context\":%d, \"stack\":%d }",
        //    heap, ustack, stack);
        lprintf("M,%d,%d,%d\n",
            heap, ustack, stack);
}

#ifdef CO2
void doCO2(unsigned long curr, funclist_t * f, void * ctx) {
  if (millis() - co2start > 10000 && gotco2) {
    if(co2sens.measure()) {
      co2ppm = co2sens.ppm;
      co2state = setco2state();
      snprintf(co2dispbuf, 8, "%d", co2ppm);
    } else {
      gotco2 = false;
      co2state = 0;
    }
  }
}
#endif

void setupWdt () {
  yield_add(NAMEOF(doData), NULL, 300, 0, false, true);
  yield_add(NAMEOF(doLog), NULL, 1000, 0, false, true);
  yield_add(NAMEOF(doPulse), NULL, 5000, 0, false, true);
#ifdef RELAY_PCF  
  if (gotpcf2) 
    yield_add(NAMEOF(doPID), NULL, 0, 0, false, true);
  else
#endif
    yield_add(NAMEOF(doPID), NULL, 200, 0, false, true);

  yield_add(NAMEOF(doTone), NULL, 0, 0, false, true);
#ifdef HUD_BLINK
  yield_add(NAMEOF(doBlink), NULL, BLINK_CYC+200, 6500, false, true);
#endif
#ifdef HUD
  yield_add(NAMEOF(doHUD), NULL, 500, 0, false, true);
#endif
  yield_add(NAMEOF(doi2cPing), NULL, 12000, 0, false, true);
#ifdef DECOMP
  yield_add(NAMEOF(doVPM), NULL, 5000, 0, false, true);

  yield_add(NAMEOF(doDecomp), NULL, 1500, 0, false, true);
  yield_add(NAMEOF(doND), NULL, 3000, 0, false, true);

#endif
#ifdef CO2
  yield_add(NAMEOF(doCO2), NULL, 6000, 0, false, true);

#endif
}

/*
void doWdt() {

    YIELD();
#ifdef ARDUINO_ARCH_ESP8266  

   // unsigned long curr = millis();
    
    //if (curr - lastPulse > 50) {
    //   digitalWrite(PWR_PIN, HIGH);
    //}
    if (curr - lastPulse > 5000) {
       if (curr - lastPulse > (2*5000)) {
          lastPulse = curr;
       }  else {
        lastPulse += 5000;
       }
       doPulse();
    }
 #endif

}

*/

#ifdef LOGGING
/* unsigned long lastLog= 0;

void doLogT() {

      unsigned long curr = millis();
     
      if (curr - lastLog > 1000) {


              if (curr - lastLog > (2*1000)) {
                lastLog = curr;
              } else {
                lastLog += 1000;
              }
              doLog();
      }
}

*/

void doLog(unsigned long curr, funclist_t * f, void * ctx) {
      char * logBuf;
      int n;
      const int sz = 255;
      logBuf = (char*)malloc(sz * sizeof(char));
      if (!logBuf) {
        lprintf("M,%s,no memory\n", __func__);
        return;
      }
              //n = snprintf(logBuf, sz, "\"log\": { \"timeStamp\": \"");
              n = snprintf(logBuf, sz, "L,");
              n += NTP::timeStamp(logBuf+n, 255 -n);
#ifdef PRESSURE
              //n += snprintf (logBuf + n, sz -n, "\", \"battV\":%s, \"depthM\":%s, \"press\":%s, \"tempC\": %s", battBuf, ddispbuf, pdispbuf, tempBuf);
              n += snprintf (logBuf + n, sz -n, ",%s,%s,%s,%s", battBuf, ddispbuf, pdispbuf, tempBuf);

#else 
              //n += snprintf (logBuf + n, sz -n, "\", \"battV\":%s", battBuf);
              n += snprintf (logBuf + n, sz -n, ",%s", battBuf);

#endif
#ifdef RELAY
           //n += snprintf (logBuf + n, sz -n, ", \"setpoint\":%d, \"setpO2\":%s, \"pO2\":[%s, %s, %s], \"sensorState\":%s, \"relay:\":%d },\n", 
           n += snprintf (logBuf + n, sz -n, ",%d,%s,%s,%s,%s,%s,%d", 

                  currSetpoint + 1, setdispbuf[currSetpoint], 
                  po2dispbuf[0], po2dispbuf[1], po2dispbuf[2], 
                  sensel, relayOn);
#else
           //n += snprintf (logBuf + n, sz -n, ", \"setpoint\":%d, \"setpO2\":%s, \"pO2\":[%s, %s, %s]},\n", 
            n += snprintf (logBuf + n, sz -n, ",%d,%s,%s,%s,%s", 
                   currSetpoint + 1, setdispbuf[currSetpoint], 
                  po2dispbuf[0], 
                  po2dispbuf[1], po2dispbuf[2]);
#endif
#ifdef CO2
            n += snprintf (logBuf + n, sz -n, ",%s", co2dispbuf);

#endif
#ifdef DECOMP
            n += snprintf (logBuf + n, sz -n, ",%d,%d,%s,%s,%s",
                    dostop, nstops, ndispbuf, stopdispbuf,stoptimedisp);

#endif
            n += snprintf (logBuf + n, sz -n, "\n");

            lprintf(logBuf);
#ifdef DEBUG
            //Syslog.print(logBuf);
            //Syslog.print("\r\n");
#endif      
      free(logBuf);
      TRC_PRINT("%s\n","doLog done");
}

#endif

void doData(unsigned long curr, funclist_t * f, void * ctx) {
        TRC_PRINT("dodata\n");

    getPO2();
   TRC_PRINT("pospo2\n");

    convPO2();
    convFO2();
#ifdef PRESSURE   
      //DBG_PRINT("depth\n");

    getDepth();
#endif

    getBatt();
    TRC_PRINT("dodata end\n");

}


#ifdef HUD

void doHUD (unsigned long curr, funclist_t * f, void * ctx) {
    HUD_FIRST_PAGE();
    do {
      //YIELD();
     huddisp();
     //if (t != 0) menu = t;
     //t = disp.getMenuEvent();      
     //if (t != 0) menu = t;

   } while ( HUD_NEXT_PAGE() );
}

#endif


void title() {
  //DBG_PRINT("%s\n","TITLE");
  disp.setPowerSave(0);
  disp.clear();
  FIRST_PAGE();

  do {
    YIELD();
    titleDisp();
  } while ( NEXT_PAGE() );
#ifdef ARDUINO_ARCH_AVR   
  toneAC(1000, 10, 200, true);
#endif
#ifdef ARDUINO_ARCH_ESP8266  
   toneAC(TONE_PIN, TONE_PIN_2, 1000, 200);
#endif
#ifdef HUD  
 disp2.setPowerSave(0);
  disp2.clear();
  HUD_FIRST_PAGE();

  do {
    YIELD();
    hudtDisp();
  } while ( HUD_NEXT_PAGE() );
#endif
}

int8_t mainscreen(){
   int8_t vert = 0;
   bool d;
   if (orientation == 1 || orientation == 3) {
     vert = 1;
     disp.setDisplayRotation(orientations[orientation]);
   }
   int8_t menu = 0, t;
   //DBG_PRINT("%s\n","mains");
    t = disp.getMenuEvent();
    if (t != 0) menu = t;
   

    t = disp.getMenuEvent();
    if (t != 0) menu = t;
   
    FIRST_PAGE();   
  
    do {  
    
      YIELD();

    //TRC_PRINT("%s\n","predisp");
  

     t = po2disp(vert);

     //TRC_PRINT("%s\n","posdisp");
     if (t != 0) menu = t;
     t = disp.getMenuEvent();      
     if (t != 0) menu = t;
      //     unsigned long curr = millis();

      d = NEXT_PAGE();

      //dbprintf("Time: %d\n", millis()-curr);
   } while ( d );
   
    switch( menu) {
          case  U8X8_MSG_GPIO_MENU_SELECT :
            lastMenu = currMenu;
            currMenu = 0;
            setOri();
            break;
          case U8X8_MSG_GPIO_MENU_NEXT : 
            setPointNext();
          break;
         case U8X8_MSG_GPIO_MENU_PREV:
            setPointPrev();
         break ;
          default:
            lastMenu = currMenu;
            break;
        }

}


int8_t calscreen(){
   int8_t menu = 0, t;
    t = disp.getMenuEvent();
    if (t != 0) menu = t;
    //getPO2();
    //convFO2();
    t = disp.getMenuEvent();
    if (t != 0) menu = t;
    FIRST_PAGE();
    do {
      YIELD();
      //if (t != 0) menu = t;
      t = caldisp();
      if (t != 0) menu = t;
      t = disp.getMenuEvent();
      if (t != 0) menu = t;
   } while ( NEXT_PAGE() );
    switch( menu) {
          case  U8X8_MSG_GPIO_MENU_SELECT :
            if (currCal == 2) custcal();
            calconfirm();
            break;
          case U8X8_MSG_GPIO_MENU_NEXT : 
            calNext();
          break;
         case U8X8_MSG_GPIO_MENU_PREV:
            calPrev();
         break ;
          default:
            lastMenu = currMenu;
            break;
        }
}

void custcal() {
  uint8_t val, res;
  val = calFrac[currCal]*100;
         disp.setFont(u8g2_font_t0_11b_mr);  
           strcpy_P(titlbuf, titleCust);

         res = disp.userInterfaceInputValue(titlbuf, "" , &val, 10, 100, 3, "%");
  if(res) {
    calFrac[currCal] = ((float) val) / 100.0F;
    buildCal();
  }
}

void init_menubuf(int size)
{
  if (menubuf) free (menubuf);
  menubuf = (char *)malloc (size * sizeof(char));
  if (!menubuf) {
      lprintf("M,%s,no memory\n", __func__);
      menubuf_size = 0;
      return;
  }
  menubuf_size = size;
}

void calconfirm(){
     int8_t t;
       char scalbuf[3][7];
       char vcalbuf[3][8];
       //char caltitl[20];
       //char calline[DRAWBUF_SIZE];
       float newscal[3];
       init_menubuf(64);
       for (int c = 0; c < 3; c++) {
         newscal[c] = calFrac[currCal] / vbuf[c];
         dtostrf(newscal[c], 6, 3, scalbuf[c]);
         dtostrf(vbuf[c], 7, 2, vcalbuf[c]);

         //floatConv(scalbuf[c], 10, newscal[c], 3, 0);
         //floatConv(vcalbuf[c], 10, vbuf[c], 3, 0);
       }
       strcpy_P(titlbuf, titleCalConf);
#ifdef ARDUINO_ARCH_ESP8266
       strcpy_P(menubuf, (char*)pgm_read_dword(&(calTxt[currCal])));
#endif
#ifdef ARDUINO_ARCH_AVR
       strcpy_P(menubuf, (char*)pgm_read_word(&(calTxt[currCal])));
#endif
       strcat(menubuf, " ");
       strcat(menubuf, caldispbuf[currCal]);
       strcat(menubuf, "%");

       //snprintf(titlbuf, 20, "%s %s%%", calTxt[currCal], caldispbuf[currCal]);
       snprintf (drawbuf, DRAWBUF_SIZE, "%s%s|%s%s|%s%s", vcalbuf[0], scalbuf[0], vcalbuf [1] , scalbuf[1],vcalbuf[2], scalbuf[2]);
       disp.setFont(u8g2_font_t0_11b_mr);  
       t = disp.userInterfaceMessage(titlbuf, menubuf , drawbuf, " Cancel \n OK ");
       if (t == 2) {
            for (int c = 0; c < 3; c++) scales[c] = newscal[c];
           int p = scalofs;
#ifdef ARDUINO_ARCH_AVR
           EEPROM.put (p, scales[0]);
           EEPROM.put (p += sizeof(float), scales[1]);
           EEPROM.put (p  += sizeof(float), scales[2]);
           if (currCal == 2)  EEPROM.put(custofs, calFrac[2]);
           //EEPROM.commit();          
#endif
#ifdef ARDUINO_ARCH_ESP8266
          saveVPMConfig();
     
#endif 
       } else {
            lastMenu = currMenu;
            currMenu = 0;       
       }
}

void menudisp() {
    disp.setFont(u8g2_font_t0_11b_mr);  
    init_menubuf(128);

    strcpy_P(titlbuf, titleOptions);

    strcpy_P(menubuf, menuOptions);
    lastMenu = currMenu;
    currMenu = disp.userInterfaceSelectionList(titlbuf, 1, menubuf);          
}

#ifdef ARDUINO_ARCH_ESP8266
void remdisp() {
    uint8_t m;
    bool is_ap;
    init_menubuf(64);

    disp.setFont(u8g2_font_t0_11b_mr);  
    strcpy_P(titlbuf, titleRemote);
    strcpy_P(menubuf, menuRemote);
    m = disp.userInterfaceSelectionList(titlbuf, 1, menubuf);
    if (m == 1) {
      OTAinit(is_ap = false);             
    } else if (m == 2) {
      OTAinit(is_ap = true);
    } else if (m == 3) {
      disconnectWifi();
      lastMenu = currMenu;
      currMenu = 0;
      return;
    } else if (m == 4) {
        int r = wifiNetSelScan();
        if(!r) {
           lastMenu = currMenu;
           currMenu = 0;
          return;
        }
    } else if (m == 5) {
      menuDeleteDiveLogs();
      lastMenu = currMenu;
      currMenu = 0;
      return;
    }
    OTAloop(is_ap);
}

#ifdef RELAY
void relaydisp() {
    uint8_t m;
    init_menubuf(64);

 while (currMenu != 0) {
  
  disp.setFont(u8g2_font_t0_11b_mr);  
    strcpy_P(titlbuf, titleRelay);
    if (forcerelay) {
      strcat (titlbuf, ": ");
      if (forcedvalue >= 4000.0) strcat (titlbuf, "On");
      else strcat (titlbuf, "Off");
    } else if (tuning) {
      strcat (titlbuf, ": Tuning");
    }
    strcpy_P(menubuf, menuRelay);
    m = disp.userInterfaceSelectionList(titlbuf, 1, menubuf);
    switch (m){
     case 1:
      forcerelay = true;
      forcedvalue = 4000; 
      break;
     case 2:
      forcerelay = true;
      forcedvalue = 0; 
      break;
   case 3:
      forcerelay = false;
      break;
   case 4:   
      changeAutoTune();
      break;
   case 5:
       lastMenu = currMenu;
       currMenu = 0;
       break;
    }
 }

}
#endif 

void infodisp() {
  FSInfo f;
  
  disp.clear();
  init_menubuf(48);

   while (disp.getMenuEvent() == 0) {
    YIELD();
    FIRST_PAGE();
    SPIFFS.info(f);

    do {

      int lofs = 11;
      disp.setFont(u8g2_font_t0_11b_mr);  
      strcpy_P(titlbuf, ver);
      disp.drawStr( (256 / 2) - (disp.getStrWidth(titlbuf)/2),lofs, titlbuf);
      lofs += 12;
      snprintf (menubuf, menubuf_size, "Free Mem= %d", ESP.getFreeHeap());
      disp.drawStr( 2,lofs, menubuf);
      lofs += 12;

      snprintf (menubuf, menubuf_size, "Devs=[RTC:%d ADC:%d Prs:%d Rly:%d Hud:%d CO2:%d]", gotrtc, gotAds, gotms5803, got_relay, got_hud, gotco2);
      disp.drawStr( 2,lofs, menubuf);
       lofs += 12;

      snprintf (menubuf, menubuf_size, "Free Disk= %dk/%dk",f.usedBytes/1024 , f.totalBytes/1024);
      disp.drawStr( 2,lofs, menubuf);
      //const char  inf [] = "Info";
      //disp.drawStr( (256 / 2) - (disp.getStrWidth(inf)/2),(64/2 - 11/2), inf);
    } while (NEXT_PAGE());
  }
  lastMenu = currMenu;
  currMenu = 0;

}

#ifdef DECOMP

bool vpm_config_ok= false;

void vpmdisp_pre() {
  disp.clear();
  FIRST_PAGE();
  do {
    disp.setFont(u8g2_font_t0_11b_mr);  
    const char  vpmt [] = "VPM-B test";
    disp.drawStr( (256 / 2) - (disp.getStrWidth(vpmt)/2),(64/2 - 11/2), vpmt);
  } while (NEXT_PAGE());
  loadTestDives();
  vpm_data_validate();
  vpm_test_run();
  vpm_data_validate();
  //sleep(20000);        
   lastMenu = currMenu;
   currMenu = 0;
}

void vpmdisp() {
  disp.clear();
  FIRST_PAGE();
  do {
    disp.setFont(u8g2_font_t0_11b_mr);  
    const char  vpmt [] = "VPM-B test dive";
    disp.drawStr( (256 / 2) - (disp.getStrWidth(vpmt)/2),(64/2 - 11/2), vpmt);
  } while (NEXT_PAGE());
  loadTestDives();
  if (vpm_config_ok) vpm_data_validate();
  //sleep(20000);
  //vpm.setPlannedDepth(28.0);
   lastMenu = currMenu;
   currMenu = 0;
  if (vpm_config_ok) {
    test_ready =1;
  } else {
     do {
      disp.setFont(u8g2_font_t0_11b_mr);  
      const char  vpmf [] = "Test dive load failed";
      disp.drawStr( (256 / 2) - (disp.getStrWidth(vpmf)/2),(64/2 - 11/2), vpmf);
     } while (NEXT_PAGE());
  }
}
#endif
#endif

void spdisp() {
      //char spbuf[30];
      //char tbuf [20];
      int n, m;
      uint8_t val, res;

      n = snprintf(drawbuf, DRAWBUF_SIZE, "SP %d PO2 %s\n", 1, setdispbuf[0]);
      snprintf(drawbuf +n, DRAWBUF_SIZE -n, "SP %d PO2 %s", 2, setdispbuf[1]);
      disp.setFont(u8g2_font_t0_11b_mr); 
      strcpy_P(titlbuf, titleSetpoints);
      m = disp.userInterfaceSelectionList(titlbuf, 1, drawbuf); 
      val = setpoints[m-1]*10;
      snprintf(drawbuf, DRAWBUF_SIZE, "Setpoint %d", m);
      res = disp.userInterfaceInputValueDp(drawbuf, "PO2 " , &val, 2, 16, 2, "",1);
      if (res) {
        setpoints[m-1] = (float) val/10.0F;
        buildSP();
#ifdef ARDUINO_ARCH_ESP8266
          saveVPMConfig();
#endif 
      }
      lastMenu = currMenu;
     currMenu = 0;
 }

void oriendisp() {
      //char spbuf[30];
      //char tbuf [20];
      uint8_t  ori = orientation;
      uint8_t m;
      //uint8_t val, res;
      init_menubuf(64);
      disp.setFont(u8g2_font_t0_11b_mr); 
      strcpy_P(titlbuf, titleOri);

      strcpy_P(menubuf, menuOri);
      m = disp.userInterfaceSelectionList(titlbuf, 1, menubuf); 
      switch(m) {
        case 1:
          ori = 0;
          break;
        case 2:
          ori = 2;
          break;
        case 3:
         ori = 1;
         break;
        case 4:
          ori = 3;
          break;
        break;
        default:
         ori = 0;
         break;
      }
      if (ori != orientation) {
        orientation = ori; 
#ifdef ARDUINO_ARCH_ESP8266
          saveVPMConfig();
#endif 
        setOri();

      }
      lastMenu = currMenu;
      currMenu = 0;
  }

void setOri(void) {
  uint8_t ori = orientation;
  if (orientation == 1) ori = 0;
  if (orientation == 3) ori = 2;

  disp.setDisplayRotation(orientations[ori]);
}

void initi2cA() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_SPEED);
  Wire.setClockStretchLimit(CLOCK_STRETCH);    // in µs

  if (pingi2c(EXPANDER_ADDR)) {

    if (!gotpcf) {
      PCF.begin(EXPANDER_ADDR);
      PCF.pinMode(DISP_SEL - EXPANDER_RANGE, INPUT_PULLUP);
      //PCF.pinMode(DISP_RST - EXPANDER_RANGE, OUTPUT);

      PCF.pinMode(DISP_EN_PCF, OUTPUT);
      PCF.digitalWrite(DISP_EN_PCF, 0);
       //PCF.digitalWrite(DISP_RST - EXPANDER_RANGE, 1); 
        //PCF.pinMode(PWR_PIN - EXPANDER_RANGE, OUTPUT);
        //PCF.digitalWrite(PWR_PIN - EXPANDER_RANGE, HIGH);
      gotpcf = true;
    }
  } else {
    ERR_PRINT("%s\n","NO PCF");
    gotpcf = false;
  }
  //Serial.flush();
  if (pingi2c(RTC_ADDR)) {
     setSyncProvider(dsRTC.get);
      if (timeStatus() != timeSet) {
         ERR_PRINT("%s\n","RTC INVALID");
      }
      gotrtc = true;
  } else {
    ERR_PRINT("%s\n","NO RTC");
    gotrtc = false;
  }
}
bool gotBme = false;

void initi2cB() {

 if (pingi2c(ADS_ADDR)) {
       ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV
       ads.begin();
     gotAds = true;
   } else {
    ERR_PRINT("%s\n","NO ADS");
   }

#ifdef PRESSURE
  if(pingi2c(PRESSURE_ADDR)) {
    //DBG_PRINT("%s\n","gotpraddr");
    if (!gotms5803) {
      if (psens.initializeMS_5803(false)) {
        gotms5803 = true;
      } else {
        ERR_PRINT("ms5803 init failed\n");
      }
    }
  }
  if (gotms5803) {
     //DBG_PRINT("%s\n","pressure");

    } else {
      ERR_PRINT("nopressure\n");
    }
#endif
  if(pingi2c(BME_ADDRESS)) {
     if (!bme.begin(BME_ADDRESS)) {
        ERR_PRINT("Could not find a valid BME280 sensor, check wiring!\n");
      } else {
         gotBme = true;
      } 
  }

#ifdef CO2
 if(pingi2c(SC16IS750_ADDR)) {
     if (rmtSerial.ping () != 1) {
        ERR_PRINT("Could not find a valid UART for co2 sensor, check wiring!\n");
        gotco2 = false;
      } else {
        if (!gotco2) {
          rmtSerial.begin(9600);
          co2start = millis();
          gotco2 = true;
        }
      } 
  } else {
   ERR_PRINT("Could not ping UART for co2 sensor, check wiring!\n");

    gotco2 = false;
  }
#endif
#ifdef RELAY
   initi2cRelay();
#endif
#ifdef HUD_BLINK
  initi2cHUD();
#endif


}

#ifdef HUD_BLINK
void initi2cHUD() {
  //delay(5);
  if (pingi2c(HUD_ADDR)) {
    delay(12);
    if (!got_hud) {
      hud_rst();
    }
    //delay(50);
      got_hud = true;
    // if(hud_rdy() >= 0) {
    //    got_hud = true;
    // } else
    //    got_hud = false;
  } else {
           got_hud = false;
  }
  if (!got_hud) {
    DBG_PRINT("NO BLINKY\n");
  }
  //delay(50);
}
#endif

void initi2cRelay(){
#ifdef RELAY_PCF    
  if (pingi2c(EXPANDER2_ADDR)) {
    if(!gotpcf2) {
      PCFr.begin(EXPANDER2_ADDR);
      PCFr.pinMode(RELAY_PIN, OUTPUT);
      PCFr.digitalWrite(RELAY_PIN, 0);
      gotpcf2 = true;
    }
  } else {
    DBG_PRINT("NO RELAY PCF\n");
    gotpcf2 = false;
  }
#endif
   if(pingi2c(RELAY_ADDR)) {
        if (!got_relay) {
          relay_rst();
          delay(12);

        }
          //delay(12);

        if(!relay_did_window) relay_set_window(WINDOW_SIZE);
        got_relay = true;
        delay(12);
        relay_set_output(0);
  } else {
    DBG_PRINT("Could not find a valid Relay controller, check wiring!\n");
            got_relay = false;

  }
}


void initSPIFFS() {
  if(!SPIFFS.begin()) {
    SPIFFS.format();
  }
   //Serial.setDebugOutput(true);
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {    
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      DBG_PRINT("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
      if (fileName.startsWith("/logs/surface") && fileName.endsWith(".log") && !fileName.equals(lastlogfilename)) SPIFFS.remove(fileName);
    }
  }
}


void deleteDiveLogs() {
    {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {    
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      DBG_PRINT("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
      if (fileName.startsWith("/logs/dive") && fileName.endsWith(".log") && !fileName.equals(lastlogfilename)) SPIFFS.remove(fileName);
    }
  }
}


void bootInit(int old_didsleep) {
 initi2cB();
#ifdef DECOMP
  
  initSPIFFS();
  DBG_PRINT("\n");
  loadVPMConfig();
#endif

  buildSP();
  buildCal();
  lastMenu = currMenu;
  currMenu = 1;
  //PCF.digitalWrite(DISP_RST - EXPANDER_RANGE, 0);
//delay(10);
  //PCF.digitalWrite(DISP_RST - EXPANDER_RANGE, 1);
//delay(10);
#ifdef ARDUINO_ARCH_ESP8266

   SPI.begin();
   SPI.setFrequency(8000000);
#endif
  disp.begin(DISP_SEL, DISP_NXT, DISP_PRV);
#ifdef HUD
  disp2.begin();
#endif
  setOri();
#ifdef RELAY
  myPID.SetOutputLimits(0, WINDOW_SIZE);
  myPID.SetMode(AUTOMATIC);
  
  
#endif
  //DBG_PRINT("%s\n","sudone");
  //
  if (!old_didsleep) {
#ifdef ARDUINO_ARCH_ESP8266
    toneAC(TONE_PIN, TONE_PIN_2, 1000, 200);
#endif
    for (int c = 0; c < 4; c++) {
      delay(50);
      YIELD();
    }
  }
}


#ifdef ARDUINO_ARCH_ESP8266
//extern "C" void ets_update_cpu_frequency(int freqmhz);

#if F_CPU != 160000000L
#error "Needs 160MHz"
#endif
void setup (void) 
{
  int autopower_temp;
  int old_didsleep = 0;
  int dowakeup = 0;
  REG_CLR_BIT(0x3ff00014, BIT(0));
  delay(10);
  REG_SET_BIT(0x3ff00014, BIT(0));
  ets_update_cpu_frequency(160);
  //char resetReason[20];
#ifdef DEBUG
    Serial.begin(115200);
    DBG_PRINT("%s\n","init");
    DBG_PRINT("%s\n",ESP.getResetReason().c_str());
    DBG_PRINT("%d\n",F_CPU);
    DBG_PRINT("%d\n",ESP.getCpuFreqMHz());
    DBG_PRINT("%d\n",clockCyclesPerMicrosecond());
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(I2C_SPEED);
    Wire.setClockStretchLimit(CLOCK_STRETCH);    // in µs

    scani2c();
#endif
    initi2cA();

    initSPIFFS();
    loadVPMConfig();
  pinMode (DISP_CS, OUTPUT);
  pinMode (DISP_DC, OUTPUT);

  uint32_t didsleep_buf = 0;
  ESP.rtcUserMemoryRead(0, &didsleep_buf, sizeof(didsleep_buf));
  old_didsleep = didsleep_buf;
  didsleep = 0;
  didsleep_buf = 0;
  DBG_PRINT("%s\n","prepwroff");
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite (PWR_PIN, HIGH);

  ESP.rtcUserMemoryWrite(0, &didsleep_buf, sizeof(didsleep_buf));
  //EEPROM.commit();
  if (gotrtc) NTP::digitalClockDisplay(now());
#ifdef RELAY
  initi2cRelay();
  relayOn = 0;
#ifdef RELAY_PCF    
  if(gotpcf2) {
    PCFr.pinMode(RELAY_PIN, OUTPUT);
    PCFr.digitalWrite(RELAY_PIN, 0);
  }
#endif
  if (got_relay) {
    relay_set_output(0);
  }
#endif
#ifdef HUD_BLINK
  initi2cHUD();
#endif
  if (gotpcf) {
    dowakeup |= !PCF.digitalRead(DISP_SEL - EXPANDER_RANGE);

  }



  if(gotpcf) dowakeup |= !PCF.digitalRead(DISP_SEL - EXPANDER_RANGE);
  if (dowakeup || autopower) {
    bootInit(old_didsleep);
    onWakeUp();
  } else {
        goToSleepShort();
  }
}

void goToSleepShort() {
    didsleep = 1;
    uint32_t didsleep_buf = didsleep;
    ESP.rtcUserMemoryWrite(0, &didsleep_buf, sizeof(didsleep_buf));
    //DBG_PRINT("%s\n","prepwroff");
    digitalWrite(PWR_PIN, LOW);
    //PCF.digitalWrite(PWR_PIN - EXPANDER_RANGE, LOW);
    delay(25);
    digitalWrite(PWR_PIN, HIGH);
    //PCF.digitalWrite(PWR_PIN - EXPANDER_RANGE, HIGH);
    delay(100);
    //EEPROM.commit();
     DBG_PRINT("%s\n","presleep");

    sleepNow();
}


void goToSleep() {
  if (!autopower) {
     //DBG_PRINT("%s\n","presl");
     disp.setPowerSave(1);
#ifdef HUD
     disp2.setPowerSave(1);
#endif
    if (gotpcf) PCF.digitalWrite(DISP_EN_PCF, 0);
#ifdef RELAY_PCF    
    if(gotpcf2) PCFr.digitalWrite(RELAY_PIN, 0);
#endif
    if(got_relay) relay_set_output(0);

#ifdef HUD_BLINK
    if (got_hud) hud_rst();
#endif
    goToSleepShort();
  }
}


void onWakeUp () {
    if (gotpcf) PCF.digitalWrite(DISP_EN_PCF, 1);
    autopower=1;
    saveVPMConfig();  
  //DBG_PRINT("%s\n","postsl");

   SPI.begin();
   SPI.setFrequency(8000000);
  disp.begin(DISP_SEL, DISP_NXT, DISP_PRV);
  disp.setPowerSave(0);
#ifdef HUD
  disp2.begin();
  disp2.setPowerSave(0);
#endif
}

#endif

void loop (void) 
{
  int8_t menu = 0;
  int8_t powerdown = 0;
  goToSleep();
  setupWdt();
#ifdef RELAY
  windowStartTime = millis();
#endif
  setOri();
  title();
  for (int c = 0; c < 20; c++) {
    delay(50);
    YIELD();
  }
  YIELD();
  unsigned long lastdispres = millis();
  while (!powerdown) {
   switch (currMenu) {
      case 0:
        menudisp();
      break;
      case 1:
        mainscreen();
        break;
      case 2:
        gaslist();
        break;
      case 3:
        spdisp();
        break;
      case 4:
        calscreen();
        break;
      case 5:
        oriendisp();
        break;
      case 6:
        remdisp();
        break;
      case 7:
        vpmdisp();
         break;
      case 8:
         relaydisp();
         break;
      case 9:
         silent = !silent;
          lastMenu = currMenu;
          currMenu = 0; 
          break;
      case 10:
        infodisp();
        break;
      case 11:
        powerdown = 1;
        autopower = 0;
        saveVPMConfig();
        lastMenu = currMenu;
        currMenu = 1;
        break;
      default:
        lastMenu = currMenu;
        currMenu=1;
        break;
   }
   unsigned long curt = millis();
   if (curt - lastdispres > 33000 || currMenu != lastMenu) {
       disp.begin(DISP_SEL, DISP_NXT, DISP_PRV);
       disp.setPowerSave(0);
#ifdef HUD
       disp2.begin();
       disp2.setPowerSave(0);
#endif
      lastdispres = curt;
   }
  }
}

#ifdef HUD_BLINK
//uint8_t last_hud_rdy = -1;
unsigned long last_hud_reset = 0;
uint8_t hud_did_cyc = 0;
/*
unsigned long last_blink = 0;

void doBlinkT(void) {
  unsigned long curr = millis();
  if (curr - last_blink > 1000) {
     doBlink();
  }
  if (curr - last_blink > (2 *1000)){ 
      last_blink = curr;
    } else {
      last_blink += 1000;
    }
}
*/

void doBlink(unsigned long curr, funclist_t * f, void * ctx) {
 uint8_t rdy;
  if (gotBme) { 
    DBG_PRINT( "BME Pressure: %f\n", bme.readPressure()); 
    DBG_PRINT( "BME Temp: %f\n", bme.readTemperature());
  }
 if (got_hud) {
  TRC_PRINT("%s\n","Blinky");
  //rdy = hud_rdy();
  if (/*rdy  >= 0 &&*/ last_hud_reset > 0 && millis()-last_hud_reset > 6000) {
      if (/*last_hud_rdy < 0 || */ hud_did_cyc == 0) {
          TRC_PRINT("%s\n","Blinky Cyc");
          cons->flush();
        hud_set_cyc(BLINK_CYC, BLINK_DUR, BLINK_INT);
        hud_did_cyc = 1;
      }
      TRC_PRINT("%s\n","Blinky po2");
          cons->flush();

      hud_set_blink();
    }
    //last_hud_rdy = rdy;
  }
  delay(1);
  TRC_PRINT("%s\n","Blinky Done");

}

void hud_set_blink(void) {
  float p;
  uint8_t buf [8];
  buf[7] = 0;
  buf[0] = 0x1e;
  for (int c = 0; c < 3; c++){
      p = (po2buf[c] - 1.0) *10.0;
      if (p  < -0.99) {
        buf[c+4] = RED; //red
      } else if (p > 0.99) {
        buf[c+4] = GREEN; //green
      } else {
        buf[c+4] = ORN; //orange
      }
      p = fabs(p);
      buf [c+1] = round(p);
      if (buf[c+1]== 0) buf [c+1] = 1;
      if(buf[c+1] > 4) buf[7] = 1;
      //else bright = 0;
  }
  Wire.beginTransmission(HUD_ADDR);
  Wire.write(buf, 8);
  Wire.endTransmission();
  delay(1);
}

void hud_set_cyc(uint16_t cyc, uint16_t dur, uint16_t intvl) {
    uint8_t buf [7];
    buf [0] = 0x1f;
    buf [1] = cyc >> 8;
    buf [2] = cyc & 0xff;
    buf [3] = dur >> 8;
    buf [4] = dur & 0xff;
    buf [5] = intvl >> 8;
    buf [6] = intvl & 0xff;
    Wire.beginTransmission(HUD_ADDR);
    Wire.write(buf, 7);
    Wire.endTransmission();
    delay(1);
}

void hud_rst() {
    uint8_t buf;
    buf  = 0xff;
    last_hud_reset = millis();
    hud_did_cyc = 0;
    Wire.beginTransmission(HUD_ADDR);
    Wire.write(buf);
    Wire.endTransmission();
    delay(1);
}

uint8_t hud_rdy() {
  TRC_PRINT("%s\n","hud_rdy");
  uint8_t rdy = 0, avail = 0;
  Wire.beginTransmission(HUD_ADDR);
    Wire.write((uint8_t)0x0);
    Wire.endTransmission();
  Wire.requestFrom(HUD_ADDR, 1);
  delay(6);
  avail = Wire.available();
  if (avail <= 0) {
        TRC_PRINT("%s\n","hud not rdy");
        delay(1);

    return -1;
  }
  if (avail > 1) ERR_PRINT ("WEIRD- got %d\n", avail);
  rdy = Wire.read();
    TRC_PRINT("hud_rdy done %d\n", rdy);
   delay(1);
  return rdy;
}
#endif

#ifdef DECOMP

void doDecomp(unsigned long curr, funclist_t * f, void * ctx) {
  
        TRC_PRINT("dodecomp\n");
        if(vpm_config_ok) {
            getStops();
        }
}

void doND(unsigned long curr, funclist_t * f, void * ctx) {
      TRC_PRINT("donodeco\n");
        if(vpm_config_ok) {
            getND();
        }
}

void gaslist() {
      //char spbuf[30];
      //char tbuf [20];
      int n = 0, m;
      uint8_t val, res;
      bool changed = false;
      init_menubuf(192);

      if (vpm_config_ok) {
           while (1) { 
              int ngas = vpm.getGasListLength();
              gases * gaslist = vpm.getGasList();
              n = 0;
              for (int c = 0; c < ngas; c++) {
                 n += snprintf(menubuf +n, menubuf_size -n, "%d. %-20s: %-6s\n", c+1, gaslist[c].name, gaslist[c].active ? "active" : "");
              }
              n += snprintf(menubuf +n, menubuf_size -n, "%d. %-20s%-8s", ngas+1, "Exit", "");
              disp.setFont(u8g2_font_t0_11b_mr); 
              strcpy_P(titlbuf, titleGasList);
              m = disp.userInterfaceSelectionList(titlbuf, 1, menubuf); 
              if(m == ngas+1) {
                  lastMenu = currMenu;
                  currMenu = 0; 
                 if (changed) {
                  vpm.gasChange();
                  saveVPMConfig();
                  }
                free(menubuf);
                menubuf = NULL;
                return;
              }
               for (int c = 0; c < ngas; c++) {
                if (c == m-1) {
                  changed = !gaslist[c].active;
                  gaslist[c].active  = true;
                }
                else {
                  gaslist[c].active = false;
                }
              }
              
           }
      }
      lastMenu = currMenu;
      currMenu = 0;
}

static bool inVPM = false;

void doVPM(unsigned long curr, funclist_t * f, void * ctx) {
  if (vpm_config_ok) {
    getStops();
    getND();
    if (!inVPM) {
      inVPM = true;
      dive_running = vpm.real_time_dive_loop();
      if (dive_running) {
        logfile_mode = dive;
        if (!test_ready) silent = false;
      }
      else logfile_mode = surface;
      inVPM = false;
    //test_ready = 1;
    }
 }
}

bool loadConfigFileByName(char * fname) {
    DBG_PRINT("%s\n","Loading...");
  File confFile = SPIFFS.open(fname, "r");
  if (!confFile) {
    ERR_PRINT("Failed to open config file %s\n", fname);
    return false;
  }
  size_t size = confFile.size();
  if (size > 8192) {
    ERR_PRINT("config file size is too large\n");
    return false;
  }
  // Allocate a buffer to store contents of the file.
  //std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.

  //confFile.readBytes(buf.get(), size);
  YIELD();
  //StaticJsonBuffer<16384> configBuffer;
  DynamicJsonBuffer  configBuffer; //(size*2);

    
  //JsonObject &conf = configBuffer.parseObject(buf.get());
    JsonObject &conf = configBuffer.parseObject(confFile);
    YIELD();

  if (!conf.success()) {
    ERR_PRINT("Failed to parse config file\n");
      confFile.close();
          return false;
  }
  //conf.prettyPrintTo(Serial);
#ifdef LOGGING
  //conf.prettyPrintTo(logFile);
#endif
  vpm_config_ok = vpm.loadconfig(conf);
  
  if(!vpm_config_ok) {
    ERR_PRINT("Load VPM-B config failed\n");
      confFile.close();

    return false;
  }
  getOxyConfig(conf);
  confFile.close();
  DBG_PRINT("%s\n","Loaded...");
  vpm_data_validate();
  return true;
}

void loadVPMConfig() {
  if (!loadConfigFileByName("/config.json")) {
      YIELD();

    if(!loadConfigFileByName("/config.json.0")) {
        YIELD();

            if(!loadConfigFileByName("/config.json.1")) {

               ERR_PRINT("Failed to load any config");
               dontsave = true;

            }
      }
  }
  lprintf("C,loaded config\n");
  YIELD();
}

void loadTestDives() {
  int size;
  DBG_PRINT("%s\n","Loading test dives...");
  File testDiveFile = SPIFFS.open("/testDive.json", "r");
  if (!testDiveFile) {
    ERR_PRINT("Failed to open test dive file");
    vpm_config_ok = false;
    return;
  }
  
  size = testDiveFile.size();
  if (size > 8192) {
    ERR_PRINT("test dive file size is too large");
    vpm_config_ok = false;

    return;
  }
  //std::unique_ptr<char[]> diveBuf(new char[size]);
  //testDiveFile.readBytes(diveBuf.get(), size);

  DynamicJsonBuffer  diveBuffer;//(size*2);
    
  JsonObject &testDives = diveBuffer.parseObject(testDiveFile);
  if (!testDives.success()) {
    ERR_PRINT("Failed to parse test dive file");
      testDiveFile.close();
      vpm_config_ok = false;

    return;
  }
  testDiveFile.close();
  vpm_config_ok = vpm.loaddives(testDives);

  DBG_PRINT("%s\n","Loaded...");
  YIELD();
#ifdef DEBUG
  testDives.prettyPrintTo(Serial);

  YIELD();
#endif
}

char * enclogo = NULL;


void saveVPMConfig() {  
  //const int size = 4096;
  if (dontsave) {
    ERR_PRINT("Can't save. Config invalid.");
    return;
  }
  DBG_PRINT("%s\n","Saving...");
  DynamicJsonBuffer  configBuffer;
  JsonObject &root = configBuffer.createObject();
  JsonObject &conf = root.createNestedObject("config");
  vpm.saveconfig(conf);
  setOxyConfig(conf);
    DBG_PRINT("%s\n","Created Objects...");
  SPIFFS.remove("/dummy.txt");
   File confFile = SPIFFS.open("/config.json.1", "w");
  if (!confFile) {
    ERR_PRINT("Failed to open config file for writing");
    return;
  }
  root.prettyPrintTo(confFile);
  confFile.close();
  DBG_PRINT ("Now Renaming"); 
  if (SPIFFS.exists("/config.json.0")) SPIFFS.remove("/config.json.0");
  if (!SPIFFS.rename("/config.json", "/config.json.0")) ERR_PRINT("Cannot rename original config"); 
  if(!SPIFFS.rename("/config.json.1", "/config.json")) ERR_PRINT("Cannot rename new config"); 
  File dummy = SPIFFS.open("/dummy.txt", "w");
  if (dummy) {
    char line[81];
    memset(line, '0', 80);
    line[80] = 0;
    for (int c = 0; c < 80; c++) {
         dummy.println(line);  
    }
    dummy.close();
  }
  //root.prettyPrintTo(Serial);
  if (enclogo) {
    free (enclogo);
    enclogo = NULL;
  }
  DBG_PRINT("%s\n","Saved...");
  lprintf("C,saved config\n");

  YIELD();
}

void vpm_data_validate() {
 if (vpm_config_ok) {
  vpm_config_ok = vpm.validate();
   if(vpm_config_ok)  DBG_PRINT("%s\n","Validated...");
    else ERR_PRINT("Validation failed...");
    YIELD();
 } else {
  ERR_PRINT("Data invalid before validation");
 }
}

void vpm_test_run() {
   if (vpm_config_ok) {

  DBG_PRINT("%s\n","Running...");
  vpm.real_time_dive();
  YIELD();
  DBG_PRINT("%s\n","Done...");
  vpm.output_dive_state();
   }
}

#endif

void doi2cPing(unsigned long curr, funclist_t * f, void * ctx) {
  initi2cA();
  initi2cB();
  TRC_PRINT("doi2cping done\n");
}

#ifdef ARDUINO_ARCH_ESP8266

const char* host = NULL; //"oxygauge-webupdate";
const char* ssid = NULL; //"TP-LINK_719B";
const char* password = NULL; //"63953145";
const char* AP_ssid = NULL; //"TP-LINK_719B";
const char* AP_password = NULL; //"63953145";

const char* user = NULL; //"admin";
const char* upwd = NULL; //"thingy";


ESP8266WebServer server(80);
//const char* serverIndex = "
//IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
const char* ntpServerName = NULL; //"uk.pool.ntp.org";


File fsUploadFile;
bool do_reload = false;
//File web server


//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  else if(filename.endsWith(".json")) return "application/json";
  return "text/plain";
}

bool handleFileRead(String path){
  DBG_PRINT("%s%s\n","handleFileRead: ", path.c_str());
  if(path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload(){
   if(!server.authenticate(user, upwd)) {
          return server.requestAuthentication();
      }
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String uploadFileName = upload.filename;
    if(!uploadFileName.startsWith("/")) uploadFileName = "/"+uploadFileName;
    if (uploadFileName.endsWith("config.json")) {
          do_reload = true;
     }
    DBG_PRINT("handleFileUpload Name: %s\n", uploadFileName.c_str()); 
    fsUploadFile = SPIFFS.open(uploadFileName, "w");
    uploadFileName = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    //DBG_PRINT("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
     if(do_reload) {
      loadVPMConfig();
      do_reload = false;
     }
    DBG_PRINT("handleFileUpload Size: "); 
    DBG_PRINT("%d\n",upload.totalSize);
  }
}

void handleFileDelete(){
    if(!server.authenticate(user, upwd))
      return server.requestAuthentication();
  if(server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  DBG_PRINT("%s%s\n","handleFileDelete: ", path.c_str());
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  if(SPIFFS.remove(path)) server.send(200, "text/plain", "");
  else   server.send(500, "text/plain", "DELETE FAILED");

  path = String();
}

void handleFileCreate(){
    if(!server.authenticate(user, upwd))
      return server.requestAuthentication();
  if(server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  DBG_PRINT("%s%s\n","handleFileCreate: ", path.c_str());
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if(file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}

void handleFormat() {
  if(!server.authenticate(user, upwd))
      return server.requestAuthentication();
   if(!server.hasArg("confirm") || !server.arg("confirm").equals("yes")) {server.send(500, "text/plain", "BAD ARGS"); return;}
   DBG_PRINT("%s%s\n","handleFormt: ");
   SPIFFS.format();

   server.send(200, "text/plain", "OK");
}

void handleFileList() {
  if(!server.authenticate(user, upwd))
      return server.requestAuthentication();
  if(!server.hasArg("dir")) {server.send(500, "text/plain", "BAD ARGS"); return;}
  
  String path = server.arg("dir");
  DBG_PRINT("%s%s\n","handleFileList: ",  path.c_str());
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    String fname =  String(entry.name()).substring(1);
    size_t fileSize = entry.size();

    fname.replace("\"","\\\"");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += fname;
    output += "\",\"size\":\"";
    output += formatBytes(fileSize);
    output += "\"}";
    entry.close();
  }
  
  output += "]";
  server.send(200, "text/json", output);
}
void handleRemoteFirmwareInstall(){
     if(!server.authenticate(user, upwd))
        return server.requestAuthentication();
      server.sendHeader("Connection", "close");
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(200, "text/plain", (Update.hasError())?"FAIL":"OK");
      ESP.restart();
}

void handleRemoteFirmwareUpload(){
   if(!server.authenticate(user, upwd)) {
          return server.requestAuthentication();
      }
      HTTPUpload& upload = server.upload();
      if(upload.status == UPLOAD_FILE_START){
        Serial.setDebugOutput(true);
        WiFiUDP::stopAll();
#ifdef DEBUG 
        DBG_PRINT("Update: %s\n", upload.filename.c_str());
#endif

        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if(!Update.begin(maxSketchSpace)){//start with max available size
          Update.printError(Serial);
        }
      } else if(upload.status == UPLOAD_FILE_WRITE){
        if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
          Update.printError(Serial);
        }
      } else if(upload.status == UPLOAD_FILE_END){
        if(Update.end(true)){ //true to set the size to the current progress
#ifdef DEBUG 
          DBG_PRINT("Update Success: %u\nRebooting...\n", upload.totalSize);
#endif
        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
      yield();
}

void handleSPIFFSFirmwareInstall () {
  if(!server.authenticate(user, upwd))
      return server.requestAuthentication();
  if(server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  DBG_PRINT("%s%s\n","FirmwareInstall: ", path.c_str());
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  //if(path.endsWith("/")) path += "index.html";
  //String contentType = getContentType(path);
  if(SPIFFS.exists(path) && path.endsWith(".bin")){
    File file = SPIFFS.open(path, "r");
    Serial.setDebugOutput(true);
    WiFiUDP::stopAll();
#ifdef DEBUG 
     DBG_PRINT("Update File: %s\n",path.c_str());
#endif

     uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
     if(!Update.begin(maxSketchSpace)){//start with max available size
          Update.printError(Serial);
           return server.send(500, "text/plain", "INSTALL FAILED");

      }
      uint8_t * buf = (uint8_t *) malloc (8192);
      if(!buf) {
          return server.send(500, "text/plain", "INSTALL FAILED");

      }
      while(file.available()) {
        int nread = file.read(buf, 8192);
        if(Update.write(buf,nread) != nread){
          Update.printError(Serial);
          return server.send(500, "text/plain", "INSTALL FAILED");
        }
        yield();
      }
      if(Update.end(true)){ 
        file.close();
        free(buf);  
        server.send(200, "text/plain", "UPDATE OK REBOOTING");
        ESP.restart();
        return;
      } else {
                Update.printError(Serial);
                file.close();
                free(buf);
                return server.send(500, "text/plain", "INSTALL FAILED");
      }
      
  }
  return server.send(404, "text/plain", "FILE NOT FOUND OR WRONG TYPE");
}


//int got_wifi = 0;

void drawWifi(int wid, int y) {
    if(got_wifi) disp.drawXBMP(wid - ( wifi_width + battery_width +5), y +1, wifi_width, wifi_height, (const unsigned char*)pgm_read_dword(&(wifis[wifiIdx])));

}


void doSysLog(){
    if (syslogHost) {
    Syslog.setLoghost(syslogHost);
      Syslog.setOwnHostname(host);
      Syslog.setLog(LOG_USER, LOG_INFO);
      Syslog.enable(true);
      Syslog.setLogLevel(LOG_INFO);
      Syslog.setMsgId(112);
      Syslog.setApp("OxyGauge");
    }
}
void OTAinit (bool is_ap) {
  //disp.clear();
    if (AP_ssid == NULL || strlen(AP_ssid) == 0) {
      AP_ssid = "OxyGauge"; 
     }
     if (AP_password == NULL || strlen(AP_password) == 0) {
      AP_password = "OxyGauge";
      
     }
     if (host == NULL || strlen (host) == 0 ) {
      host = "oxygauge-webupdate";
     }
     if (user == NULL || strlen (user) == 0 ) {
      user = "admin";
     }
     if (upwd == NULL || strlen (upwd) == 0 ) {
      upwd = "OxyGauge";
     }
     if (ntpServerName == NULL || strlen (ntpServerName) == 0 ) {
      ntpServerName = "time.nist.gov";
     }
  if(!is_ap) {
    WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);

  } else {
     WiFi.mode(WIFI_AP);
     WiFi.softAP(AP_ssid, AP_password);
  }
  OTAWait(is_ap);
}

void OTAWait(bool is_ap) 
{
 int  progress = 1;

  while (WiFi.status() != WL_CONNECTED && !is_ap) {
      YIELD();
      DBG_PRINT(".");
      FIRST_PAGE();
       do {
        disp.setFont(u8g2_font_t0_11b_mr);  
         const char  wf [] = "WiFi";
          disp.drawStr( (256 / 2) - (disp.getStrWidth(wf)/2),(64/2 - 11/2), wf);
          disp.drawFrame(256/8, 64/2 + 11, 256 - 2*(256/8), 11);
          disp.drawBox(256/8 +2, 64/2 + 11 +2, round(((256 - 2*(256/8)) - 4)*((float)progress/100.0)), 11-4);
        } while (NEXT_PAGE());
        progress += 5;
        if (progress > 100) progress = 1;
        delay(50);
  }
  if(WiFi.status() == WL_CONNECTED || is_ap) {
    MDNS.begin(host);
     server.on("/", HTTP_GET, [](){
      if(!server.authenticate(user, upwd)) {
          server.requestAuthentication();
      }
      if(!handleFileRead("/edit.html")) server.send(404, "text/plain", "FileNotFound");
  });
    //server.on("/", HTTP_GET, [](){
    //   if(!server.authenticate(user, upwd))
    //  return server.requestAuthentication();
    //  server.sendHeader("Connection", "close");
    //  server.sendHeader("Access-Control-Allow-Origin", "*");
    //  server.send(200, "text/html", serverIndex);
    //});
    server.on("/update", HTTP_POST, handleRemoteFirmwareInstall, handleRemoteFirmwareUpload);
    //SERVER INIT
  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  //install firmware form spiffs
   server.on("/format", HTTP_GET, handleFormat);
  //load editor
  server.on("/edit", HTTP_GET, [](){
      if(!server.authenticate(user, upwd)) {
          server.requestAuthentication();
      }
    if(!handleFileRead("/edit.html")) server.send(404, "text/plain", "FileNotFound");
  });
  server.on("/install", HTTP_GET, handleSPIFFSFirmwareInstall);

  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, [](){ 
      if(!server.authenticate(user, upwd))
        return server.requestAuthentication();
      server.send(200, "text/plain", ""); 
     }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([](){
      if(!server.authenticate(user, upwd)) {
          server.requestAuthentication();
      }
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });
#ifdef DEBUG 
    DBG_PRINT("%s\n","Starting NTP");
#endif
    myNTP.setServerName(ntpServerName);
    myNTP.begin();
    
    server.begin();
    MDNS.addService("http", "tcp", 80);
    doSysLog();
#ifdef DEBUG 
    DBG_PRINT("Ready! Open http://%s.local in your browser\n", host);
#endif
    got_wifi = true;
  } else {
    got_wifi =  false;
#ifdef DEBUG 
    DBG_PRINT("%s\n","WiFi Failed");
#endif
  }
}

void setRSSI() {
      int rssi = WiFi.RSSI();
      //DBG_PRINT("%d\n",rssi);
              if (rssi >= -30) wifiIdx = 4;
              else if (rssi >= -67) wifiIdx = 4;
              else if (rssi >= -70) wifiIdx = 3;
              else if (rssi >= -80) wifiIdx = 2;
              else if (rssi >= -90) wifiIdx = 1;
              else wifiIdx = 0;
}
bool wifi_bkgnd = false;
void doBkgndWifi(unsigned long curr, funclist_t * f, void * ctx) {
      if(WiFi.status() != WL_CONNECTED) got_wifi = false;
       if (got_wifi) {
              setRSSI();
              server.handleClient();
              myNTP.doNTP();
       }
}

void disconnectWifi() {
    server.close();
     server.stop();
      WiFi.disconnect(true);
    got_wifi = false;
    wifiIdx = 0;
}

int wifiNetSelScan() {
  int n = 0;
  init_menubuf(384);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);
  got_wifi = false;
  //Serial.println("scan start");

  // WiFi.scanNetworks will return the number of networks found
  int nnet = WiFi.scanNetworks();
  for (int c = 0; c < nnet; c++) {
    n += snprintf(menubuf +n, menubuf_size -n, "%d. %-20s: %d: %s\n", c+1, WiFi.SSID(c).c_str(), WiFi.RSSI(c), (WiFi.encryptionType(c) == ENC_TYPE_NONE) ? " " : "*");
  }
  n += snprintf(menubuf +n, menubuf_size -n, "%d. %-20s%-8s", nnet+1, "Exit", "");
  disp.setFont(u8g2_font_t0_11b_mr); 
  strcpy_P(titlbuf, titleWiFiSel);
  int m = disp.userInterfaceSelectionList(titlbuf, 1, menubuf); 
  free(menubuf);
  menubuf = NULL;
  if(m == nnet+1) {
     lastMenu = currMenu;
     currMenu = 0;
     wifiIdx = 0;
     return 0;
  } 
  m--;
  ssid = strdupnull(WiFi.SSID(m).c_str());
  password = "";
  OTAWait(false);
  return 1;
}

void OTAloop (bool is_ap) {
  int8_t menu = 0, t;
  int nchrs = 0;
   const int wid = 256;
    const int ht = 64;
  init_menubuf(64);

  while (menu == 0) {
      setRSSI();
 
      server.handleClient();
    FIRST_PAGE();
    do { 
      t = disp.getMenuEvent();
      if (t != 0) menu = t; 
      YIELD();
      if(got_wifi) {
          const char swr [] = "WiFi Ready";
          const char awr [] = "WiFi AP Ready";
          const char * wr = is_ap ? awr : swr; 
          drawBatt(wid, 0);
          drawWifi(wid, 0);
          disp.drawStr( (wid / 2) - (disp.getStrWidth(wr)/2),(ht/2 - 11/2), wr);
          nchrs = snprintf (menubuf, menubuf_size, "http://%s.local", host);
          //DBG_PRINT ("nchrs =");
          //DBG_PRINT("%d\n",nchrs);
          disp.drawStr( (wid / 2) - (disp.getStrWidth(menubuf)/2),(ht/2 - 11/2) + 12, menubuf);
          nchrs = 0;
          if (myNTP.haveNTP()) {
              nchrs = snprintf (menubuf, menubuf_size, "NTP ");
          }
          if (timeStatus() == timeSet) { 
              nchrs += NTP::digitalClockString(menubuf +nchrs, menubuf_size -nchrs, now());
          }
          if (nchrs > 0) {
          //DBG_PRINT ("nchrs2 =");
          //DBG_PRINT("%d\n",nchrs);
             disp.drawStr( (256 / 2) - (disp.getStrWidth(menubuf)/2),(ht/2 - 11/2) + 24, menubuf);
          }
      } else{
        const char wff [] =  "WiFi Failed";
          disp.drawStr( (256 / 2) - (disp.getStrWidth(wff)/2),(ht/2 - 11/2), wff);
      }
    } while (NEXT_PAGE());
      t = disp.getMenuEvent();
      if (t != 0) menu = t; 
 
      //delay(1);
      
      switch(menu) {
         case  U8X8_MSG_GPIO_MENU_SELECT :
            lastMenu = currMenu;
            currMenu = 0;
            break;
         case U8X8_MSG_GPIO_MENU_NEXT: 
         case U8X8_MSG_GPIO_MENU_PREV:
         default:
              menu = 0;
              break;
      }
      t = disp.getMenuEvent();
      if (t != 0) menu = t; 
      YIELD();
      myNTP.doNTP();
  }
  disp.setFont(u8g2_font_t0_11b_mr); 
  if(got_wifi) { 
      t = disp.userInterfaceMessage("WiFi", "Leave WiFi Running?" , "", " Ok \n Cancel ");
       if (t == 1) {
          if (!wifi_bkgnd) {
            yield_add(NAMEOF(doBkgndWifi), NULL, 250, 0, false, true);
            wifi_bkgnd = true;
          }
       } else {
          disconnectWifi();
       }
  }
}

void menuDeleteDiveLogs() {
   disp.setFont(u8g2_font_t0_11b_mr); 
    int  t = disp.userInterfaceMessage("Dive Logs", "Delete All Dive Logs?" , "", " Cancel \n OK ");
       if (t == 2) {
          deleteDiveLogs();
       };
}

void getOxyConfig (JsonObject &root) {
  JsonObject &conf = root["config"];
  JsonObject &oxy = conf["oxygauge"];
  saltwater = oxy["saltwater"];
  JsonArray &cal = oxy["calibrations"];
  for (int i = 0; i < cal.size(); i++) {
    scales[i] = cal[i];
  }
  currSetpoint = oxy["curr_setpoint"];
  JsonArray &setps = conf["setpoints"];
  for (int i = 0; i < setps.size(); i++) {
    setpoints[i] = setps[i];
  }
  
  JsonObject &pid = oxy["pid"];
  kp = pid["kp"];
  ki = pid["ki"];
  kd = pid["kd"];
  
  autopower = oxy["autopower"];
  //Don't set this if the log system is already initialized
  //Otherwise it remembers the last file written at a mode change -
  //Dive surface change
  if(!lastlogfilename) lastlogfilename = strdupnull(oxy["last_logfile"]);
  calFrac[2] = oxy["cal_custom"];
  orientation = oxy["orientation"];
  host = strdupnull(oxy["host"]);
  ssid = strdupnull(oxy["SSID"]);
  
  password = strdupnull(oxy["wifi_password"]);
  AP_ssid  = strdupnull(oxy["AP_SSID"]);
  AP_password = strdupnull(oxy["AP_password"]);
  user = strdupnull(oxy["user"]);
  upwd = strdupnull(oxy["password"]);
  syslogHost = strdupnull(oxy["syslog"]);
 ntpServerName = strdupnull(oxy["NTP"]);
  newLogo_width = oxy["splash_width"];
  newLogo_height = oxy["splash_height"];

  const char * nlogo = oxy["splash"];
  int nlogos = 0;
  if(nlogo) {
    nlogos = strlen(nlogo);
    newLogoSize = base64_dec_len(nlogo, nlogos);
  } else {
    newLogoSize = 0;
  }
  //JsonArray &nlogo = oxy["splash"];
  //newLogoSize = nlogo.size();
  //DBG_PRINT("LOGO SIZE ");
  //DBG_PRINT("%s\n",newLogoSize);
  if (newLogoSize > 0) {
    if(newLogo) free (newLogo);
    newLogo = (uint8_t *)malloc(newLogoSize);
    if (!newLogo) {
      lprintf("M,%s,no memory\n", __func__);
      newLogoSize = 0;
    } else {
      base64_decode((char *)newLogo, nlogo, nlogos);
    }
    //for (int i = 0; i < newLogoSize ; i ++) {
    //  newLogo[i] = nlogo[i];
    //}
  }
  DBG_PRINT("SSID %s\n",ssid);


}


void setOxyConfig (JsonObject &conf) {
  //JsonObject &conf = root.createNestedObject("config");
  JsonObject &oxy = conf.createNestedObject("oxygauge");
  oxy["saltwater"] =   saltwater;

  JsonArray &cal = oxy.createNestedArray("calibrations");
  for (int i = 0; i < 3; i++) {
    cal.add(scales[i]);
  }
  oxy["curr_setpoint"] = currSetpoint;
  JsonArray &setps = conf.createNestedArray("setpoints");
  for (int i = 0; i < 2; i++) {
    setps.add(setpoints[i]);
  }
  
  JsonObject &pid = oxy.createNestedObject("pid");
  pid["kp"] = kp;
  pid["ki"] = ki;
  pid["kd"] = kd;
  
  oxy["autopower"] = autopower;
  oxy["last_logfile"] = lastlogfilename;

  oxy["cal_custom"] = calFrac[2];
  oxy["orientation"] = orientation;
  oxy["host"] = host;
  oxy["SSID"] = ssid;
  
  oxy["wifi_password"] = password;
  oxy["AP_SSID"] = AP_ssid;
  oxy["AP_password"] = AP_password;
  oxy["user"] = user;
  oxy["password"] = upwd;
  oxy["syslog"] = syslogHost;
  oxy["NTP"] = ntpServerName;
  int siz = 0;
  uint8_t * logo = NULL;
  if (newLogo) {
    oxy["splash_width"] = newLogo_width;
    oxy["splash_height"] = newLogo_height;
    siz = newLogoSize;
    logo = newLogo;
  } else {
      oxy["splash_width"] =  OxyTitle_width;
      oxy["splash_height"] =  OxyTitle_height;
      siz = ((OxyTitle_width +7)/8) *OxyTitle_height;
      logo = (uint8_t *) malloc(siz);
      if (!logo) {
        lprintf("M,%s,no memory\n", __func__);
        siz = 0;
      } else {
        memcpy_P(logo, OxyTitle_bits, siz);
      }
  }
   if (siz) {
    int nlogos = base64_enc_len(siz);
   
    enclogo = (char *)malloc(nlogos);
    base64_encode(enclogo, (const char *)logo, siz); 
    oxy["splash"] = enclogo;
   } else {
     oxy["splash"] = "";
   }
    //JsonArray &nlogo = oxy.createNestedArray("splash");
    //for (int i = 0; i < newLogoSize ; i ++) {
    //  nlogo.add(newLogo[i]);
    //}
    if (newLogo == NULL && logo) free (logo);
}
#endif

/*
long readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both

  long result = (high<<8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}
*/

