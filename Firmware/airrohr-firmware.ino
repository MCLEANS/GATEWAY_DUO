/************************************************************************
 *                                                                      *
 *  This source code needs to be compiled for the board                 *
 *  NodeMCU 1.0 (ESP-12E Module)                                        *
 *                                                                      *
 ************************************************************************
 *                                                                      *
 *    airRohr firmware                                                  *
 *    Copyright (C) 2016-2020  Code for Stuttgart a.o.                  *
 *    Copyright (C) 2019-2020  Dirk Mueller                             *
 *                                                                      *
 * This program is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program. If not, see <http://www.gnu.org/licenses/>. *
 *                                                                      *
 ************************************************************************
 * OK LAB Particulate Matter Sensor                                     *
 *      - nodemcu-LoLin board                                           *
 *      - Nova SDS0111                                                  *
 *  http://inovafitness.com/en/Laser-PM2-5-Sensor-SDS011-35.html        *
 *                                                                      *
 * Wiring Instruction see included Readme.md                            *
 *                                                                      *
 ************************************************************************
 *                                                                      *
 * Alternative                                                          *
 *      - nodemcu-LoLin board                                           *
 *                                                                      *
 * Wiring Instruction:                                                  *
 *      Pin 2 of dust sensor PM2.5 -> Digital 6 (PWM)                   *
 *      Pin 3 of dust sensor       -> +5V                               *
 *      Pin 4 of dust sensor PM1   -> Digital 3 (PMW)                   *
 *                                                                      *
 *                                                                      *
 ************************************************************************
 *                                                                      *
 * Please check Readme.md for other sensors and hardware                *
 *                                                                      *
 ************************************************************************
 *
 * latest mit lib 2.6.1
 * DATA:    [====      ]  40.7% (used 33316 bytes from 81920 bytes)
 * PROGRAM: [=====     ]  49.3% (used 514788 bytes from 1044464 bytes)

 * latest mit lib 2.5.2
 * DATA:    [====      ]  39.4% (used 32304 bytes from 81920 bytes)
 * PROGRAM: [=====     ]  48.3% (used 504812 bytes from 1044464 bytes)
 *
 ************************************************************************/
#include <WString.h>
#include <pgmspace.h>

// increment on change
#define SOFTWARE_VERSION_STR "NRZ-2020-129"
String SOFTWARE_VERSION(SOFTWARE_VERSION_STR);

/*****************************************************************
 * Includes                                                      *
 *****************************************************************/

#if defined(ESP8266)
#include <FS.h>                     // must be first
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <SoftwareSerial.h>
#include <Hash.h>
#include <ctime>
#include <coredecls.h>
#include <sntp.h>
#endif

#if defined(ESP32)
#define FORMAT_SPIFFS_IF_FAILED true
#include <FS.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HardwareSerial.h>
#include <hwcrypto/sha.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#endif

// includes common to ESP8266 and ESP32 (especially external libraries)
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include "./DHT.h"
#include <StreamString.h>

#include "intl_en.h"

#include "defines.h"
#include "ext_def.h"
#include "html-content.h"
#include "ca-root.h"

/******************************************************************
 * Constants                                                      *
 ******************************************************************/
constexpr unsigned SMALL_STR = 64-1;
constexpr unsigned MED_STR = 256-1;
constexpr unsigned LARGE_STR = 512-1;
constexpr unsigned XLARGE_STR = 1024-1;

#define RESERVE_STRING(name, size) String name((const char*)nullptr); name.reserve(size)

const unsigned long SAMPLETIME_MS = 30000;									// time between two measurements of the PPD42NS
const unsigned long SAMPLETIME_SDS_MS = 1000;								// time between two measurements of the SDS011, PMSx003, Honeywell PM sensor
const unsigned long WARMUPTIME_SDS_MS = 15000;								// time needed to "warm up" the sensor before we can take the first measurement
const unsigned long READINGTIME_SDS_MS = 5000;								// how long we read data from the PM sensors
const unsigned long SAMPLETIME_GPS_MS = 50;
const unsigned long DISPLAY_UPDATE_INTERVAL_MS = 5000;						// time between switching display to next "screen"
const unsigned long ONE_DAY_IN_MS = 24 * 60 * 60 * 1000;
const unsigned long PAUSE_BETWEEN_UPDATE_ATTEMPTS_MS = ONE_DAY_IN_MS;		// check for firmware updates once a day
const unsigned long DURATION_BEFORE_FORCED_RESTART_MS = ONE_DAY_IN_MS * 28;	// force a reboot every ~4 weeks

/******************************************************************
 * The variables inside the cfg namespace are persistent          *
 * configuration values. They have defaults which can be          *
 * configured at compile-time via the ext_def.h file              *
 * They can be changed by the user via the web interface, the     *
 * changes are persisted to the flash and read back after reboot. *
 * Note that the names of these variables can't be easily changed *
 * as they are part of the json format used to persist the data.  *
 ******************************************************************/
namespace cfg {
	unsigned debug = DEBUG;

	unsigned time_for_wifi_config = 600000;
	unsigned sending_intervall_ms = 30000;

	char current_lang[3];

	// credentials for basic auth of internal web server
	bool www_basicauth_enabled = WWW_BASICAUTH_ENABLED;
	char www_username[LEN_WWW_USERNAME];
	char www_password[LEN_CFG_PASSWORD];

	// wifi credentials
	char wlanssid[LEN_WLANSSID];
	char wlanpwd[LEN_CFG_PASSWORD];

	// credentials of the sensor in access point mode
	char fs_ssid[LEN_FS_SSID] = FS_SSID;
	char fs_pwd[LEN_CFG_PASSWORD] = FS_PWD;

	// (in)active sensors
	bool dht_read = DHT_READ;
	bool pms_read = PMS_READ;

	// send to "APIs"
	bool send2cfa = SEND2CFA;
	bool send2dusti = SEND2SENSORCOMMUNITY;
	bool send2madavi = SEND2MADAVI;
	bool send2sensemap = SEND2SENSEMAP;
	bool send2fsapp = SEND2FSAPP;
	bool send2aircms = SEND2AIRCMS;
	bool send2custom = SEND2CUSTOM;
	bool send2influx = SEND2INFLUX;
	bool send2csv = SEND2CSV;
	bool send2sd = SEND2SD;

	bool auto_update = AUTO_UPDATE;
	bool use_beta = USE_BETA;

	bool wifi_enabled = WIFI_ENABLED;

	bool display_wifi_info = DISPLAY_WIFI_INFO;
	bool display_device_info = DISPLAY_DEVICE_INFO;

	// API settings
	bool ssl_cfa = SSL_CFA;
	bool ssl_madavi = SSL_MADAVI;
	bool ssl_dusti = SSL_SENSORCOMMUNITY;
	char senseboxid[LEN_SENSEBOXID] = SENSEBOXID;

	char host_influx[LEN_HOST_INFLUX];
	char url_influx[LEN_URL_INFLUX];
	unsigned port_influx = PORT_INFLUX;
	char user_influx[LEN_USER_INFLUX] = USER_INFLUX;
	char pwd_influx[LEN_CFG_PASSWORD] = PWD_INFLUX;
	char measurement_name_influx[LEN_MEASUREMENT_NAME_INFLUX];
	bool ssl_influx = SSL_INFLUX;

	char host_custom[LEN_HOST_CUSTOM];
	char url_custom[LEN_URL_CUSTOM];
	bool ssl_custom = SSL_CUSTOM;
	unsigned port_custom = PORT_CUSTOM;
	char user_custom[LEN_USER_CUSTOM] = USER_CUSTOM;
	char pwd_custom[LEN_CFG_PASSWORD] = PWD_CUSTOM;

	void initNonTrivials(const char* id) {
		strcpy(cfg::current_lang, CURRENT_LANG);
		strcpy_P(www_username, WWW_USERNAME);
		strcpy_P(www_password, WWW_PASSWORD);
		strcpy_P(wlanssid, WLANSSID);
		strcpy_P(wlanpwd, WLANPWD);
		strcpy_P(host_custom, HOST_CUSTOM);
		strcpy_P(url_custom, URL_CUSTOM);
		strcpy_P(host_influx, HOST_INFLUX);
		strcpy_P(url_influx, URL_INFLUX);
		strcpy_P(measurement_name_influx, MEASUREMENT_NAME_INFLUX);

		if (!*fs_ssid) {
			strcpy(fs_ssid, SSID_BASENAME);
			strcat(fs_ssid, id);
		}
		else{
			strcat(fs_ssid, id);
		}
	}
}

#define JSON_BUFFER_SIZE 2300

enum class PmSensorCmd {
	Start,
	Stop,
	ContinuousMode
};

LoggerConfig loggerConfigs[LoggerCount];

long int sample_count = 0;
bool htu21d_init_failed = false;
bool bmp_init_failed = false;
bool bmx280_init_failed = false;
bool sht3x_init_failed = false;
bool dnms_init_failed = false;
bool gps_init_failed = false;
bool rtc_init_failed = false;
bool airrohr_selftest_failed = false;

#if defined(ESP8266)
ESP8266WebServer server(80);
#endif
#if defined(ESP32)
WebServer server(80);
#endif

#include "./airrohr-cfg.h"


/*****************************************************************
 * SDS011 declarations                                           *
 *****************************************************************/
#if defined(ESP8266)
SoftwareSerial serialSDS;
#endif
#if defined(ESP32)
#define serialSDS (Serial1)
#endif


/****************************************************************
 * ATMEGA328P declaration
 * **************************************************************/
SoftwareSerial atmega328p;

/*****************************************************************
 * DHT declaration                                               *
 *****************************************************************/
DHT dht(ONEWIRE_PIN, DHT_TYPE);

/*****************************************************************
 * Variable Definitions for PPD24NS                              *
 * P1 for PM10 & P2 for PM25                                     *
 *****************************************************************/

boolean trigP1 = false;
boolean trigP2 = false;
unsigned long trigOnP1;
unsigned long trigOnP2;

unsigned long lowpulseoccupancyP1 = 0;
unsigned long lowpulseoccupancyP2 = 0;

bool send_now = false;
unsigned long starttime;
unsigned long time_point_device_start_ms;
unsigned long starttime_SDS;
unsigned long starttime_GPS;
unsigned long act_micro;
unsigned long act_milli;
unsigned long last_micro = 0;
unsigned long min_micro = 1000000000;
unsigned long max_micro = 0;

bool is_PMS_running = true;

unsigned long sending_time = 0;
unsigned long last_update_attempt;
int last_update_returncode;
int last_sendData_returncode;

float last_value_DHT_T = -128.0;
float last_value_DHT_H = -1.0;


int pms_pm1_sum = 0;
int pms_pm10_sum = 0;
int pms_pm25_sum = 0;
int pms_val_count = 0;
int pms_pm1_max = 0;
int pms_pm1_min = 20000;
int pms_pm10_max = 0;
int pms_pm10_min = 20000;
int pms_pm25_max = 0;
int pms_pm25_min = 20000;
bool readPMSFromAtmega = true;

float last_value_PMS_P0 = -1.0;
float last_value_PMS_P1 = -1.0;
float last_value_PMS_P2 = -1.0;

String last_data_string;
int last_signal_strength;
bool readGPSFromAtmega = true;

bool readDHTFromAtmega = true;


String esp_chipid;
String last_value_SDS_version;

unsigned long SDS_error_count;
unsigned long sendData_error_count;
unsigned long WiFi_error_count;

unsigned long last_page_load = millis();

bool wificonfig_loop = false;
uint8_t sntp_time_set;

unsigned long count_sends = 0;
unsigned long last_display_millis = 0;
uint8_t next_display_count = 0;

struct struct_wifiInfo {
	char ssid[LEN_WLANSSID];
	uint8_t encryptionType;
	int32_t RSSI;
	int32_t channel;
#if defined(ESP8266)
	bool isHidden;
	uint8_t unused[3];
#endif
};

struct struct_wifiInfo *wifiInfo;
uint8_t count_wifiInfo;

template<typename T, std::size_t N> constexpr std::size_t array_num_elements(const T(&)[N]) {
	return N;
}

#define msSince(timestamp_before) (act_milli - (timestamp_before))

const char data_first_part[] PROGMEM = "{\"software_version\": \"" SOFTWARE_VERSION_STR "\", \"sensordatavalues\":[";
const char JSON_SENSOR_DATA_VALUES[] PROGMEM = "sensordatavalues";

/*****************************************************************
 * Debug output                                                  *
 *****************************************************************/

#define debug_level_check(level) { if (level > cfg::debug) return; }

static void debug_out(const String& text, unsigned int level) {
	debug_level_check(level); Serial.print(text);
}

static void debug_out(const __FlashStringHelper* text, unsigned int level) {
	debug_level_check(level); Serial.print(text);
}

static void debug_outln(const String& text, unsigned int level) {
	debug_level_check(level); Serial.println(text);
}

static void debug_outln_info(const String& text) {
	debug_level_check(DEBUG_MIN_INFO); Serial.println(text);
}

static void debug_outln_verbose(const String& text) {
	debug_level_check(DEBUG_MED_INFO); Serial.println(text);
}

static void debug_outln_error(const __FlashStringHelper* text) {
	debug_level_check(DEBUG_ERROR); Serial.println(text);
}

static void debug_outln_info(const __FlashStringHelper* text) {
	debug_level_check(DEBUG_MIN_INFO); Serial.println(text);
}

static void debug_outln_verbose(const __FlashStringHelper* text) {
	debug_level_check(DEBUG_MED_INFO); Serial.println(text);
}

static void debug_outln_info(const __FlashStringHelper* text, const String& option) {
	debug_level_check(DEBUG_MIN_INFO);
	Serial.print(text);
	Serial.println(option);
}

static void debug_outln_info(const __FlashStringHelper* text, float value) {
	debug_outln_info(text, String(value));
}

static void debug_outln_verbose(const __FlashStringHelper* text, const String& option) {
	debug_level_check(DEBUG_MED_INFO);
	Serial.print(text);
	Serial.println(option);
}

static void debug_outln_info_bool(const __FlashStringHelper* text, const bool option) {
	debug_level_check(DEBUG_MIN_INFO);
	Serial.print(text);
	Serial.println(String(option));
}

#undef debug_level_check

/*****************************************************************
 * check display values, return '-' if undefined                 *
 *****************************************************************/
static String check_display_value(double value, double undef, uint8_t len, uint8_t str_len) {
	RESERVE_STRING(s, 15);
	s = (value != undef ? String(value, len) : String("-"));
	while (s.length() < str_len) {
		s = " " + s;
	}
	return s;
}

/*****************************************************************
 * add value to json string                                  *
 *****************************************************************/
static void add_Value2Json(String& res, const __FlashStringHelper* type, const String& value) {
	RESERVE_STRING(s, SMALL_STR);

	s = F("{\"value_type\":\"{t}\",\"value\":\"{v}\"},");
	s.replace("{t}", String(type));
	s.replace("{v}", value);
	res += s;
}

static void add_Value2Json(String& res, const __FlashStringHelper* type, const __FlashStringHelper* debug_type, const float& value) {
	debug_outln_info(FPSTR(debug_type), value);
	add_Value2Json(res, type, String(value));
}

/*****************************************************************
 * send Plantower PMS sensor command start, stop, cont. mode     *
 *****************************************************************/
static bool PMS_cmd(PmSensorCmd cmd) {
	static constexpr uint8_t start_cmd[] PROGMEM = {
		0x42, 0x4D, 0xE4, 0x00, 0x01, 0x01, 0x74
	};
	static constexpr uint8_t stop_cmd[] PROGMEM = {
		0x42, 0x4D, 0xE4, 0x00, 0x00, 0x01, 0x73
	};
	static constexpr uint8_t continuous_mode_cmd[] PROGMEM = {
		0x42, 0x4D, 0xE1, 0x00, 0x01, 0x01, 0x71
	};
	constexpr uint8_t cmd_len = array_num_elements(start_cmd);

	uint8_t buf[cmd_len];
	switch (cmd) {
	case PmSensorCmd::Start:
		memcpy_P(buf, start_cmd, cmd_len);
		break;
	case PmSensorCmd::Stop:
		memcpy_P(buf, stop_cmd, cmd_len);
		break;
	case PmSensorCmd::ContinuousMode:
		memcpy_P(buf, continuous_mode_cmd, cmd_len);
		break;
	}
	serialSDS.write(buf, cmd_len);
	return cmd != PmSensorCmd::Stop;
}

/*****************************************************************
 * read config from spiffs                                       *
 *****************************************************************/

/* backward compatibility for the times when we stored booleans as strings */
static bool boolFromJSON(const DynamicJsonDocument& json, const __FlashStringHelper* key)
{
	if (json[key].is<char*>()) {
		return !strcmp_P(json[key].as<char*>(), PSTR("true"));
	}
	return json[key].as<bool>();
}

static void readConfig(bool oldconfig = false) {
	bool rewriteConfig = false;

	String cfgName(F("/config.json"));
	if (oldconfig) {
		cfgName += F(".old");
	}

	File configFile = SPIFFS.open(cfgName, "r");
	if (!configFile) {
		if (!oldconfig) {
			return readConfig(true /* oldconfig */);
		}

		debug_outln_error(F("failed to open config file."));
		return;
	}

	debug_outln_info(F("opened config file..."));
	DynamicJsonDocument json(JSON_BUFFER_SIZE);
	DeserializationError err = deserializeJson(json, configFile);
	configFile.close();

	if (!err) {
		debug_outln_info(F("parsed json..."));
		for (unsigned e = 0; e < sizeof(configShape)/sizeof(configShape[0]); ++e) {
			ConfigShapeEntry c;
			memcpy_P(&c, &configShape[e], sizeof(ConfigShapeEntry));
			if (json[c.cfg_key].isNull()) {
				continue;
			}
			switch (c.cfg_type) {
				case Config_Type_Bool:
					*(c.cfg_val.as_bool) = boolFromJSON(json, c.cfg_key);
					break;
				case Config_Type_UInt:
				case Config_Type_Time:
					*(c.cfg_val.as_uint) = json[c.cfg_key].as<unsigned int>();
					break;
				case Config_Type_String:
				case Config_Type_Password:
					strncpy(c.cfg_val.as_str, json[c.cfg_key].as<char*>(), c.cfg_len);
					c.cfg_val.as_str[c.cfg_len] = '\0';
					break;
			};
		}
		String writtenVersion(json["SOFTWARE_VERSION"].as<char*>());
		if (writtenVersion.length() && writtenVersion[0] == 'N' && SOFTWARE_VERSION != writtenVersion) {
			debug_outln_info(F("Rewriting old config from: "), writtenVersion);
			// would like to do that, but this would wipe firmware.old which the two stage loader
			// might still need
			// SPIFFS.format();
			rewriteConfig = true;
		}
		if (strcmp_P(cfg::senseboxid, PSTR("00112233445566778899aabb")) == 0) {
			cfg::senseboxid[0] = '\0';
			cfg::send2sensemap = false;
			rewriteConfig = true;
		}
		if (strlen(cfg::measurement_name_influx) == 0) {
			strcpy_P(cfg::measurement_name_influx, MEASUREMENT_NAME_INFLUX);
			rewriteConfig = true;
		}
		if (strcmp_P(cfg::host_influx, PSTR("api.luftdaten.info")) == 0) {
			cfg::host_influx[0] = '\0';
			cfg::send2influx = false;
			rewriteConfig = true;
		}
		if (boolFromJSON(json, F("pm24_read")) || boolFromJSON(json, F("pms32_read"))) {
			cfg::pms_read = true;
			rewriteConfig = true;
		}
	} else {
		debug_outln_error(F("failed to load json config"));

		if (!oldconfig) {
			return readConfig(true /* oldconfig */);
		}
	}

	if (rewriteConfig) {
		writeConfig();
	}
}

static void init_config() {

	debug_outln_info(F("mounting FS..."));
#if defined(ESP32)
	bool spiffs_begin_ok = SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED);
#else
	bool spiffs_begin_ok = SPIFFS.begin();
#endif

	if (!spiffs_begin_ok) {
		debug_outln_error(F("failed to mount FS"));
		return;
	}
	readConfig();
}

/*****************************************************************
 * write config to spiffs                                        *
 *****************************************************************/
static void writeConfig() {
	DynamicJsonDocument json(JSON_BUFFER_SIZE);
	debug_outln_info(F("Saving config..."));
	json["SOFTWARE_VERSION"] = SOFTWARE_VERSION;

	for (unsigned e = 0; e < sizeof(configShape)/sizeof(configShape[0]); ++e) {
		ConfigShapeEntry c;
		memcpy_P(&c, &configShape[e], sizeof(ConfigShapeEntry));
		switch (c.cfg_type) {
		case Config_Type_Bool:
			json[c.cfg_key].set(*c.cfg_val.as_bool);
			break;
		case Config_Type_UInt:
		case Config_Type_Time:
			json[c.cfg_key].set(*c.cfg_val.as_uint);
			break;
		case Config_Type_Password:
		case Config_Type_String:
			json[c.cfg_key].set(c.cfg_val.as_str);
			break;
		};
	}

	SPIFFS.remove(F("/config.json.old"));
	SPIFFS.rename(F("/config.json"), F("/config.json.old"));

	File configFile = SPIFFS.open(F("/config.json"), "w");
	if (configFile) {
		serializeJson(json, configFile);
		configFile.close();
		debug_outln_info(F("Config written successfully."));
	} else {
		debug_outln_error(F("failed to open config file for writing"));
	}
}

/*****************************************************************
 * Prepare information for data Loggers                          *
 *****************************************************************/
static void createLoggerConfigs() {
#if defined(ESP8266)
	auto new_session = []() { return new BearSSL::Session; };
#else
	auto new_session = []() { return nullptr; };
#endif
  	if (cfg::send2cfa) {
    	loggerConfigs[LoggerCFA].destport = 80;
    	if (cfg::ssl_cfa) {
      		loggerConfigs[LoggerCFA].destport = 80;
      		loggerConfigs[LoggerCFA].session = new_session();
    	}
  	}
	if (cfg::send2dusti) {
		loggerConfigs[LoggerSensorCommunity].destport = 80;
		if (cfg::ssl_dusti) {
			loggerConfigs[LoggerSensorCommunity].destport = 443;
			loggerConfigs[LoggerSensorCommunity].session = new_session();
		}
	}
	loggerConfigs[LoggerMadavi].destport = PORT_MADAVI;
	if (cfg::send2madavi && cfg::ssl_madavi) {
		loggerConfigs[LoggerMadavi].destport = 443;
		loggerConfigs[LoggerMadavi].session = new_session();
	}
	loggerConfigs[LoggerSensemap].destport = PORT_SENSEMAP;
	loggerConfigs[LoggerSensemap].session = new_session();
	loggerConfigs[LoggerFSapp].destport = PORT_FSAPP;
	loggerConfigs[LoggerFSapp].session = new_session();
	loggerConfigs[Loggeraircms].destport = PORT_AIRCMS;
	loggerConfigs[LoggerInflux].destport = cfg::port_influx;
	if (cfg::send2influx && cfg::ssl_influx) {
		loggerConfigs[LoggerInflux].session = new_session();
	}
	loggerConfigs[LoggerCustom].destport = cfg::port_custom;
	if (cfg::send2custom && (cfg::ssl_custom || (cfg::port_custom == 443))) {
		loggerConfigs[LoggerCustom].session = new_session();
	}
}

/*****************************************************************
 * aircms.online helper functions                                *
 *****************************************************************/
static String sha1Hex(const String& s) {
	char sha1sum_output[20];

#if defined(ESP8266)
	br_sha1_context sc;

	br_sha1_init(&sc);
	br_sha1_update(&sc, s.c_str(), s.length());
	br_sha1_out(&sc, sha1sum_output);
#endif
#if defined(ESP32)
	esp_sha(SHA1, (const unsigned char*) s.c_str(), s.length(), (unsigned char*)sha1sum_output);
#endif
	String r;
	for (uint16_t i = 0; i < 20; i++) {
		char hex[3];
		snprintf(hex, sizeof(hex), "%02x", sha1sum_output[i]);
		r += hex;
	}
	return r;
}

static String hmac1(const String& secret, const String& s) {
	String str = sha1Hex(s);
	str = secret + str;
	return sha1Hex(str);
}


/*****************************************************************
 * html helper functions                                         *
 *****************************************************************/

static void start_html_page(String& page_content, const String& title) {
	last_page_load = millis();

	RESERVE_STRING(s, LARGE_STR);
	s = FPSTR(WEB_PAGE_HEADER);
	s.replace("{t}", title);
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), s);

	server.sendContent_P(WEB_PAGE_HEADER_HEAD);

	s = FPSTR(WEB_PAGE_HEADER_BODY);
	s.replace("{t}", title);
	if (title != " ") {
		s.replace("{n}", F("&raquo;"));
	} else {
		s.replace("{n}", emptyString);
	}
	s.replace("{id}", esp_chipid);
	s.replace("{mac}", WiFi.macAddress());
	page_content += s;
}

static void end_html_page(String& page_content) {
	if (page_content.length()) {
		server.sendContent(page_content);
	}
	server.sendContent_P(WEB_PAGE_FOOTER);
}

static void add_form_input(String& page_content, const ConfigShapeId cfgid, const __FlashStringHelper* info, const int length) {
	RESERVE_STRING(s, MED_STR);
	s = F("<tr>"
			"<td title='[&lt;= {l}]'>{i}:&nbsp;</td>"
			"<td style='width:{l}em'>"
			"<input type='{t}' name='{n}' id='{n}' placeholder='{i}' value='{v}' maxlength='{l}'/>"
			"</td></tr>");
	String t_value;
	ConfigShapeEntry c;
	memcpy_P(&c, &configShape[cfgid], sizeof(ConfigShapeEntry));
	switch (c.cfg_type) {
	case Config_Type_UInt:
		t_value = String(*c.cfg_val.as_uint);
		s.replace("{t}", F("number"));
		break;
	case Config_Type_Time:
		t_value = String((*c.cfg_val.as_uint) / 1000);
		s.replace("{t}", F("number"));
		break;
	default:
		t_value = c.cfg_val.as_str;
		t_value.replace("'", "&#39;");
		if (c.cfg_type == Config_Type_Password) {
			s.replace("{t}", F("password"));
		} else {
			s.replace("{t}", F("text"));
		}
	}
	s.replace("{i}", info);
	s.replace("{n}", String(c.cfg_key));
	s.replace("{v}", t_value);
	s.replace("{l}", String(length));
	page_content += s;
}

static String form_checkbox(const ConfigShapeId cfgid, const String& info, const bool linebreak) {
	RESERVE_STRING(s, MED_STR);
	s = F("<label for='{n}'>"
	"<input type='checkbox' name='{n}' value='1' id='{n}' {c}/>"
	"<input type='hidden' name='{n}' value='0'/>"
	"{i}</label><br/>");
	if (*configShape[cfgid].cfg_val.as_bool) {
		s.replace("{c}", F(" checked='checked'"));
	} else {
		s.replace("{c}", emptyString);
	};
	s.replace("{i}", info);
	s.replace("{n}", String(configShape[cfgid].cfg_key));
	if (! linebreak) {
		s.replace("<br/>", emptyString);
	}
	return s;
}

static String form_submit(const String& value) {
	String s = F(	"<tr>"
					"<td>&nbsp;</td>"
					"<td>"
					"<input type='submit' name='submit' value='{v}' />"
					"</td>"
					"</tr>");
	s.replace("{v}", value);
	return s;
}

static String form_select_lang() {
	String s_select = F(" selected='selected'");
	String s = F(	"<tr>"
					"<td>" INTL_LANGUAGE ":&nbsp;</td>"
					"<td>"
					"<select id='current_lang' name='current_lang'>"
					"<option value='BG'>Bulgarian (BG)</option>"
					"<option value='CZ'>Český (CZ)</option>"
					"<option value='DE'>Deutsch (DE)</option>"
					"<option value='DK'>Dansk (DK)</option>"
					"<option value='EN'>English (EN)</option>"
					"<option value='ES'>Español (ES)</option>"
					"<option value='FR'>Français (FR)</option>"
					"<option value='IT'>Italiano (IT)</option>"
					"<option value='LU'>Lëtzebuergesch (LU)</option>"
					"<option value='NL'>Nederlands (NL)</option>"
					"<option value='PL'>Polski (PL)</option>"
					"<option value='PT'>Português (PT)</option>"
					"<option value='RS'>Srpski (RS)</option>"
					"<option value='RU'>Русский (RU)</option>"
					"<option value='SE'>Svenska (SE)</option>"
					"<option value='TR'>Türkçe (TR)</option>"
					"<option value='UA'>український (UA)</option>"
					"</select>"
					"</td>"
					"</tr>");

	s.replace("'" + String(cfg::current_lang) + "'>", "'" + String(cfg::current_lang) + "'" + s_select + ">");
	return s;
}

static String tmpl(const __FlashStringHelper* patt, const String& value) {
	String s = patt;
	s.replace("{v}", value);
	return s;
}

static void add_line_value(String& s, const __FlashStringHelper* name, const String& value) {
	s += F("<br/>");
	s += name;
	s += ": ";
	s += value;
}

static void add_line_value_bool(String& s, const __FlashStringHelper* name, const bool value) {
	add_line_value(s, name, String(value));
}

static void add_line_value_bool(String&s, const __FlashStringHelper* patt, const __FlashStringHelper* name, const bool value) {
	s += F("<br/>");
	s += tmpl(patt, name);
	s += ": ";
	s += String(value);
}

static void add_table_row_from_value(String& page_content, const __FlashStringHelper* sensor, const __FlashStringHelper* param, const String& value, const String& unit) {
	RESERVE_STRING(s, MED_STR);
	s = F("<tr><td>{s}</td><td>{p}</td><td class='r'>{v}&nbsp;{u}</td></tr>");
	s.replace("{s}", sensor);
	s.replace("{p}", param);
	s.replace("{v}", value);
	s.replace("{u}", unit);
	page_content += s;
}

static void add_table_row_from_value(String& page_content, const __FlashStringHelper* param, const String& value, const char* unit = nullptr) {
	RESERVE_STRING(s, MED_STR);
	s = F("<tr><td>{p}</td><td class='r'>{v}&nbsp;{u}</td></tr>");
	s.replace("{p}", param);
	s.replace("{v}", value);
	s.replace("{u}", String(unit));
	page_content += s;
}

static int32_t calcWiFiSignalQuality(int32_t rssi) {
	// Treat 0 or positive values as 0%
	if (rssi >= 0 || rssi < -100) {
		rssi = -100;
	}
	if (rssi > -50) {
		rssi = -50;
	}
	return (rssi + 100) * 2;
}

static String wlan_ssid_to_table_row(const String& ssid, const String& encryption, int32_t rssi) {
	String s = F(	"<tr>"
					"<td>"
					"<a href='#wlanpwd' onclick='setSSID(this)' class='wifi'>{n}</a>&nbsp;{e}"
					"</td>"
					"<td style='width:80%;vertical-align:middle;'>"
					"{v}%"
					"</td>"
					"</tr>");
	s.replace("{n}", ssid);
	s.replace("{e}", encryption);
	s.replace("{v}", String(calcWiFiSignalQuality(rssi)));
	return s;
}

static void add_warning_first_cycle(String& page_content) {
	String s = FPSTR(INTL_TIME_TO_FIRST_MEASUREMENT);
	unsigned int time_to_first = cfg::sending_intervall_ms - msSince(starttime);
	if (time_to_first > cfg::sending_intervall_ms) {
		time_to_first = 0;
	}
	s.replace("{v}", String(((time_to_first + 500) / 1000)));
	page_content += s;
}

static void add_age_last_values(String& s) {
	s += "<b>";
	unsigned int time_since_last = msSince(starttime);
	if (time_since_last > cfg::sending_intervall_ms) {
		time_since_last = 0;
	}
	s += String((time_since_last + 500) / 1000);
	s += FPSTR(INTL_TIME_SINCE_LAST_MEASUREMENT);
	s += FPSTR(WEB_B_BR_BR);
}

static String add_sensor_type(const String& sensor_text) {
	RESERVE_STRING(s, SMALL_STR);
	s = sensor_text;
	s.replace("{pm}", FPSTR(INTL_PARTICULATE_MATTER));
	s.replace("{t}", FPSTR(INTL_TEMPERATURE));
	s.replace("{h}", FPSTR(INTL_HUMIDITY));
	s.replace("{p}", FPSTR(INTL_PRESSURE));
	s.replace("{l_a}", FPSTR(INTL_LEQ_A));
	return s;
}

/*****************************************************************
 * Webserver request auth: prompt for BasicAuth
 *
 * -Provide BasicAuth for all page contexts except /values and images
 *****************************************************************/
static bool webserver_request_auth() {
	if (cfg::www_basicauth_enabled && ! wificonfig_loop) {
		debug_outln_info(F("validate request auth..."));
		if (!server.authenticate(cfg::www_username, cfg::www_password)) {
			server.requestAuthentication(BASIC_AUTH, "Sensor Login", F("Authentication failed"));
			return false;
		}
	}
	return true;
}

static void sendHttpRedirect() {
	server.sendHeader(F("Location"), F("http://192.168.4.1/config"));
	server.send(302, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), emptyString);
}

/*****************************************************************
 * Webserver root: show all options                              *
 *****************************************************************/
static void webserver_root() {
	if (WiFi.status() != WL_CONNECTED) {
		sendHttpRedirect();
	} else {
		if (!webserver_request_auth())
		{ return; }

		RESERVE_STRING(page_content, XLARGE_STR);
		start_html_page(page_content, emptyString);
		debug_outln_info(F("ws: root ..."));

		// Enable Pagination
		page_content += FPSTR(WEB_ROOT_PAGE_CONTENT);
		page_content.replace(F("{t}"), FPSTR(INTL_CURRENT_DATA));
		page_content.replace(F("{s}"), FPSTR(INTL_DEVICE_STATUS));
		page_content.replace(F("{conf}"), FPSTR(INTL_CONFIGURATION));
		page_content.replace(F("{restart}"), FPSTR(INTL_RESTART_SENSOR));
		page_content.replace(F("{debug_setting}"), FPSTR(INTL_DEBUG_SETTING_TO));
		end_html_page(page_content);
	}
}

/*****************************************************************
 * Webserver config: show config page                            *
 *****************************************************************/

static void webserver_config_send_body_get(String& page_content) {
	using namespace cfg;

	auto add_form_checkbox = [&page_content](const ConfigShapeId cfgid, const String& info) {
		page_content += form_checkbox(cfgid, info, true);
	};

	auto add_form_checkbox_sensor = [&add_form_checkbox](const ConfigShapeId cfgid, __const __FlashStringHelper* info) {
		add_form_checkbox(cfgid, add_sensor_type(info));
	};

	debug_outln_info(F("begin webserver_config_body_get ..."));
	page_content += F("<form method='POST' action='/config' style='width:100%;'>\n<b>" INTL_WIFI_SETTINGS "</b><br/>");
	debug_outln_info(F("ws: config page 1"));
	if (wificonfig_loop) {  // scan for wlan ssids
		page_content += F("<div id='wifilist'>" INTL_WIFI_NETWORKS "</div><br/>");
	}
	page_content += FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_wlanssid, FPSTR(INTL_FS_WIFI_NAME), LEN_WLANSSID-1);
	add_form_input(page_content, Config_wlanpwd, FPSTR(INTL_PASSWORD), LEN_CFG_PASSWORD-1);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	page_content += F("<hr/>\n<br/><b>");

	page_content += FPSTR(INTL_AB_HIER_NUR_ANDERN);
	page_content += FPSTR(WEB_B_BR);
	
	// Paginate page after ~ 1500 Bytes
	server.sendContent(page_content);
	page_content = emptyString;

	add_form_checkbox(Config_www_basicauth_enabled, FPSTR(INTL_BASICAUTH));
	page_content += FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_www_username, FPSTR(INTL_USER), LEN_WWW_USERNAME-1);
	add_form_input(page_content, Config_www_password, FPSTR(INTL_PASSWORD), LEN_CFG_PASSWORD-1);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	page_content += FPSTR(BR_TAG);

	// Paginate page after ~ 1500 Bytes
	server.sendContent(page_content);

	if (! wificonfig_loop) {
		page_content = FPSTR(INTL_FS_WIFI_DESCRIPTION);
		page_content += FPSTR(BR_TAG);

		page_content += FPSTR(TABLE_TAG_OPEN);
		add_form_input(page_content, Config_fs_ssid, FPSTR(INTL_FS_WIFI_NAME), LEN_FS_SSID-1);
		add_form_input(page_content, Config_fs_pwd, FPSTR(INTL_PASSWORD), LEN_CFG_PASSWORD-1);
		page_content += FPSTR(TABLE_TAG_CLOSE_BR);
		page_content += F("<hr/>\n<br/><b>");

		// Paginate page after ~ 1500 Bytes
		server.sendContent(page_content);
	}
	page_content = FPSTR(WEB_BR_LF_B);
	page_content += F(INTL_FIRMWARE "</b>&nbsp;");
	add_form_checkbox(Config_auto_update, FPSTR(INTL_AUTO_UPDATE));
	add_form_checkbox(Config_use_beta, FPSTR(INTL_USE_BETA));

	page_content += FPSTR(TABLE_TAG_OPEN);
	page_content += form_select_lang();
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);

	page_content += F("<script>"
	    "var $ = function(e) { return document.getElementById(e); };"
	    "function updateOTAOptions() { "
		"$('current_lang').disabled = $('use_beta').disabled = !$('auto_update').checked; "
		"}; updateOTAOptions(); $('auto_update').onchange = updateOTAOptions;"
		"</script>");

	page_content += FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_debug, FPSTR(INTL_DEBUG_LEVEL), 1);
	add_form_input(page_content, Config_sending_intervall_ms, FPSTR(INTL_MEASUREMENT_INTERVAL), 5);
	add_form_input(page_content, Config_time_for_wifi_config, FPSTR(INTL_DURATION_ROUTER_MODE), 5);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	page_content += F("<hr/>\n");

	server.sendContent(page_content);
	page_content = FPSTR(WEB_BR_LF_B);
	page_content += FPSTR(INTL_MORE_SETTINGS);
	page_content += FPSTR(WEB_B_BR);

	add_form_checkbox(Config_wifi_enabled, FPSTR(INTL_ENABLE_WIFI));

	// Paginate page after ~ 1500 Bytes
	server.sendContent(page_content);
	page_content = emptyString;

	add_form_checkbox(Config_display_wifi_info, FPSTR(INTL_DISPLAY_WIFI_INFO));
	add_form_checkbox(Config_display_device_info, FPSTR(INTL_DISPLAY_DEVICE_INFO));

	page_content += F("<hr/>\n");

	server.sendContent(page_content);
	page_content = FPSTR(WEB_BR_LF_B);
	page_content += FPSTR(INTL_SENSORS);
	page_content += FPSTR(WEB_B_BR);

	// Paginate page after ~ 1500 Bytes
	server.sendContent(page_content);
	page_content = emptyString;

	add_form_checkbox_sensor(Config_dht_read, FPSTR(INTL_DHT22));
	add_form_checkbox_sensor(Config_pms_read, FPSTR(INTL_PMS));

	page_content += F("<hr/>\n");

	// Paginate page after ~ 1500 Bytes
	server.sendContent(page_content);
	page_content = emptyString;

	page_content += FPSTR(WEB_BR_LF_B);
	page_content += F("APIs");
	page_content += FPSTR(WEB_B_BR);
	// Paginate page after ~ 1500 Bytes

	server.sendContent(page_content);
	page_content = FPSTR(TABLE_TAG_CLOSE_BR);
	page_content += FPSTR(BR_TAG);
	page_content += form_checkbox(Config_send2custom, FPSTR(INTL_SEND_TO_OWN_API), false);
	page_content += FPSTR(WEB_NBSP_NBSP_BRACE);
	page_content += form_checkbox(Config_ssl_custom, FPSTR(WEB_HTTPS), false);
	page_content += FPSTR(WEB_BRACE_BR);

	server.sendContent(page_content);
	page_content = FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_host_custom, FPSTR(INTL_SERVER), LEN_HOST_CUSTOM-1);
	add_form_input(page_content, Config_url_custom, FPSTR(INTL_PATH), LEN_URL_CUSTOM-1);
	add_form_input(page_content, Config_port_custom, FPSTR(INTL_PORT), MAX_PORT_DIGITS);
	add_form_input(page_content, Config_user_custom, FPSTR(INTL_USER), LEN_USER_CUSTOM-1);
	add_form_input(page_content, Config_pwd_custom, FPSTR(INTL_PASSWORD), LEN_CFG_PASSWORD-1);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);

	page_content += FPSTR(BR_TAG);
	page_content += F("<hr/>");
	server.sendContent(page_content);

	page_content = FPSTR(TABLE_TAG_CLOSE_BR);

	page_content += form_submit(FPSTR(INTL_SAVE_AND_RESTART));
	page_content += FPSTR(BR_TAG);
	page_content += FPSTR(WEB_BR_FORM);
	if (wificonfig_loop) {  // scan for wlan ssids
		page_content += F("<script>window.setTimeout(load_wifi_list,1000);</script>");
	}

	server.sendContent(page_content);
	page_content = emptyString;
}

static void webserver_config_send_body_post(String& page_content) {
	String masked_pwd;

	using namespace cfg;

	for (unsigned e = 0; e < sizeof(configShape)/sizeof(configShape[0]); ++e) {
		ConfigShapeEntry c;
		memcpy_P(&c, &configShape[e], sizeof(ConfigShapeEntry));
		const String s_param(c.cfg_key);
		if (!server.hasArg(s_param)) {
			continue;
		}

		switch (c.cfg_type) {
		case Config_Type_UInt:
			*(c.cfg_val.as_uint) = server.arg(s_param).toInt();
			break;
		case Config_Type_Time:
			*(c.cfg_val.as_uint) = server.arg(s_param).toInt() * 1000;
			break;
		case Config_Type_Bool:
			*(c.cfg_val.as_bool) = (server.arg(s_param) == "1");
			break;
		case Config_Type_String:
			strncpy(c.cfg_val.as_str, server.arg(s_param).c_str(), c.cfg_len);
			c.cfg_val.as_str[c.cfg_len] = '\0';
			break;
		case Config_Type_Password:
			const String server_arg(server.arg(s_param));
			masked_pwd = emptyString;
			for (uint8_t i=0;i<server_arg.length();i++)
				masked_pwd += '*';
			if (masked_pwd != server_arg || !server_arg.length()) {
				server_arg.toCharArray(c.cfg_val.as_str, LEN_CFG_PASSWORD);
			}
			break;
		}
	}

	add_line_value_bool(page_content, FPSTR(INTL_SEND_TO), F("Sensor.Community"), send2dusti);
	add_line_value_bool(page_content, FPSTR(INTL_SEND_TO), F("Madavi"), send2madavi);
	add_line_value_bool(page_content, FPSTR(INTL_READ_FROM), FPSTR(SENSORS_DHT22), dht_read);
	add_line_value_bool(page_content, FPSTR(INTL_READ_FROM), FPSTR(SENSORS_PMSx003), pms_read);
	

	// Paginate after ~ 1500 bytes
	server.sendContent(page_content);
	page_content = emptyString;

	add_line_value_bool(page_content, FPSTR(INTL_ENABLE_WIFI), wifi_enabled);
	add_line_value_bool(page_content, FPSTR(INTL_DISPLAY_WIFI_INFO), display_wifi_info);
	add_line_value_bool(page_content, FPSTR(INTL_DISPLAY_DEVICE_INFO), display_device_info);
	add_line_value_bool(page_content, FPSTR(INTL_AUTO_UPDATE), auto_update);
	add_line_value_bool(page_content, FPSTR(INTL_USE_BETA), use_beta);
	add_line_value(page_content, FPSTR(INTL_DEBUG_LEVEL), String(debug));
	add_line_value(page_content, FPSTR(INTL_MEASUREMENT_INTERVAL), String(sending_intervall_ms));
	add_line_value_bool(page_content, FPSTR(INTL_SEND_TO), F("Feinstaub-App"), send2fsapp);
	add_line_value_bool(page_content, FPSTR(INTL_SEND_TO), F("aircms.online"), send2aircms);
	add_line_value_bool(page_content, FPSTR(INTL_SEND_TO), FPSTR(WEB_CSV), send2csv);
	add_line_value_bool(page_content, FPSTR(INTL_SEND_TO), FPSTR(WEB_FEINSTAUB_APP), send2fsapp);
	add_line_value_bool(page_content, FPSTR(INTL_SEND_TO), F("opensensemap"), send2sensemap);

	// Paginate after ~ 1500 bytes
	server.sendContent(page_content);
	page_content = emptyString;

	page_content += F("<br/>senseBox-ID ");
	page_content += senseboxid;
	page_content += FPSTR(WEB_BR_BR);
	add_line_value_bool(page_content, FPSTR(INTL_SEND_TO_OWN_API), send2custom);
	add_line_value(page_content, FPSTR(INTL_SERVER), host_custom);
	add_line_value(page_content, FPSTR(INTL_PATH), url_custom);
	add_line_value(page_content, FPSTR(INTL_PORT), String(port_custom));
	add_line_value(page_content, FPSTR(INTL_USER), user_custom);
	add_line_value(page_content, FPSTR(INTL_PASSWORD), pwd_custom);
	page_content += F("<br/><br/>InfluxDB: ");
	page_content += String(send2influx);
	add_line_value(page_content, FPSTR(INTL_SERVER), host_influx);
	add_line_value(page_content, FPSTR(INTL_PATH), url_influx);
	add_line_value(page_content, FPSTR(INTL_PORT), String(port_influx));
	add_line_value(page_content, FPSTR(INTL_USER), user_influx);
	add_line_value(page_content, FPSTR(INTL_PASSWORD), pwd_influx);
	add_line_value(page_content, F("Measurement"), measurement_name_influx);
	add_line_value(page_content, F("SSL"), String(ssl_influx));
	page_content += FPSTR(WEB_BR_BR);
	page_content += FPSTR(INTL_SENSOR_IS_REBOOTING);

	server.sendContent(page_content);
	page_content = emptyString;
}

static void webserver_config() {
	if (!webserver_request_auth())
	{ return; }

	debug_outln_info(F("ws: config page ..."));

	server.sendHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
	server.sendHeader(F("Pragma"), F("no-cache"));
	server.sendHeader(F("Expires"), F("0"));
	// Enable Pagination (Chunked Transfer)
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);

	RESERVE_STRING(page_content, XLARGE_STR);

	start_html_page(page_content, FPSTR(INTL_CONFIGURATION));
	if (wificonfig_loop) {  // scan for wlan ssids
		page_content += FPSTR(WEB_CONFIG_SCRIPT);
	}

	if (server.method() == HTTP_GET) {
		webserver_config_send_body_get(page_content);
	} else {
		webserver_config_send_body_post(page_content);
	}
	end_html_page(page_content);

	if (server.method() == HTTP_POST) {
		writeConfig();
		sensor_restart();
	}
}

static void sensor_restart() {
#if defined(ESP8266)
		WiFi.disconnect();
		WiFi.mode(WIFI_OFF);
		delay(100);
#endif
		SPIFFS.end();
		serialSDS.end();
		debug_outln_info(F("Restart."));
		delay(500);
		ESP.restart();
		// should not be reached
		while(true) { yield(); }
}

/*****************************************************************
 * Webserver wifi: show available wifi networks                  *
 *****************************************************************/
static void webserver_wifi() {
	String page_content;

	debug_outln_info(F("wifi networks found: "), String(count_wifiInfo));
	if (count_wifiInfo == 0) {
		page_content += FPSTR(BR_TAG);
		page_content += FPSTR(INTL_NO_NETWORKS);
		page_content += FPSTR(BR_TAG);
	} else {
		std::unique_ptr<int[]> indices(new int[count_wifiInfo]);
		debug_outln_info(F("ws: wifi ..."));
		for (unsigned i = 0; i < count_wifiInfo; ++i) {
			indices[i] = i;
		}
		for (unsigned i = 0; i < count_wifiInfo; i++) {
			for (unsigned j = i + 1; j < count_wifiInfo; j++) {
				if (wifiInfo[indices[j]].RSSI > wifiInfo[indices[i]].RSSI) {
					std::swap(indices[i], indices[j]);
				}
			}
		}
		int duplicateSsids = 0;
		for (int i = 0; i < count_wifiInfo; i++) {
			if (indices[i] == -1) {
				continue;
			}
			for (int j = i + 1; j < count_wifiInfo; j++) {
				if (strncmp(wifiInfo[indices[i]].ssid, wifiInfo[indices[j]].ssid, sizeof(wifiInfo[0].ssid)) == 0) {
					indices[j] = -1; // set dup aps to index -1
					++duplicateSsids;
				}
			}
		}

		page_content += FPSTR(INTL_NETWORKS_FOUND);
		page_content += String(count_wifiInfo - duplicateSsids);
		page_content += FPSTR(BR_TAG);
		page_content += FPSTR(BR_TAG);
		page_content += FPSTR(TABLE_TAG_OPEN);
		//if (n > 30) n=30;
		for (int i = 0; i < count_wifiInfo; ++i) {
			if (indices[i] == -1
 #if defined (ESP8266)
				|| wifiInfo[indices[i]].isHidden
 #endif
			) {
				continue;
			}
			// Print SSID and RSSI for each network found
#if defined(ESP8266)
			page_content += wlan_ssid_to_table_row(wifiInfo[indices[i]].ssid, ((wifiInfo[indices[i]].encryptionType == ENC_TYPE_NONE) ? " " : u8"🔒"), wifiInfo[indices[i]].RSSI);
#endif
#if defined(ESP32)
			page_content += wlan_ssid_to_table_row(wifiInfo[indices[i]].ssid, ((wifiInfo[indices[i]].encryptionType == WIFI_AUTH_OPEN) ? " " : u8"🔒"), wifiInfo[indices[i]].RSSI);
#endif
		}
		page_content += FPSTR(TABLE_TAG_CLOSE_BR);
		page_content += FPSTR(BR_TAG);
	}
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}

/*****************************************************************
 * Webserver root: show latest values                            *
 *****************************************************************/
static void webserver_values() {
	if ((WiFi.status() != WL_CONNECTED) && cfg::wifi_enabled) {	
		sendHttpRedirect();	
	} else {
		RESERVE_STRING(page_content, XLARGE_STR);
		start_html_page(page_content, FPSTR(INTL_CURRENT_DATA));
		const String unit_PM("µg/m³");
		const String unit_Deg("°");
		const String unit_T("°C");
		const String unit_H("%");
		const String unit_P("hPa");
		const String unit_NC(F("#/cm³"));
		const String unit_TS("µm");
		const String unit_LA(F("dB(A)"));

		const int signal_quality = calcWiFiSignalQuality(last_signal_strength);
		debug_outln_info(F("ws: values ..."));
		if (!count_sends) {
			page_content += F("<b style='color:red'>");
			add_warning_first_cycle(page_content);
			page_content += FPSTR(WEB_B_BR_BR);
		} else {
			add_age_last_values(page_content);
		}

		server.sendContent(page_content);
		page_content = F("<table cellspacing='0' border='1' cellpadding='5'>\n"
				  "<tr><th>" INTL_SENSOR "</th><th> " INTL_PARAMETER "</th><th>" INTL_VALUE "</th></tr>");
	
	
		if (cfg::pms_read) {
			page_content += FPSTR(EMPTY_ROW);
			add_table_row_from_value(page_content, FPSTR(SENSORS_PMSx003), FPSTR(WEB_PM1), check_display_value(last_value_PMS_P0, -1, 1, 0), unit_PM);
			add_table_row_from_value(page_content, FPSTR(SENSORS_PMSx003), FPSTR(WEB_PM25), check_display_value(last_value_PMS_P2, -1, 1, 0), unit_PM);
			add_table_row_from_value(page_content, FPSTR(SENSORS_PMSx003), FPSTR(WEB_PM10), check_display_value(last_value_PMS_P1, -1, 1, 0), unit_PM);
		}

		if (cfg::dht_read) {
			page_content += FPSTR(EMPTY_ROW);
			add_table_row_from_value(page_content, FPSTR(SENSORS_DHT22), FPSTR(INTL_TEMPERATURE), check_display_value(last_value_DHT_T, -128, 1, 0), unit_T);
			add_table_row_from_value(page_content, FPSTR(SENSORS_DHT22), FPSTR(INTL_HUMIDITY), check_display_value(last_value_DHT_H, -1, 1, 0), unit_H);
		}

		server.sendContent(page_content);
		page_content = FPSTR(EMPTY_ROW);
		if(cfg::wifi_enabled) {
			add_table_row_from_value(page_content, F("WiFi"), FPSTR(INTL_SIGNAL_STRENGTH), String(last_signal_strength), "dBm");
			add_table_row_from_value(page_content, F("WiFi"), FPSTR(INTL_SIGNAL_QUALITY), String(signal_quality), "%");
		}
		page_content += FPSTR(EMPTY_ROW);
		page_content += FPSTR(TABLE_TAG_CLOSE_BR);
		end_html_page(page_content);
	}
}

static String delayToString(unsigned time_ms) {

	char buf[64];
	String s;

	if (time_ms > 2 * 1000 * 60 * 60 * 24) {
		sprintf_P(buf, PSTR("%d days, "), time_ms / (1000 * 60 * 60 * 24));
		s += buf;
		time_ms %= 1000 * 60 * 60 * 24;
	}

	if (time_ms > 2 * 1000 * 60 * 60) {
		sprintf_P(buf, PSTR("%d hours, "), time_ms / (1000 * 60 * 60));
		s += buf;
		time_ms %= 1000 * 60 * 60;
	}

	if (time_ms > 2 * 1000 * 60) {
		sprintf_P(buf, PSTR("%d min, "), time_ms / (1000 * 60));
		s += buf;
		time_ms %= 1000 * 60;
	}

	if (time_ms > 2 * 1000) {
		sprintf_P(buf, PSTR("%ds, "), time_ms / 1000);
		s += buf;
	}

	if (s.length() > 2) {
		s = s.substring(0, s.length() - 2);
	}

	return s;
}

/*****************************************************************
 * Webserver root: show device status
 *****************************************************************/
static void webserver_status() {
	if ((WiFi.status() != WL_CONNECTED) && cfg::wifi_enabled) {	
		sendHttpRedirect();
		return;	
	}
	RESERVE_STRING(page_content, XLARGE_STR);
	start_html_page(page_content, FPSTR(INTL_DEVICE_STATUS));

	debug_outln_info(F("ws: status ..."));
	server.sendContent(page_content);
	page_content = F("<table cellspacing='0' border='1' cellpadding='5'>\n"
			 "<tr><th> " INTL_PARAMETER "</th><th>" INTL_VALUE "</th></tr>");
	String versionHtml(SOFTWARE_VERSION);
	versionHtml += F("/ST:");
	versionHtml += String(!airrohr_selftest_failed);
	versionHtml += '/';
	versionHtml += ESP.getFullVersion();
	versionHtml.replace("/", FPSTR(BR_TAG));
	add_table_row_from_value(page_content, FPSTR(INTL_FIRMWARE), versionHtml);
	add_table_row_from_value(page_content, F("Free Memory"), String(ESP.getFreeHeap()));
	add_table_row_from_value(page_content, F("Heap Fragmentation"), String(ESP.getHeapFragmentation()), "%");
	if (cfg::auto_update) {
		add_table_row_from_value(page_content, F("Last OTA"), delayToString(millis() - last_update_attempt));
	}
	if(cfg::wifi_enabled){
	#if defined(ESP8266)
		add_table_row_from_value(page_content, F("NTP Sync"), String(sntp_time_set));
		StreamString ntpinfo;

		for (unsigned i = 0; i < SNTP_MAX_SERVERS; i++) {
			const ip_addr_t* sntp = sntp_getserver(i);
			if (sntp && !ip_addr_isany(sntp)) {
				const char* name = sntp_getservername(i);
				if (!ntpinfo.isEmpty()) {
					ntpinfo.print(FPSTR(BR_TAG));
				}
				ntpinfo.printf_P(PSTR("%s (%s)"), IPAddress(*sntp).toString().c_str(), name ? name : "?");
				ntpinfo.printf_P(PSTR(" reachable: %s"), sntp_getreachability(i) ? "Yes" : "No");
			}
		}
		add_table_row_from_value(page_content, F("NTP Info"), ntpinfo);
	#endif
	}
	if(cfg::wifi_enabled){
		time_t now = time(nullptr);
		add_table_row_from_value(page_content, FPSTR(INTL_TIME), ctime(&now));
	}
	add_table_row_from_value(page_content, F("Uptime"), delayToString(millis() - time_point_device_start_ms));
	add_table_row_from_value(page_content, F("Reset Reason"), ESP.getResetReason());

	page_content += FPSTR(EMPTY_ROW);
	page_content += F("<tr><td colspan='2'><b>" INTL_ERROR "</b></td></tr>");
	if(cfg::wifi_enabled){
		add_table_row_from_value(page_content, F("WiFi"), String(WiFi_error_count));
		if (last_update_returncode != 0) {
			add_table_row_from_value(page_content, F("OTA Return"),
				last_update_returncode > 0 ? String(last_update_returncode) : HTTPClient::errorToString(last_update_returncode));
		}
		add_table_row_from_value(page_content, F("Data Send"), String(sendData_error_count));
		if (last_sendData_returncode != 0) {
			add_table_row_from_value(page_content, F("Data Send Return"),
				last_sendData_returncode > 0 ? String(last_sendData_returncode) : HTTPClient::errorToString(last_sendData_returncode));
		}
	}

	page_content += FPSTR(EMPTY_ROW);
	page_content += F("<tr><td colspan='2'><b>" "LOG COUNT" "</b></td></tr>");

	server.sendContent(page_content);
	page_content = emptyString;

	if (count_sends > 0) {
		page_content += FPSTR(EMPTY_ROW);
		add_table_row_from_value(page_content, F(INTL_NUMBER_OF_MEASUREMENTS), String(count_sends));
		if (sending_time > 0) {
			add_table_row_from_value(page_content, F(INTL_TIME_SENDING_MS), String(sending_time), "ms");
		}
	}

	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	end_html_page(page_content);
}

/*****************************************************************
 * Webserver set debug level                                     *
 *****************************************************************/
static void webserver_debug_level() {
	if (!webserver_request_auth())
	{ return; }

	RESERVE_STRING(page_content, LARGE_STR);
	start_html_page(page_content, FPSTR(INTL_DEBUG_LEVEL));
	debug_outln_info(F("ws: debug level ..."));

	if (server.hasArg("lvl")) {
		const int lvl = server.arg("lvl").toInt();
		if (lvl >= 0 && lvl <= 5) {
			cfg::debug = lvl;
			page_content += F("<h3>");
			page_content += FPSTR(INTL_DEBUG_SETTING_TO);
			page_content += ' ';

			const __FlashStringHelper* lvlText;
			switch (lvl) {
			case DEBUG_ERROR:
				lvlText = F(INTL_ERROR);
				break;
			case DEBUG_WARNING:
				lvlText = F(INTL_WARNING);
				break;
			case DEBUG_MIN_INFO:
				lvlText = F(INTL_MIN_INFO);
				break;
			case DEBUG_MED_INFO:
				lvlText = F(INTL_MED_INFO);
				break;
			case DEBUG_MAX_INFO:
				lvlText = F(INTL_MAX_INFO);
				break;
			default:
				lvlText = F(INTL_NONE);
			}

			page_content += lvlText;
			page_content += F(".</h3>");
		}
	}
	end_html_page(page_content);
}

/*****************************************************************
 * Webserver remove config                                       *
 *****************************************************************/
static void webserver_removeConfig() {
	if (!webserver_request_auth())
	{ return; }

	RESERVE_STRING(page_content, LARGE_STR);
	start_html_page(page_content, FPSTR(INTL_DELETE_CONFIG));
	debug_outln_info(F("ws: removeConfig ..."));

	if (server.method() == HTTP_GET) {
		page_content += FPSTR(WEB_REMOVE_CONFIG_CONTENT);

	} else {
		// Silently remove the desaster backup
		SPIFFS.remove(F("/config.json.old"));
		if (SPIFFS.exists(F("/config.json"))) {	//file exists
			debug_outln_info(F("removing config.json..."));
			if (SPIFFS.remove(F("/config.json"))) {
				page_content += F("<h3>" INTL_CONFIG_DELETED ".</h3>");
			} else {
				page_content += F("<h3>" INTL_CONFIG_CAN_NOT_BE_DELETED ".</h3>");
			}
		} else {
			page_content += F("<h3>" INTL_CONFIG_NOT_FOUND ".</h3>");
		}
	}
	end_html_page(page_content);
}

/*****************************************************************
 * Webserver reset NodeMCU                                       *
 *****************************************************************/
static void webserver_reset() {
	if (!webserver_request_auth())
	{ return; }

	String page_content;
	page_content.reserve(512);

	start_html_page(page_content, FPSTR(INTL_RESTART_SENSOR));
	debug_outln_info(F("ws: reset ..."));

	if (server.method() == HTTP_GET) {
		page_content += FPSTR(WEB_RESET_CONTENT);
	} else {
		sensor_restart();
	}
	end_html_page(page_content);
}

/*****************************************************************
 * Webserver data.json                                           *
 *****************************************************************/
static void webserver_data_json() {
	String s1;
	unsigned long age = 0;

	debug_outln_info(F("ws: data json..."));
	if (!count_sends) {
		s1 = FPSTR(data_first_part);
		s1 += "]}";
		age = cfg::sending_intervall_ms - msSince(starttime);
		if (age > cfg::sending_intervall_ms) {
			age = 0;
		}
		age = 0 - age;
	} else {
		s1 = last_data_string;
		debug_outln(last_data_string, DEBUG_MED_INFO);
		age = msSince(starttime);
		if (age > cfg::sending_intervall_ms) {
			age = 0;
		}
	}
	String s2 = F(", \"age\":\"");
	s2 += String((long)((age + 500) / 1000));
	s2 += F("\", \"sensordatavalues\"");
	s1.replace(F(", \"sensordatavalues\""), s2);
	server.send(200, FPSTR(TXT_CONTENT_TYPE_JSON), s1);
}

/*****************************************************************
 * Webserver prometheus metrics endpoint                         *
 *****************************************************************/
static void webserver_prometheus_endpoint() {
	debug_outln_info(F("ws: prometheus endpoint..."));
	String data_4_prometheus = F("software_version{version=\"" SOFTWARE_VERSION_STR "\",{id}} 1\nuptime_ms{{id}} {up}\nsending_intervall_ms{{id}} {si}\nnumber_of_measurements{{id}} {cs}\n");
	debug_outln_info(F("Parse JSON for Prometheus"));
	String id(F("node=\"" SENSOR_BASENAME));
	id += esp_chipid;
	id += '\"';
	data_4_prometheus.replace("{id}", id);
	data_4_prometheus.replace("{up}", String(msSince(time_point_device_start_ms)));
	data_4_prometheus.replace("{si}", String(cfg::sending_intervall_ms));
	data_4_prometheus.replace("{cs}", String(count_sends));
	DynamicJsonDocument json2data(JSON_BUFFER_SIZE);
	DeserializationError err = deserializeJson(json2data, last_data_string);
	if (!err) {
		for (JsonObject measurement : json2data[FPSTR(JSON_SENSOR_DATA_VALUES)].as<JsonArray>()) {
			data_4_prometheus += measurement["value_type"].as<char*>();
			data_4_prometheus += '{';
			data_4_prometheus += id;
			data_4_prometheus += "} ";
			data_4_prometheus += measurement["value"].as<char*>();
			data_4_prometheus += '\n';
		}
		data_4_prometheus += F("last_sample_age_ms{");
		data_4_prometheus += id;
		data_4_prometheus += "} ";
		data_4_prometheus += String(msSince(starttime));
		data_4_prometheus += '\n';
	} else {
		debug_outln_error(FPSTR(DBG_TXT_DATA_READ_FAILED));
	}
	debug_outln(data_4_prometheus, DEBUG_MED_INFO);
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN), data_4_prometheus);
}

/*****************************************************************
 * Webserver Images                                              *
 *****************************************************************/
static void webserver_images() {
	server.sendHeader(F("Cache-Control"), F("max-age=2592000, public"));

	if (server.arg("name") == F("luftdaten_logo")) {
		debug_outln_info(F("ws: luftdaten_logo..."));
		server.send_P(200, TXT_CONTENT_TYPE_IMAGE_PNG,
			LUFTDATEN_INFO_LOGO_PNG, LUFTDATEN_INFO_LOGO_PNG_SIZE);
	} else {
		webserver_not_found();
	}
}

/*****************************************************************
 * Webserver page not found                                      *
 *****************************************************************/
static void webserver_not_found() {
	last_page_load = millis();
	debug_outln_info(F("ws: not found ..."));
	if (WiFi.status() != WL_CONNECTED) {
		if ((server.uri().indexOf(F("success.html")) != -1) || (server.uri().indexOf(F("detect.html")) != -1)) {
			server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), FPSTR(WEB_IOS_REDIRECT));
		} else {
			sendHttpRedirect();
		}
	} else {
		server.send(404, FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN), F("Not found."));
	}
}

/*****************************************************************
 * Webserver setup                                               *
 *****************************************************************/
static void setup_webserver() {
	server.on("/", webserver_root);
	server.on(F("/config"), webserver_config);
	server.on(F("/wifi"), webserver_wifi);
	server.on(F("/values"), webserver_values);
	server.on(F("/status"), webserver_status);
	server.on(F("/generate_204"), webserver_config);
	server.on(F("/fwlink"), webserver_config);
	server.on(F("/debug"), webserver_debug_level);
	server.on(F("/removeConfig"), webserver_removeConfig);
	server.on(F("/reset"), webserver_reset);
	server.on(F("/data.json"), webserver_data_json);
	server.on(F("/metrics"), webserver_prometheus_endpoint);
	server.on(F("/images"), webserver_images);
	server.onNotFound(webserver_not_found);

	debug_outln_info(F("Starting Webserver... "), WiFi.localIP().toString());
	server.begin();
}

static int selectChannelForAp() {
	std::array<int, 14> channels_rssi;
	std::fill(channels_rssi.begin(), channels_rssi.end(), -100);

	for (unsigned i = 0; i < count_wifiInfo; i++) {
		if (wifiInfo[i].RSSI > channels_rssi[wifiInfo[i].channel]) {
			channels_rssi[wifiInfo[i].channel] = wifiInfo[i].RSSI;
		}
	}

	if ((channels_rssi[1] < channels_rssi[6]) && (channels_rssi[1] < channels_rssi[11])) {
		return 1;
	} else if ((channels_rssi[6] < channels_rssi[1]) && (channels_rssi[6] < channels_rssi[11])) {
		return 6;
	} else {
		return 11;
	}
}


/****************************************************************
 * ENABLE WEB-SERVER OFFLINE
 * **************************************************************/
void enableWebServerOffline(){
	debug_outln_info(F("Starting WiFiManager"));
	debug_outln_info(F("AP ID: "), String(cfg::fs_ssid));
	debug_outln_info(F("Password: "), String(cfg::fs_pwd));

	WiFi.disconnect(true);

	WiFi.mode(WIFI_AP);
	const IPAddress apIP(192, 168, 4, 1);
	WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
	WiFi.softAP(cfg::fs_ssid, cfg::fs_pwd, selectChannelForAp());
	// In case we create a unique password at first start
	debug_outln_info(F("AP Password is: "), cfg::fs_pwd);

	DNSServer dnsServer;
	// Ensure we don't poison the client DNS cache
	dnsServer.setTTL(0);
	dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer.start(53, "*", apIP);							// 53 is port for DNS server

	setup_webserver();
}

/*****************************************************************
 * WifiConfig                                                    *
 *****************************************************************/
static void wifiConfig() {
	debug_outln_info(F("Starting WiFiManager"));
	debug_outln_info(F("AP ID: "), String(cfg::fs_ssid));
	debug_outln_info(F("Password: "), String(cfg::fs_pwd));

	wificonfig_loop = true;

	WiFi.disconnect(true);
	debug_outln_info(F("scan for wifi networks..."));
	count_wifiInfo = WiFi.scanNetworks(false /* scan async */, true /* show hidden networks */);
	delete [] wifiInfo;
	wifiInfo = new struct_wifiInfo[count_wifiInfo];

	for (int i = 0; i < count_wifiInfo; i++) {
		String SSID;
		uint8_t* BSSID;

		memset(&wifiInfo[i], 0, sizeof(struct_wifiInfo));
#if defined(ESP8266)
		WiFi.getNetworkInfo(i, SSID, wifiInfo[i].encryptionType,
			wifiInfo[i].RSSI, BSSID, wifiInfo[i].channel,
			wifiInfo[i].isHidden);
#else
		WiFi.getNetworkInfo(i, SSID, wifiInfo[i].encryptionType,
			wifiInfo[i].RSSI, BSSID, wifiInfo[i].channel);
#endif
		SSID.toCharArray(wifiInfo[i].ssid, sizeof(wifiInfo[0].ssid));
	}

	WiFi.mode(WIFI_AP);
	const IPAddress apIP(192, 168, 4, 1);
	WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
	WiFi.softAP(cfg::fs_ssid, cfg::fs_pwd, selectChannelForAp());
	// In case we create a unique password at first start
	debug_outln_info(F("AP Password is: "), cfg::fs_pwd);
	
	DNSServer dnsServer;
	// Ensure we don't poison the client DNS cache
	dnsServer.setTTL(0);
	dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer.start(53, "*", apIP);							// 53 is port for DNS server

	setup_webserver();

	// 10 minutes timeout for wifi config
	last_page_load = millis();
	while ((millis() - last_page_load) < cfg::time_for_wifi_config + 500) {
		dnsServer.processNextRequest();
		server.handleClient();
#if defined(ESP8266)
		wdt_reset(); // nodemcu is alive
		MDNS.update();
#endif
		yield();
	}

	WiFi.softAPdisconnect(true);
	WiFi.mode(WIFI_STA);

	dnsServer.stop();
	delay(100);

	debug_outln_info(FPSTR(DBG_TXT_CONNECTING_TO), cfg::wlanssid);

	WiFi.begin(cfg::wlanssid, cfg::wlanpwd);

	debug_outln_info(F("---- Result Webconfig ----"));
	debug_outln_info(F("WLANSSID: "), cfg::wlanssid);
	debug_outln_info(FPSTR(DBG_TXT_SEP));
	debug_outln_info_bool(F("PMS: "), cfg::pms_read);
	debug_outln_info_bool(F("DHT: "), cfg::dht_read);
	debug_outln_info(FPSTR(DBG_TXT_SEP));
	debug_outln_info_bool(F("SensorCommunity: "), cfg::send2dusti);
	debug_outln_info_bool(F("Madavi: "), cfg::send2madavi);
	debug_outln_info_bool(F("CSV: "), cfg::send2csv);
	debug_outln_info(FPSTR(DBG_TXT_SEP));
	debug_outln_info_bool(F("Autoupdate: "), cfg::auto_update);
	debug_outln_info(F("Debug: "), String(cfg::debug));
	wificonfig_loop = false;
}

static void waitForWifiToConnect(int maxRetries) {
	int retryCount = 0;
	while ((WiFi.status() != WL_CONNECTED) && (retryCount < maxRetries)) {
		delay(500);
		debug_out(".", DEBUG_MIN_INFO);
		++retryCount;
	}
}

/*****************************************************************
 * WiFi auto connecting script                                   *
 *****************************************************************/
static void connectWifi() {
#if defined(ESP8266)
	// Enforce Rx/Tx calibration
	system_phy_set_powerup_option(1);
	// 20dBM == 100mW == max tx power allowed in europe
	WiFi.setOutputPower(20.0f);
	WiFi.setSleepMode(WIFI_NONE_SLEEP);
	WiFi.setPhyMode(WIFI_PHY_MODE_11N);
	delay(100);
#endif
	if (WiFi.getAutoConnect()) {
		WiFi.setAutoConnect(false);
	}
	if (!WiFi.getAutoReconnect()) {
		WiFi.setAutoReconnect(true);
	}
	WiFi.mode(WIFI_STA);
	WiFi.hostname(cfg::fs_ssid);
	WiFi.begin(cfg::wlanssid, cfg::wlanpwd); // Start WiFI

	debug_outln_info(FPSTR(DBG_TXT_CONNECTING_TO), cfg::wlanssid);

	waitForWifiToConnect(40);
	debug_outln_info(emptyString);
	if (WiFi.status() != WL_CONNECTED) {
		String fss(cfg::fs_ssid);
		wifiConfig();
		if (WiFi.status() != WL_CONNECTED) {
			waitForWifiToConnect(20);
			debug_outln_info(emptyString);
		}
	}
	debug_outln_info(F("WiFi connected, IP is: "), WiFi.localIP().toString());
	last_signal_strength = WiFi.RSSI();

	if (MDNS.begin(cfg::fs_ssid)) {
		MDNS.addService("http", "tcp", 80);
		MDNS.addServiceTxt("http", "tcp", "PATH", "/config");
	}
}

#if defined(ESP8266)
BearSSL::X509List x509_dst_root_ca(dst_root_ca_x3);

static void configureCACertTrustAnchor(WiFiClientSecure* client) {
	constexpr time_t fw_built_year = (__DATE__[ 7] - '0') * 1000 + \
							  (__DATE__[ 8] - '0') *  100 + \
							  (__DATE__[ 9] - '0') *   10 + \
							  (__DATE__[10] - '0');
	if (time(nullptr) < (fw_built_year - 1970) * 365 * 24 * 3600) {
		debug_outln_info(F("Time incorrect; Disabling CA verification."));
		client->setInsecure();
	}
	else {
		client->setTrustAnchors(&x509_dst_root_ca);
	}
}
#endif

static WiFiClient* getNewLoggerWiFiClient(const LoggerEntry logger) {

	WiFiClient* _client;
	if (loggerConfigs[logger].session) {
		_client = new WiFiClientSecure;
#if defined(ESP8266)
		static_cast<WiFiClientSecure*>(_client)->setSession(loggerConfigs[logger].session);
		static_cast<WiFiClientSecure*>(_client)->setBufferSizes(1024, TCP_MSS > 1024 ? 2048 : 1024);
		switch (logger) {
		case Loggeraircms:
		case LoggerInflux:
		case LoggerCustom:
		case LoggerFSapp:
			static_cast<WiFiClientSecure*>(_client)->setInsecure();
			break;
		default:
			configureCACertTrustAnchor(static_cast<WiFiClientSecure*>(_client));
		}
#endif
	} else {
		_client = new WiFiClient;
	}
	_client->setTimeout(20000);
	return _client;
}
/*****************************************************************
 * send data to rest api                                         *
 *****************************************************************/
static unsigned long sendData(const LoggerEntry logger, const String& data, const int pin, const char* host, const char* url) {

	unsigned long start_send = millis();
	const __FlashStringHelper* contentType;
	int result = 0;

	String s_Host(FPSTR(host));
	String s_url(FPSTR(url));

	switch (logger) {
	case Loggeraircms:
		contentType = FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN);
		break;
	case LoggerInflux:
		contentType = FPSTR(TXT_CONTENT_TYPE_INFLUXDB);
		break;
	default:
		contentType = FPSTR(TXT_CONTENT_TYPE_JSON);
		break;
	}

	std::unique_ptr<WiFiClient> client(getNewLoggerWiFiClient(logger));

	HTTPClient http;
	http.setTimeout(20 * 1000);
	http.setUserAgent(SOFTWARE_VERSION + '/' + esp_chipid);
    http.setReuse(false);
	bool send_success = false;
	if (logger == LoggerCustom && (*cfg::user_custom || *cfg::pwd_custom)) {
		http.setAuthorization(cfg::user_custom, cfg::pwd_custom);
	}
	if (logger == LoggerInflux && (*cfg::user_influx || *cfg::pwd_influx)) {
		http.setAuthorization(cfg::user_influx, cfg::pwd_influx);
	}
		if (http.begin(*client, s_Host, loggerConfigs[logger].destport, s_url, !!loggerConfigs[logger].session)) {
		http.addHeader(F("Content-Type"), contentType);
		http.addHeader(F("X-Sensor"), String(F(SENSOR_BASENAME)) + esp_chipid);
		if (pin) {
			http.addHeader(F("X-PIN"), String(pin));
		}

		result = http.POST(data);

		if (result >= HTTP_CODE_OK && result <= HTTP_CODE_ALREADY_REPORTED) {
			debug_outln_info(F("Succeeded - "), s_Host);
			send_success = true;
		} else if (result >= HTTP_CODE_BAD_REQUEST) {
			debug_outln_info(F("Request failed with error: "), String(result));
			debug_outln_info(F("Details:"), http.getString());
		}
		http.end();
	} else {
		debug_outln_info(F("Failed connecting to "), s_Host);
	}

	if (!send_success && result != 0) {
		sendData_error_count++;
		last_sendData_returncode = result;
	}

	return millis() - start_send;
}

/*****************************************************************
 * send data to mqtt api                                         *
 *****************************************************************/
// rejected (see issue #33)

/*****************************************************************
 * send data to influxdb                                         *
 *****************************************************************/
static void create_influxdb_string_from_data(String& data_4_influxdb, const String& data) {
	debug_outln_verbose(F("Parse JSON for influx DB: "), data);
	DynamicJsonDocument json2data(JSON_BUFFER_SIZE);
	DeserializationError err = deserializeJson(json2data, data);
	if (!err) {
		data_4_influxdb += cfg::measurement_name_influx;
		data_4_influxdb += F(",node=" SENSOR_BASENAME);
		data_4_influxdb += esp_chipid + " ";
		for (JsonObject measurement : json2data[FPSTR(JSON_SENSOR_DATA_VALUES)].as<JsonArray>()) {
			data_4_influxdb += measurement["value_type"].as<char*>();
			data_4_influxdb += '=';
			data_4_influxdb += measurement["value"].as<char*>();
			data_4_influxdb += ',';
		}
		if ((unsigned)(data_4_influxdb.lastIndexOf(',') + 1) == data_4_influxdb.length()) {
			data_4_influxdb.remove(data_4_influxdb.length() - 1);
		}

		data_4_influxdb += '\n';
	} else {
		debug_outln_error(FPSTR(DBG_TXT_DATA_READ_FAILED));
	}
}

/*****************************************************************
 * send data as csv to serial out                                *
 *****************************************************************/
static void send_csv(const String& data) {
	DynamicJsonDocument json2data(JSON_BUFFER_SIZE);
	DeserializationError err = deserializeJson(json2data, data);
	debug_outln_info(F("CSV Output: "), data);
	if (!err) {
		String headline = F("Timestamp_ms;");
		String valueline(act_milli);
		valueline += ';';
		for (JsonObject measurement : json2data[FPSTR(JSON_SENSOR_DATA_VALUES)].as<JsonArray>()) {
			headline += measurement["value_type"].as<char*>();
			headline += ';';
			valueline += measurement["value"].as<char*>();
			valueline += ';';
		}
		static bool first_csv_line = true;
		if (first_csv_line) {
			if (headline.length() > 0) {
				headline.remove(headline.length() - 1);
			}
			Serial.println(headline);
			first_csv_line = false;
		}
		if (valueline.length() > 0) {
			valueline.remove(valueline.length() - 1);
		}
		Serial.println(valueline);
	} else {
		debug_outln_error(FPSTR(DBG_TXT_DATA_READ_FAILED));
	}
}

/*****************************************************************
 * read DHT22 sensor values                                      *
 *****************************************************************/
static void fetchSensorDHT(String& s) {
	debug_outln_verbose(FPSTR(DBG_TXT_START_READING), FPSTR(SENSORS_DHT22));

	// Check if valid number if non NaN (not a number) will be send.
	last_value_DHT_T = -128;
	last_value_DHT_H = -1;

	int count = 0;
	const int MAX_ATTEMPTS = 5;
	while ((count++ < MAX_ATTEMPTS)) {
		auto t = dht.readTemperature();
		auto h = dht.readHumidity();
		if (isnan(t) || isnan(h)) {
			delay(100);
			t = dht.readTemperature(false);
			h = dht.readHumidity();
		}
		if (isnan(t) || isnan(h)) {
			debug_outln_error(F("DHT11/DHT22 read failed"));
		} else {
			last_value_DHT_T = t;
			last_value_DHT_H = h;
			add_Value2Json(s, F("temperature"), FPSTR(DBG_TXT_TEMPERATURE), last_value_DHT_T);
			add_Value2Json(s, F("humidity"), FPSTR(DBG_TXT_HUMIDITY), last_value_DHT_H);
			break;
		}
	}
	debug_outln_info(FPSTR(DBG_TXT_SEP));

	debug_outln_verbose(FPSTR(DBG_TXT_END_READING), FPSTR(SENSORS_DHT22));
}

/*****************************************************************
 * read Plantronic PM sensor sensor values                       *
 *****************************************************************/
static void fetchSensorPMS(String& s) {
	char buffer;
	int value;
	int len = 0;
	int pm1_serial = 0;
	int pm10_serial = 0;
	int pm25_serial = 0;
	int checksum_is = 0;
	int checksum_should = 0;
	bool checksum_ok = false;
	int frame_len = 24;				// min. frame length

	debug_outln_verbose(FPSTR(DBG_TXT_START_READING), FPSTR(SENSORS_PMSx003));
	if (msSince(starttime) < (cfg::sending_intervall_ms - (WARMUPTIME_SDS_MS + READINGTIME_SDS_MS))) {
		if (is_PMS_running) {
			is_PMS_running = PMS_cmd(PmSensorCmd::Stop);
		}
	} else {
		if (! is_PMS_running) {
			is_PMS_running = PMS_cmd(PmSensorCmd::Start);
		}

		while (serialSDS.available() > 0) {
			buffer = serialSDS.read();
			debug_outln(String(len) + " - " + String(buffer, DEC) + " - " + String(buffer, HEX) + " - " + int(buffer) + " .", DEBUG_MAX_INFO);
//			"aa" = 170, "ab" = 171, "c0" = 192
			value = int(buffer);
			switch (len) {
			case 0:
				if (value != 66) {
					len = -1;
				};
				break;
			case 1:
				if (value != 77) {
					len = -1;
				};
				break;
			case 2:
				checksum_is = value;
				break;
			case 3:
				frame_len = value + 4;
				break;
			case 10:
				pm1_serial += ( value << 8);
				break;
			case 11:
				pm1_serial += value;
				break;
			case 12:
				pm25_serial = ( value << 8);
				break;
			case 13:
				pm25_serial += value;
				break;
			case 14:
				pm10_serial = ( value << 8);
				break;
			case 15:
				pm10_serial += value;
				break;
			case 22:
				if (frame_len == 24) {
					checksum_should = ( value << 8 );
				};
				break;
			case 23:
				if (frame_len == 24) {
					checksum_should += value;
				};
				break;
			case 30:
				checksum_should = ( value << 8 );
				break;
			case 31:
				checksum_should += value;
				break;
			}
			if ((len > 2) && (len < (frame_len - 2))) { checksum_is += value; }
			len++;
			if (len == frame_len) {
				debug_outln_verbose(FPSTR(DBG_TXT_CHECKSUM_IS), String(checksum_is + 143));
				debug_outln_verbose(FPSTR(DBG_TXT_CHECKSUM_SHOULD), String(checksum_should));
				if (checksum_should == (checksum_is + 143)) {
					checksum_ok = true;
				} else {
					len = 0;
				};
				if (checksum_ok && (msSince(starttime) > (cfg::sending_intervall_ms - READINGTIME_SDS_MS))) {
					if ((! isnan(pm1_serial)) && (! isnan(pm10_serial)) && (! isnan(pm25_serial))) {
						pms_pm1_sum += pm1_serial;
						pms_pm10_sum += pm10_serial;
						pms_pm25_sum += pm25_serial;
						if (pms_pm1_min > pm1_serial) {
							pms_pm1_min = pm1_serial;
						}
						if (pms_pm1_max < pm1_serial) {
							pms_pm1_max = pm1_serial;
						}
						if (pms_pm25_min > pm25_serial) {
							pms_pm25_min = pm25_serial;
						}
						if (pms_pm25_max < pm25_serial) {
							pms_pm25_max = pm25_serial;
						}
						if (pms_pm10_min > pm10_serial) {
							pms_pm10_min = pm10_serial;
						}
						if (pms_pm10_max < pm10_serial) {
							pms_pm10_max = pm10_serial;
						}
						debug_outln_verbose(F("PM1 (sec.): "), String(pm1_serial));
						debug_outln_verbose(F("PM2.5 (sec.): "), String(pm25_serial));
						debug_outln_verbose(F("PM10 (sec.) : "), String(pm10_serial));
						pms_val_count++;
					}
					len = 0;
					checksum_ok = false;
					pm1_serial = 0;
					pm10_serial = 0;
					pm25_serial = 0;
					checksum_is = 0;
				}
			}
			yield();
		}
	}
	if (send_now) {
		last_value_PMS_P0 = -1;
		last_value_PMS_P1 = -1;
		last_value_PMS_P2 = -1;
		if (pms_val_count > 2) {
			pms_pm1_sum = pms_pm1_sum - pms_pm1_min - pms_pm1_max;
			pms_pm10_sum = pms_pm10_sum - pms_pm10_min - pms_pm10_max;
			pms_pm25_sum = pms_pm25_sum - pms_pm25_min - pms_pm25_max;
			pms_val_count = pms_val_count - 2;
		}
		if (pms_val_count > 0) {
			last_value_PMS_P0 = float(pms_pm1_sum) / float(pms_val_count);
			last_value_PMS_P1 = float(pms_pm10_sum) / float(pms_val_count);
			last_value_PMS_P2 = float(pms_pm25_sum) / float(pms_val_count);
			add_Value2Json(s, F("PMS_P0"), F("PM1:   "), last_value_PMS_P0);
			add_Value2Json(s, F("PMS_P1"), F("PM10:  "), last_value_PMS_P1);
			add_Value2Json(s, F("PMS_P2"), F("PM2.5: "), last_value_PMS_P2);
			debug_outln_info(FPSTR(DBG_TXT_SEP));
		}
		pms_pm1_sum = 0;
		pms_pm10_sum = 0;
		pms_pm25_sum = 0;
		pms_val_count = 0;
		pms_pm1_max = 0;
		pms_pm1_min = 20000;
		pms_pm10_max = 0;
		pms_pm10_min = 20000;
		pms_pm25_max = 0;
		pms_pm25_min = 20000;
		if (cfg::sending_intervall_ms > (WARMUPTIME_SDS_MS + READINGTIME_SDS_MS)) {
			is_PMS_running = PMS_cmd(PmSensorCmd::Stop);
		}
	}

	debug_outln_verbose(FPSTR(DBG_TXT_END_READING), FPSTR(SENSORS_PMSx003));
}

/*****************************************************************
 * OTAUpdate                                                     *
 *****************************************************************/

static bool fwDownloadStream(WiFiClientSecure& client, const String& url, Stream* ostream) {

	HTTPClient http;
	int bytes_written = -1;

	http.setTimeout(20 * 1000);
    http.setReuse(false);

	debug_outln_verbose(F("HTTP GET: "), String(FPSTR(FW_DOWNLOAD_HOST)) + ':' + String(FW_DOWNLOAD_PORT) + url);

	if (http.begin(client, FPSTR(FW_DOWNLOAD_HOST), FW_DOWNLOAD_PORT, url)) {
		int r = http.GET();
		debug_outln_verbose(F("GET r: "), String(r));
		last_update_returncode = r;
		if (r == HTTP_CODE_OK) {
			bytes_written = http.writeToStream(ostream);
		}
		http.end();
	}

	if (bytes_written > 0)
		return true;

	return false;
}

static bool fwDownloadStreamFile(WiFiClientSecure& client, const String& url, const String& fname) {

	String fname_new(fname);
	fname_new += F(".new");
	bool downloadSuccess = false;

	File fwFile = SPIFFS.open(fname_new, "w");
	if (fwFile) {
		downloadSuccess = fwDownloadStream(client, url, &fwFile);
		fwFile.close();
		if (downloadSuccess) {
			SPIFFS.remove(fname);
			SPIFFS.rename(fname_new, fname);
			debug_outln_info(F("Success downloading: "), url);
		}
	}

	if (downloadSuccess)
		return true;

	SPIFFS.remove(fname_new);
	return false;
}

#if defined(ESP8266)
static bool launchUpdateLoader(const String& md5) {

	File loaderFile = SPIFFS.open(F("/loader.bin"), "r");
	if (!loaderFile) {
		return false;
	}

	if (!Update.begin(loaderFile.size(), U_FLASH)) {
		return false;
	}

	if (md5.length() && !Update.setMD5(md5.c_str())) {
		return false;
	}

	if (Update.writeStream(loaderFile) != loaderFile.size()) {
		return false;
	}
	loaderFile.close();

	if (!Update.end()) {
		return false;
	}

	debug_outln_info(F("Erasing SDK config."));
	ESP.eraseConfig();

	sensor_restart();
	return true;
}
#endif

static void twoStageOTAUpdate() {

	if (!cfg::auto_update) return;

#if defined(ESP8266)
	debug_outln_info(F("twoStageOTAUpdate"));

	String lang_variant(cfg::current_lang);
	if (lang_variant.length() != 2) {
		lang_variant = CURRENT_LANG;
	}
	lang_variant.toLowerCase();

	String fetch_name(F(OTA_BASENAME "/update/latest_"));
	if (cfg::use_beta) {
		fetch_name = F(OTA_BASENAME "/beta/latest_");
	}
	fetch_name += lang_variant;
	fetch_name += F(".bin");

	WiFiClientSecure client;
	BearSSL::Session clientSession;

	client.setBufferSizes(1024, TCP_MSS > 1024 ? 2048 : 1024);
	client.setSession(&clientSession);
	configureCACertTrustAnchor(&client);

	String fetch_md5_name(fetch_name);
	fetch_md5_name += F(".md5");

	StreamString newFwmd5;
	if (!fwDownloadStream(client, fetch_md5_name, &newFwmd5))
		return;

	newFwmd5.trim();
	if (newFwmd5 == ESP.getSketchMD5()) {
		debug_outln_verbose(F("No newer version available."));
		return;
	}

	debug_outln_info(F("Update md5: "), newFwmd5);
	debug_outln_info(F("Sketch md5: "), ESP.getSketchMD5());

	// We're entering update phase, kill off everything else
	WiFiUDP::stopAll();
	WiFiClient::stopAllExcept(&client);
	delay(100);

	String firmware_name(F("/firmware.bin"));
	String firmware_md5(F("/firmware.bin.md5"));
	String loader_name(F("/loader.bin"));
	if (!fwDownloadStreamFile(client, fetch_name, firmware_name))
		return;
	if (!fwDownloadStreamFile(client, fetch_md5_name, firmware_md5))
		return;
	if (!fwDownloadStreamFile(client, FPSTR(FW_2ND_LOADER_URL), loader_name))
		return;

	File fwFile = SPIFFS.open(firmware_name, "r");
	if (!fwFile) {
		SPIFFS.remove(firmware_name);
		SPIFFS.remove(firmware_md5);
		debug_outln_error(F("Failed reopening fw file.."));
		return;
	}
	size_t fwSize = fwFile.size();
	MD5Builder md5;
	md5.begin();
	md5.addStream(fwFile, fwSize);
	md5.calculate();
	fwFile.close();
	String md5String = md5.toString();

	// Firmware is always at least 128 kB and padded to 16 bytes
	if (fwSize < (1<<17) || (fwSize % 16 != 0) || newFwmd5 != md5String) {
		debug_outln_info(F("FW download failed validation.. deleting"));
		SPIFFS.remove(firmware_name);
		SPIFFS.remove(firmware_md5);
		return;
	}

	StreamString loaderMD5;
	if (!fwDownloadStream(client, String(FPSTR(FW_2ND_LOADER_URL)) + F(".md5"), &loaderMD5))
		return;

	loaderMD5.trim();

	debug_outln_info(F("launching 2nd stage"));
	if (!launchUpdateLoader(loaderMD5)) {
		debug_outln_error(FPSTR(DBG_TXT_UPDATE_FAILED));
		SPIFFS.remove(firmware_name);
		SPIFFS.remove(firmware_md5);
		return;
	}
#endif
}

static String displayGenerateFooter(unsigned int screen_count) {
	String display_footer;
	for (unsigned int i = 0; i < screen_count; ++i) {
		display_footer += (i != (next_display_count % screen_count)) ? " . " : " o ";
	}
	return display_footer;
}

static void powerOnTestSensors() {
	if (cfg::wifi_enabled)
	{
		debug_outln_info(F("WIFI ENABLED..."));
	}
	else
	{
		debug_outln_info(F("WIFI DISABLED..."));
	}

	if (cfg::pms_read) {
		debug_outln_info(F("Read PMS(1,3,5,6,7)003..."));
		PMS_cmd(PmSensorCmd::Start);
		delay(100);
		PMS_cmd(PmSensorCmd::ContinuousMode);
		delay(100);
		debug_outln_info(F("Stopping PMS..."));
		is_PMS_running = PMS_cmd(PmSensorCmd::Stop);
	}

	if (cfg::dht_read) {
		dht.begin();										// Start DHT
		debug_outln_info(F("Read DHT..."));
	}

}

static void logEnabledAPIs() {
	debug_outln_info(F("Send to :"));
	if (cfg::send2dusti) {
		debug_outln_info(F("sensor.community"));
	}
  	if (cfg::send2cfa) {
    	debug_outln_info(F("CFA"));
  	}

	if (cfg::send2fsapp) {
		debug_outln_info(F("Feinstaub-App"));
	}

	if (cfg::send2madavi) {
		debug_outln_info(F("Madavi.de"));
	}

	if (cfg::send2csv) {
		debug_outln_info(F("Serial as CSV"));
	}

	if (cfg::send2custom) {
		debug_outln_info(F("custom API"));
	}

	if (cfg::send2aircms) {
		debug_outln_info(F("aircms API"));
	}

	if (cfg::send2influx) {
		debug_outln_info(F("custom influx DB"));
	}
	debug_outln_info(FPSTR(DBG_TXT_SEP));
	if (cfg::auto_update) {
		debug_outln_info(F("Auto-Update active..."));
	}
}

static void setupNetworkTime() {
	// server name ptrs must be persisted after the call to configTime because internally
	// the pointers are stored see implementation of lwip sntp_setservername()
	static char ntpServer1[18], ntpServer2[18], ntpServer3[18];
#if defined(ESP8266)
	settimeofday_cb([]() {
		if (!sntp_time_set) {
			time_t now = time(nullptr);
			debug_outln_info(F("SNTP synced: "), ctime(&now));
			twoStageOTAUpdate();
			last_update_attempt = millis();
		}
		sntp_time_set++;
	});
#endif
	strcpy_P(ntpServer1, NTP_SERVER_1);
	strcpy_P(ntpServer2, NTP_SERVER_2);
	configTime(0, 0, ntpServer1, ntpServer2, ntpServer3);
}

static unsigned long sendDataToOptionalApis(const String &data) {
	unsigned long sum_send_time = 0;

	if (cfg::send2madavi) {
		debug_outln_info(FPSTR(DBG_TXT_SENDING_TO), F("madavi.de: "));
		sum_send_time += sendData(LoggerMadavi, data, 0, HOST_MADAVI, URL_MADAVI);
	}

	if (cfg::send2sensemap && (cfg::senseboxid[0] != '\0')) {
		debug_outln_info(FPSTR(DBG_TXT_SENDING_TO), F("opensensemap: "));
		String sensemap_path(tmpl(FPSTR(URL_SENSEMAP), cfg::senseboxid));
		sum_send_time += sendData(LoggerSensemap, data, 0, HOST_SENSEMAP, sensemap_path.c_str());
	}

	if (cfg::send2fsapp) {
		debug_outln_info(FPSTR(DBG_TXT_SENDING_TO), F("Server FS App: "));
		sum_send_time += sendData(LoggerFSapp, data, 0, HOST_FSAPP, URL_FSAPP);
	}

	if (cfg::send2aircms) {
		debug_outln_info(FPSTR(DBG_TXT_SENDING_TO), F("aircms.online: "));
		unsigned long ts = millis() / 1000;
		String token = WiFi.macAddress();
		String aircms_data("L=");
		aircms_data += esp_chipid;
		aircms_data += "&t=";
		aircms_data += String(ts, DEC);
		aircms_data += F("&airrohr=");
		aircms_data += data;
		String aircms_url(FPSTR(URL_AIRCMS));
		aircms_url += hmac1(sha1Hex(token), aircms_data + token);

		sum_send_time += sendData(Loggeraircms, aircms_data, 0, HOST_AIRCMS, aircms_url.c_str());
	}

	if (cfg::send2influx) {
		debug_outln_info(FPSTR(DBG_TXT_SENDING_TO), F("custom influx db: "));
		RESERVE_STRING(data_4_influxdb, LARGE_STR);
		create_influxdb_string_from_data(data_4_influxdb, data);
		sum_send_time += sendData(LoggerInflux, data_4_influxdb, 0, cfg::host_influx, cfg::url_influx);
	}

	if (cfg::send2custom) {
		String data_to_send = data;
		data_to_send.remove(0, 1);
		String data_4_custom(F("{\"esp8266id\": \""));
		data_4_custom += esp_chipid;
		data_4_custom += "\", ";
		data_4_custom += data_to_send;
		debug_outln_info(FPSTR(DBG_TXT_SENDING_TO), F("custom api: "));
		sum_send_time += sendData(LoggerCustom, data_4_custom, 0, cfg::host_custom, cfg::url_custom);
	}

	if (cfg::send2csv) {
		debug_outln_info(F("## Sending as csv: "));
		send_csv(data);
	}

	return sum_send_time;
}

/*****************************************************************
 * The Setup                                                     *
 *****************************************************************/

void setup(void) {
	Serial.begin(9600);	// Output to Serial at 9600 baud
	atmega328p.begin(9600, SWSERIAL_8N1,ATMEGA_TX,ATMEGA_RX,false,256);
	
#if defined(ESP8266)
	serialSDS.begin(9600, SWSERIAL_8N1, PM_SERIAL_RX, PM_SERIAL_TX);
#endif
#if defined(ESP32)
	serialSDS.begin(9600, SERIAL_8N1, PM_SERIAL_RX, PM_SERIAL_TX);
#endif
	serialSDS.enableIntTx(true);
	serialSDS.setTimeout((12 * 9 * 1000) / 9600);

#if defined(WIFI_LoRa_32_V2)
	// reset the OLED display, e.g. of the heltec_wifi_lora_32 board
	pinMode(RST_OLED, OUTPUT);
	digitalWrite(RST_OLED, LOW);
	delay(50);
	digitalWrite(RST_OLED, HIGH);
#endif

#if defined(ESP8266)
	esp_chipid = std::move(String(ESP.getChipId()));
#endif
#if defined(ESP32)
	uint64_t chipid_num;
	chipid_num = ESP.getEfuseMac();
	esp_chipid = String((uint16_t)(chipid_num >> 32), HEX);
	esp_chipid += String((uint32_t)chipid_num, HEX);
#endif
	cfg::initNonTrivials(esp_chipid.c_str());
	WiFi.persistent(false);

	debug_outln_info(F("airRohr: " SOFTWARE_VERSION_STR "/"), String(CURRENT_LANG));
	if ((airrohr_selftest_failed = !ESP.checkFlashConfig(true) /* after 2.7.0 update: || !ESP.checkFlashCRC() */)) {
		debug_outln_error(F("ERROR: SELF TEST FAILED!"));
		SOFTWARE_VERSION += F("-STF");
	}

	init_config();

	if(cfg::wifi_enabled){
		setupNetworkTime();
		connectWifi();
		setup_webserver();
	}
	if(!cfg::wifi_enabled){
		enableWebServerOffline();
	}
	createLoggerConfigs();
	debug_outln_info(F("\nChipId: "), esp_chipid);

	powerOnTestSensors();
	logEnabledAPIs();

	delay(50);

	// sometimes parallel sending data and web page will stop nodemcu, watchdogtimer set to 120 seconds
#if defined(ESP8266)
	wdt_disable();
#if defined(NDEBUG)
	wdt_enable(120000);
#endif
#endif

	starttime = millis();									// store the start time
	last_update_attempt = time_point_device_start_ms = starttime;
	last_display_millis = starttime_SDS = starttime;
}

/*****************************************************************
 * And action                                                    *
 *****************************************************************/
void loop(void) {
	String result_PMS;

	unsigned sum_send_time = 0;

	act_micro = micros();
	act_milli = millis();
	send_now = msSince(starttime) > cfg::sending_intervall_ms;
	// Wait at least 30s for each NTP server to sync

	if (!sntp_time_set && send_now &&
			msSince(time_point_device_start_ms) < 1000 * 2 * 30 + 5000) {
		debug_outln_info(F("NTP sync not finished yet, skipping send"));
		send_now = false;
		starttime = act_milli;
	}

	sample_count++;

#if defined(ESP8266)
	wdt_reset(); // nodemcu is alive
#endif

	if (last_micro != 0) {
		unsigned long diff_micro = act_micro - last_micro;
		if (max_micro < diff_micro) {
			max_micro = diff_micro;
		}
		if (min_micro > diff_micro) {
			min_micro = diff_micro;
		}
	}
	last_micro = act_micro;

	if (msSince(time_point_device_start_ms) > DURATION_BEFORE_FORCED_RESTART_MS) {
		sensor_restart();
	}

	if (msSince(last_update_attempt) > PAUSE_BETWEEN_UPDATE_ATTEMPTS_MS) {
		twoStageOTAUpdate();
		last_update_attempt = act_milli;
	}

	server.handleClient();
	if(!cfg::wifi_enabled){
		#if defined(ESP8266)
		wdt_reset(); // nodemcu is alive
		#endif
	}
	yield();

	if (send_now) {
		last_signal_strength = WiFi.RSSI();
		RESERVE_STRING(data, LARGE_STR);
		data = FPSTR(data_first_part);
		RESERVE_STRING(result, MED_STR);


		if (cfg::pms_read) {
			data += result_PMS;
      		if(cfg::wifi_enabled) {
				/**
				 * Send PMS value here
				 */
			}
		}
		
		if (cfg::dht_read) {
			// getting temperature and humidity (optional)
			fetchSensorDHT(result);
			data += result;
      		if(cfg::wifi_enabled) {
				/**
				 * Send DHT value here
				 */
			}
			result = emptyString;
		}
	
		add_Value2Json(data, F("samples"), String(sample_count));
		add_Value2Json(data, F("min_micro"), String(min_micro));
		add_Value2Json(data, F("max_micro"), String(max_micro));
		add_Value2Json(data, F("signal"), String(last_signal_strength));

		if ((unsigned)(data.lastIndexOf(',') + 1) == data.length()) {
			data.remove(data.length() - 1);
		}
		data += "]}";

		yield();

		if(cfg::wifi_enabled) {
			sum_send_time += sendDataToOptionalApis(data);
		}

		// https://en.wikipedia.org/wiki/Moving_average#Cumulative_moving_average
		sending_time = (3 * sending_time + sum_send_time) / 4;
		if (sum_send_time > 0) {
			debug_outln_info(F("Time for Sending (ms): "), String(sending_time));
		}

		// reconnect to WiFi if disconnected
		if ((WiFi.status() != WL_CONNECTED && cfg::wifi_enabled)) {
			debug_outln_info(F("Connection lost, reconnecting "));
			WiFi_error_count++;
			WiFi.reconnect();
			waitForWifiToConnect(20);
		}

		// Resetting for next sampling
		last_data_string = std::move(data);
		lowpulseoccupancyP1 = 0;
		lowpulseoccupancyP2 = 0;
		sample_count = 0;
		last_micro = 0;
		min_micro = 1000000000;
		max_micro = 0;
		sum_send_time = 0;
		starttime = millis();								// store the start time
		count_sends++;
	}
	yield();
#if defined(ESP8266)
	MDNS.update();
	serialSDS.perform_work();
#endif

	if (sample_count % 500 == 0) {
//		Serial.println(ESP.getFreeHeap(),DEC);
	}
}
