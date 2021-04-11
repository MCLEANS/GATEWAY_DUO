// Language config
#define CURRENT_LANG INTL_LANG

// Wifi config
const char WLANSSID[] PROGMEM = "Freifunk-disabled";
const char WLANPWD[] PROGMEM = "";

// BasicAuth config
const char WWW_USERNAME[] PROGMEM = "admin";
const char WWW_PASSWORD[] PROGMEM = "feinstaub";
#define WWW_BASICAUTH_ENABLED 0

// Sensor Wifi config (config mode)
#define FS_SSID "GATEWAY_DUO-"
#define FS_PWD ""

// Where to send the data?
#define SEND2CFA 1
#define SSL_CFA 0
#define SEND2SENSORCOMMUNITY 0
#define SSL_SENSORCOMMUNITY 0
#define SEND2MADAVI 1
#define SSL_MADAVI 0
#define SEND2SENSEMAP 0
#define SEND2FSAPP 0
#define SEND2AIRCMS 0
#define SEND2MQTT 0
#define SEND2INFLUX 0
#define SEND2LORA 0
#define SEND2CSV 0
#define SEND2SD 1
#define SEND2CUSTOM 0

// OpenSenseMap
#define SENSEBOXID ""

enum LoggerEntry {
    LoggerCFA,
    LoggerSensorCommunity,
    LoggerMadavi,
    LoggerSensemap,
    LoggerFSapp,
    Loggeraircms,
    LoggerInflux,
    LoggerCustom,
    LoggerCount
};

struct LoggerConfig {
    uint16_t destport;
    uint16_t _unused;
#if defined(ESP8266)
    BearSSL::Session* session;
#else
    void* session;
#endif
};

// IMPORTANT: NO MORE CHANGES TO VARIABLE NAMES NEEDED FOR EXTERNAL APIS
static const char HOST_CFA[] PROGMEM = "api.sensors.africa";
static const char URL_CFA[] PROGMEM = " /v1/push-sensor-data/";
#define PORT_CFA 80

static const char HOST_MADAVI[] PROGMEM = "api-rrd.madavi.de";
static const char URL_MADAVI[] PROGMEM = "/data.php";
#define PORT_MADAVI 80

static const char HOST_SENSORCOMMUNITY[] PROGMEM = "api.sensor.community";
static const char URL_SENSORCOMMUNITY[] PROGMEM = "/v1/push-sensor-data/";
#define PORT_SENSORCOMMUNITY 80

static const char HOST_SENSEMAP[] PROGMEM = "ingress.opensensemap.org";
static const char URL_SENSEMAP[] PROGMEM = "/boxes/{v}/data?luftdaten=1";
#define PORT_SENSEMAP 443

static const char HOST_FSAPP[] PROGMEM = "h2801469.stratoserver.net";
static const char URL_FSAPP[] PROGMEM = "/data.php";
#define PORT_FSAPP 443

static const char HOST_AIRCMS[] PROGMEM = "doiot.ru";
static const char URL_AIRCMS[] PROGMEM = "/php/sensors.php?h=";
// As of 2019/09 uses invalid certifiates on ssl/port 443 and does not support Maximum Fragment Length Negotiation (MFLN)
// So we can not use SSL
#define PORT_AIRCMS 80

static const char FW_DOWNLOAD_HOST[] PROGMEM = "firmware.sensor.community";
#define FW_DOWNLOAD_PORT 443

static const char FW_2ND_LOADER_URL[] PROGMEM = OTA_BASENAME "/loader-002.bin";

static const char NTP_SERVER_1[] PROGMEM = "0.pool.ntp.org";
static const char NTP_SERVER_2[] PROGMEM = "1.pool.ntp.org";

// define own API
static const char HOST_CUSTOM[] PROGMEM = "192.168.234.1";
static const char URL_CUSTOM[] PROGMEM = "/data.php";
#define PORT_CUSTOM 80
#define USER_CUSTOM ""
#define PWD_CUSTOM ""
#define SSL_CUSTOM 0

// define own InfluxDB
static const char HOST_INFLUX[] PROGMEM = "ec2-34-250-53-214.eu-west-1.compute.amazonaws.com";
static const char URL_INFLUX[] PROGMEM = "/write?db=airquality";
#define PORT_INFLUX 8086
#define USER_INFLUX ""
#define PWD_INFLUX ""
static const char MEASUREMENT_NAME_INFLUX[] PROGMEM = " ";
#define SSL_INFLUX 0

//  pin assignments for NodeMCU V2 board
#if defined(ESP8266)
// define pin for one wire sensors
#define ONEWIRE_PIN D7

// define serial interface pins for particle sensors
// Serial confusion: These definitions are based on SoftSerial
// TX (transmitting) pin on one side goes to RX (receiving) pin on other side
// SoftSerial RX PIN is D1 and goes to SDS TX
// SoftSerial TX PIN is D2 and goes to SDS RX
#define PM_SERIAL_RX D0
#define PM_SERIAL_TX D0

//define pins for ATMEGA328P
#define ATMEGA_RX D4
#define ATMEGA_TX D3

// define pins for status LEDs
#define GPS_LED P0
#define LOGGER_LED P1
#define RTC_LED P2
#define MIC_LED P3
#define PMS_LED P4
#define DHT_LED P5
#endif

// Device is WiFi Enabled
#define WIFI_ENABLED 0

// DHT22, temperature, humidity
#define DHT_READ 1
#define DHT_TYPE DHT22
#define DHT_API_PIN 7

// PMS1003, PMS300, 3PMS5003, PMS6003, PMS7003
#define PMS_READ 1
#define PMS_API_PIN 1

// Show wifi info on displays
#define DISPLAY_WIFI_INFO 1

// Show device info on displays
#define DISPLAY_DEVICE_INFO 1

// automatic firmware updates
#define AUTO_UPDATE 0

// use beta firmware
#define USE_BETA 0

// Set debug level for serial output?
#define DEBUG 3