#include <FastLED.h>
#include <WiFi.h>
#include "LittleFS.h"
#include <AsyncTCP.h>
#include <ElegantOTA.h>
#include <ESPAsyncWebServer.h>
#include <esp_wifi.h>
#include <Arduino_JSON.h>
#include <ESPmDNS.h>
#include <EEPROM.h>
#include "wifi_tools.h"
#include "sectionManager.h"
// For led chips like Neopixels, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806 define both DATA_PIN and CLOCK_PIN
#define DATA_PIN 12
#define CLOCK_PIN 13

#define EEPROM_SIZE 230
#define APSSID_START 0
#define APSSID_END 21
#define APPWD_START 22
#define APPWD_END 43
#define APIP_START 44
#define APIP_END 65
#define WFSSID_START 66
#define WFPWD_START 88
#define WFMOD_START 116
#define WFMOD_END 122
#define WFIP_START 123
#define WFIP_END 142
#define WFNM_START 143
#define WFNM_END 160
#define WFGW_START 161
#define WFGW_END 180
#define WFPD_START 181
#define WFPD_END 200
#define WFSD_START 201
#define WFSD_END 216
#define MAGIC_CODE_SIZE 7
#define MAGIC_CODE_INDEX 217
#define LAST_INT_START 225

// Define the array of leds
CRGB* leds = nullptr;
uint16_t numLeds = 78;
uint16_t numLeds_per_box = 78;
uint8_t boxes = 1;

uint8_t last_int = 0;
String wsid;
String wpass;
String password;
String static_ip;
String myIP;
String LOCAL_IP;
String apname;
String master_config;

const char* VER = "V1.0";
const char* MAGIC_CODE = "AADIBOb";                      //magic code to match
const char* file = "/config.json";                       //master config
const char* s_data_file = "/ServiceData_jsonfile.txt";   //service config
const char* sys_data_file = "/SystemData_jsonfile.txt";  //system config
String Service;
String service_s;
String host_ip;
int port;
int uinterval;
int u_time;
String c_id;

int QOS;  // 0
String U_name;
String s_pass;
String p_topic;
String Http_requestpath;
bool debug = true;
void readSysFile();
void writeEEPROM(int start, const String& param);
void clearEEPROM(int start, int end);
std::string ReadFileToString(const char* filename);

unsigned long ota_progress_millis = 0;
SectionManager* sectionManager = nullptr;
AsyncWebServer server(80);
WiFiClient network;
//####################################### check first time starting ############################
bool checkFirstTime() {
  for (int APaddress = MAGIC_CODE_INDEX; APaddress < (MAGIC_CODE_INDEX + MAGIC_CODE_SIZE); ++APaddress) {
    Serial.println(char(EEPROM.read(APaddress)));
    if (MAGIC_CODE[APaddress - MAGIC_CODE_INDEX] != char(EEPROM.read(APaddress))) {
      return true;
    }
  }
  return false;
}
//####################################### fill ############################
void oneTimeRunCompleted() {
  for (int APaddress = MAGIC_CODE_INDEX; APaddress < (MAGIC_CODE_INDEX + MAGIC_CODE_SIZE); ++APaddress) {
    EEPROM.write(APaddress, MAGIC_CODE[APaddress - MAGIC_CODE_INDEX]);
  }
  EEPROM.commit();
  Serial.println("First Time runcode saved");
}
//####################################### testing wifi connection function ############################
bool testWifi(void) {
  int c = 0;
  Serial.println("Waiting for Wifi to connect");
  while (c < 30) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    delay(500);
    Serial.print(WiFi.status());
    Serial.print(".");
    c++;
  }
  Serial.println("");
  Serial.println("Connect timed out, opening AP");
  LOCAL_IP = "Not Connected";
  WiFi.mode(WIFI_AP);
  return false;
}

//############   Conversion for acceesspoint ip into unsigned int ###################
const int numberOfPieces = 4;
String ipaddress[numberOfPieces];
void ipAdress(String& eap, String& iip1, String& iip2, String& iip3, String& iip4) {

  int counter = 0;
  int lastIndex = 0;
  for (int i = 0; i < eap.length(); i++) {

    if (eap.substring(i, i + 1) == ".") {
      ipaddress[counter] = eap.substring(lastIndex, i);

      lastIndex = i + 1;
      counter++;
    }
    if (i == eap.length() - 1) {
      ipaddress[counter] = eap.substring(lastIndex);
    }
  }
  iip1 = ipaddress[0];
  iip2 = ipaddress[1];
  iip3 = ipaddress[2];
  iip4 = ipaddress[3];
}

void setup() {
  bool firstTime = false;
  // put your setup code here, to run once:
  Serial.begin(115200);  // opens serial port, sets data rate to 115200 bps
  Serial.println(F("Aadi Bob's LED Strips"));

  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("failed to initialise EEPROM");
    delay(3000);
  }
  //########################  reading config file ########################################
  if (!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  Serial.println("LittleFS successfully mounted");
  std::string buf1 = ReadFileToString(file);

  master_config = String(buf1.c_str());

  Serial.println("File Content config:");
  Serial.print(master_config);
  Serial.println("");
  Serial.println("");

  WiFi.mode(WIFI_MODE_APSTA);


  readSysFile();
  firstTime = checkFirstTime();
  //##############################   ACCESS POINT begin on given credential #################################

  if (firstTime) {
    delay(10000);
    Serial.println("");
    Serial.println("FIRST TIME AP ADDRESS SETTING");

    JSONVar root = JSON.parse(master_config);

    if (JSON.typeof(root) == "undefined") {
      Serial.println("Parsing input failed!");
      Serial.println(master_config);
      return;
    }
    String apip = root["AP_IP"];
    clearEEPROM(APIP_START, APIP_END);  //AP IP
    Serial.print("WRITING  default AP IP  :: ");
    Serial.println(apip.c_str());
    writeEEPROM(APIP_START, apip);

    Serial.println("###  FIRST TIME RESET WiFi ### ");
    EEPROM.write(WFPWD_START, 0);   //wifi password
    EEPROM.write(WFMOD_START, 0);   //wifi mode
    EEPROM.write(WFSSID_START, 0);  //wifi access name
    EEPROM.commit();

    String apsid = root["AP_name"];
    String appass = root["AP_pass"];
    clearEEPROM(APSSID_START, APSSID_END);  //AP ssid
    Serial.print("WRITING  default AP ID  :: ");
    Serial.println(apsid.c_str());
    writeEEPROM(APSSID_START, apsid);

    clearEEPROM(APPWD_START, APPWD_END);  //AP ssid
    writeEEPROM(APPWD_START, appass);
    Serial.print("WRITING  default AP PWD  :: ");
    Serial.println(appass.c_str());
  }
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@  EEPROM read FOR SSID-PASSWORD- ACCESS POINT IP @@@@@@@@@@@@@@@@@@@@@@@@@@@@
  String esid = EEPROM.readString(APSSID_START);
  String ip1, ip2, ip3, ip4;
  delay(100);
  //for (int ssidaddress = APSSID_START; EEPROM.read(ssidaddress) != '\0'; ++ssidaddress) {
  //  esid += char(EEPROM.read(ssidaddress));
  //}

  Serial.print("Access point SSID:");
  Serial.println(esid);

  String epass = EEPROM.readString(APPWD_START);
  //for (int passaddress = APPWD_START; EEPROM.read(passaddress) != '\0'; ++passaddress) {
  //  epass += char(EEPROM.read(passaddress));
  //}
  Serial.print("Access point PASSWORD:");
  Serial.print(epass);
  Serial.println(" ");

  String eap = EEPROM.readString(APIP_START);

  //for (int APaddress = APIP_START; EEPROM.read(APaddress) != '\0'; ++APaddress) {
  //  eap += char(EEPROM.read(APaddress));
  //}
  Serial.print("Access point ADDRESS: ");
  Serial.print(eap);
  Serial.println(" ");

  ipAdress(eap, ip1, ip2, ip3, ip4);

  IPAddress Ip(ip1.toInt(), ip2.toInt(), ip3.toInt(), ip4.toInt());
  IPAddress NMask(255, 255, 255, 0);

  WiFi.softAPConfig(Ip, Ip, NMask);
  myIP = WiFi.softAPIP().toString();

  Serial.print("#### SERVER STARTED ON THIS: ");
  Serial.print(myIP);
  Serial.println("####");


  WiFi.softAP(esid.c_str(), epass.c_str());  //Password not used
  apname = esid;
  delay(800);
  if (!MDNS.begin(esid)) {
    Serial.println("Error setting up MDNS responder!");
    delay(3000);
  } else {
    Serial.println("mDNS responder started");
  }

  if (firstTime) oneTimeRunCompleted();
  server.serveStatic("/", LittleFS, "/").setDefaultFile("main.html");

  //###################################   ACTIONS FROM WEBPAGE BUTTTONS  ##############################
  server.on("/main", HTTP_GET, onMain);
  server.on("/scan_wifi", HTTP_GET, onScanWiFi);
  server.on("/applyBtnFunction", HTTP_GET, onApply);
  server.on("/applySysBtnFunction", HTTP_GET, onApplySystem);
  server.on("/connectBtnFunction", HTTP_GET, onConnectBtn);
  server.on("/rebootbtnfunction", HTTP_GET, onReboot);
  server.on("/adminpasswordfunction", HTTP_GET, onAdminPassword);
  server.on("/resetbtnfunction", HTTP_GET, onReset);
  server.on("/applyServiceFunction", HTTP_GET, onApplyService);

  // for (int wsidaddress = WFSSID_START; EEPROM.read(wsidaddress) != '\0'; ++wsidaddress) {
  //   wsid += char(EEPROM.read(wsidaddress));
  // }
  wsid = EEPROM.readString(WFSSID_START);
  Serial.println("");
  Serial.println("");
  Serial.print("Wifi SSID: ");
  Serial.println(wsid);

  // for (int passaddress = WFPWD_START; EEPROM.read(passaddress) != '\0'; ++passaddress) {
  //   wpass += char(EEPROM.read(passaddress));
  // }
  wpass = EEPROM.readString(WFPWD_START);
  Serial.print("Wifi Password: ");
  Serial.println(wpass);

  String W_mode = EEPROM.readString(WFMOD_START);
  // for (int modeaddress = WFMOD_START; EEPROM.read(modeaddress) != '\0'; ++modeaddress) {
  //   W_mode += char(EEPROM.read(modeaddress));
  // }
  Serial.print("WI-FI_MODE: ");
  Serial.println(W_mode);

  if (wsid == NULL) {
    wsid = "Not Given";
    LOCAL_IP = "network not set";
  }

  //####################################### MODE CHECKING(DHCP-STATIC) AND WIFI BEGIN #######################################

  if (W_mode == "dhcp") {
    wifi_tools.begin(wsid.c_str(), wpass.c_str());
    delay(2000);
    if (testWifi()) {
      Serial.print("YOU ARE CONNECTED:");
      Serial.println(WiFi.status());
      LOCAL_IP = WiFi.localIP().toString();
      Serial.println(LOCAL_IP);
    }
  }


  if (W_mode == "static") {
    // for (int passaddress = WFIP_START; passaddress < WFIP_END; ++passaddress) {
    //   static_ip += char(EEPROM.read(passaddress));
    // }
    static_ip = EEPROM.readString(WFIP_START);
    Serial.print("W-static_ip: ");
    Serial.println(static_ip);

    ipAdress(static_ip, ip1, ip2, ip3, ip4);
    String sb1, sb2, sb3, sb4;
    String sub_net = EEPROM.readString(WFNM_START);
    // for (int passaddress = WFNM_START; passaddress < WFNM_END; ++passaddress) {
    //   sub_net += char(EEPROM.read(passaddress));
    // }

    Serial.print("sub_net-: ");
    Serial.println(sub_net);
    delay(1000);

    ipAdress(sub_net, sb1, sb2, sb3, sb4);

    String g1, g2, g3, g4;
    String G_add = EEPROM.readString(WFGW_START);
    // for (int passaddress = WFGW_START; passaddress < WFGW_END; ++passaddress) {
    //   G_add += char(EEPROM.read(passaddress));
    // }
    Serial.print("G_add-: ");
    Serial.println(G_add);
    ipAdress(G_add, g1, g2, g3, g4);

    String p1, p2, p3, p4;
    String P_dns = EEPROM.readString(WFPD_START);
    // for (int passaddress = WFPD_START; passaddress < WFPD_END; ++passaddress) {
    //   P_dns += char(EEPROM.read(passaddress));
    // }
    Serial.print("Primary_dns-: ");
    Serial.println(P_dns);
    ipAdress(P_dns, p1, p2, p3, p4);

    String s1, s2, s3, s4;
    String S_dns = EEPROM.readString(WFSD_START);
    // for (int passaddress = WFSD_START; passaddress < WFSD_END; ++passaddress) {
    //   S_dns += char(EEPROM.read(passaddress));
    // }
    Serial.print("SECONDARY_dns-: ");
    Serial.println(S_dns);
    ipAdress(S_dns, s1, s2, s3, s4);

    IPAddress S_IP(ip1.toInt(), ip2.toInt(), ip3.toInt(), ip4.toInt());
    IPAddress gateway(g1.toInt(), g2.toInt(), g3.toInt(), g4.toInt());
    IPAddress subnet(sb1.toInt(), sb2.toInt(), sb3.toInt(), sb4.toInt());
    IPAddress primaryDNS(p1.toInt(), p2.toInt(), p3.toInt(), p4.toInt());    //optional
    IPAddress secondaryDNS(s1.toInt(), s2.toInt(), s3.toInt(), s4.toInt());  //optional

    if (!WiFi.config(S_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
      Serial.println("STA Failed to configure");
    }

    WiFi.begin(wsid.c_str(), wpass.c_str());
    delay(1000);

    if (testWifi()) {
      Serial.print(WiFi.status());
      Serial.println("YOU ARE CONNECTED");
      LOCAL_IP = WiFi.localIP().toString();
      Serial.println(LOCAL_IP);
    }
  }

  numLeds = numLeds_per_box * boxes;
  leds = new (std::nothrow) CRGB[numLeds];
  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, numLeds);
  sectionManager = new SectionManager(leds);
  // tell the sectionManager the number sections your light has
  sectionManager->addSections(boxes);
  uint16_t start = 0;
  uint16_t end = 0;
  for (uint16_t i = 0; i < boxes; i++) {
    start = i * numLeds_per_box;
    end = start + numLeds_per_box - 1;
    sectionManager->addRangeToSection(i, start, end);
    Serial.print("start:");
    Serial.print(start);
    Serial.print(", end:");
    Serial.println(end);
  }
  sectionManager->clearAllSections();
  readServiceFile();
  ElegantOTA.begin(&server);  // Start ElegantOTA
  // ElegantOTA callbacks
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);
  server.begin();
}

void loop() {

  // for (uint8_t i = 0; i < boxes; i++) {
  //   // put your main code here, to run repeatedly:
  //   // Turn the LED on, then pause
  //   sectionManager->fillSectionWithColor(i, 0xFF0000, FillStyle(ALL_AT_ONCE));
  //   //FastLED.show();
  //   delay(500);
  //   // Now turn the LED off, then pause
  //   sectionManager->fillSectionWithColor(i, 0x000000, FillStyle(ALL_AT_ONCE));
  //   //FastLED.show();
  //   delay(500);

  //   // Turn the LED on, then pause
  //   sectionManager->fillSectionWithColor(i, 0x00FF00, FillStyle(ALL_AT_ONCE));
  //   //FastLED.show();
  //   delay(500);
  //   // Now turn the LED off, then pause
  //   sectionManager->fillSectionWithColor(i, 0x000000, FillStyle(ALL_AT_ONCE));
  //   //FastLED.show();
  //   delay(500);

  //   // Turn the LED on, then pause
  //   sectionManager->fillSectionWithColor(i, 0x0000FF, FillStyle(ALL_AT_ONCE));
  //   //FastLED.show();
  //   delay(500);
  //   // Now turn the LED off, then pause
  //   sectionManager->fillSectionWithColor(i, 0x000000, FillStyle(ALL_AT_ONCE));
  //   //FastLED.show();
  //   delay(500);
  //   Serial.println("bob");
  // }
  // for (uint8_t i = 0; i < boxes; i++) {
  //   for (uint8_t j = 0; j < numLeds_per_box; j++) {
  //     leds[j + (i * numLeds_per_box)] = CRGB::Blue;
  //   }
  //   FastLED.show();
  //   delay(500);
  //   Serial.println("aadi");
  // }
  delay(500);
}


//################################# WEB PAGE LOAD  ###############################
void onMain(AsyncWebServerRequest* request) {
  if (debug) Serial.println("**main**");
  String content = "{\"myIP\":\"" + myIP + "\",\"localIP\":\"" + LOCAL_IP + "\",\"U_name\":\"" + U_name + "\",\"wsid\":\"" + wsid + "\",\"apname\":\"" + apname + "\",\"boxes\":\"" + boxes + "\",\"led_per\":\"" + numLeds_per_box + "\",\"MAC\":\"" + WiFi.macAddress() + "\",\"debug\":" + (debug ? "true" : "false") + ",\"VER\":\"" + VER + "\"}";
  if (debug) Serial.println(content);
  request->send(200, "application/json", content);
}
//################################# SCAN WiFI  ###############################
void onScanWiFi(AsyncWebServerRequest* request) {
  if (debug) Serial.println("**scan_wifi**");
  String scan_wifi = request->getParam("scan_wifi")->value();
  if (scan_wifi) {
    if (debug) Serial.println("scan_wifi");

    String json = "[";
    int n = WiFi.scanNetworks();
    if (n == 0) {
      if (debug) Serial.println("no networks found");
    } else {
      if (debug) Serial.print(n);
      if (debug) Serial.println(" networks found");
      for (int i = 0; i < n; ++i) {
        // Print SSID and RSSI for each network found
        if (i)
          json += ", ";
        json += " {";
        json += "\"rssi\":" + String(WiFi.RSSI(i));
        json += ",\"ssid\":\"" + WiFi.SSID(i) + "\"";
        json += ",\"bssid\":\"" + WiFi.BSSIDstr(i) + "\"";
        json += ",\"channel\":" + String(WiFi.channel(i));
        json += ",\"secure\":" + String(WiFi.encryptionType(i));
        //                json += ",\"hidden\":"+String(WiFi.isHidden(i)?"true":"false");
        json += "}";
        if (i == (n - 1)) {
          json += "]";
        }
      }
      delay(10);
      if (debug) Serial.println(json);
      request->send(200, "application/json", json);
    }
  }
}
//################################# AP SSID-PASSWORD-IP RECEIVING FROM WEB PAGE WRITING TO EEPROM  ###############################
void onApply(AsyncWebServerRequest* request) {
  if (debug) Serial.println("**applyBtnFunction**");
  String txtssid = request->getParam("txtssid")->value();
  String txtpass = request->getParam("txtpass")->value();
  String txtaplan = request->getParam("txtaplan")->value();

  if (txtssid.length() > 0) {
    Serial.print("WRITING AP SSID :: ");
    Serial.println(txtssid.c_str());
    writeEEPROM(APSSID_START, txtssid);
  }
  if (txtpass.length() > 0) {
    Serial.print("AP PASSWORD WRITE:: ");
    Serial.println(txtpass.c_str());
    writeEEPROM(APPWD_START, txtpass);
  }
  if (txtaplan.length() > 0) {
    Serial.print("access point IP  WRITE:: ");
    Serial.println(txtaplan.c_str());
    writeEEPROM(APIP_START, txtaplan);
  }

  request->send(200, "text/plain", "ok");
}
//################################# SYSTEM CONFIG RECEIVING FROM WEB PAGE WRITING TO FILE  ###############################
void onApplySystem(AsyncWebServerRequest* request) {
  if (debug) Serial.println("**applySysBtnFunction**");
  String parameters = request->getParam("parameters")->value();
  if (debug) Serial.println(parameters);

  File f = LittleFS.open(sys_data_file, "w");

  if (!f) {
    Serial.println("system data file open failed");
  } else {
    Serial.println("system data File Writing");
    f.print(parameters);
    f.close();  //Close file
    Serial.println("File closed");
  }

  readSysFile();
  request->send(200, "text/plain", "ok");
}
//#####################################  Receving WIFI credential from WEB Page ############################
void onConnectBtn(AsyncWebServerRequest* request) {
  if (debug) Serial.println("**connectBtnFunction**");
  String wifi_ssid = request->getParam("wifi_ssid")->value();
  String wifi_pass = request->getParam("wifi_pass")->value();
  String wifi_MODE = request->getParam("wifi_MODE")->value();
  Serial.println(wifi_MODE);

  if (wifi_ssid.length() > 0) {
    for (int i = 66; i < 87; ++i) {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();
    Serial.print("RE-writting wifi SSID: ");
    for (int j = 0; j < wifi_ssid.length(); ++j) {
      EEPROM.write(66 + j, wifi_ssid[j]);
      Serial.print("WRITING wifi SSID :: ");
      Serial.println(wifi_ssid[j]);
    }
    EEPROM.commit();
  }
  if (wifi_pass.length() > 0) {

    for (int i = 88; i < 103; ++i) {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();

    for (int i = 0; i < wifi_pass.length(); ++i) {
      EEPROM.write(88 + i, wifi_pass[i]);
      Serial.print("PASS WRITE:: ");
      Serial.println(wifi_pass[i]);
    }
    EEPROM.commit();
  }

  //##########################   Writing WIFI settings to EEPROM ###############################################
  if (wifi_MODE.length() > 0) {
    clearEEPROM(WFMOD_START, WFMOD_END);
    writeEEPROM(WFMOD_START, wifi_MODE);
  }

  if (wifi_MODE == "static") {
    Serial.println(" Mode STATIC selected ");

    String txtipadd = request->getParam("txtipadd")->value();
    String net_m = request->getParam("net_m")->value();
    String G_add = request->getParam("G_add")->value();
    String P_dns = request->getParam("P_dns")->value();
    String S_dns = request->getParam("S_dns")->value();

    Serial.println(wifi_ssid);
    Serial.println(wifi_pass);
    Serial.println(txtipadd);
    Serial.println(net_m);
    Serial.println(G_add);
    Serial.println(P_dns);
    Serial.println(S_dns);

    if (txtipadd.length() > 0) {
      clearEEPROM(WFIP_START, WFIP_END);
      Serial.println("PRIMARY SSID writing:: ");
      writeEEPROM(WFIP_START, txtipadd);
    }

    if (net_m.length() > 0) {
      clearEEPROM(WFNM_START, WFNM_END);
      Serial.println("PRIMARY NETMASK writing:: ");
      writeEEPROM(WFNM_START, net_m);
    }

    if (G_add.length() > 0) {
      clearEEPROM(WFGW_START, WFGW_END);
      Serial.println("PRIMARY Gateway writing:: ");
      writeEEPROM(WFGW_START, G_add);
    }
    if (P_dns.length() > 0) {
      clearEEPROM(WFPD_START, WFPD_END);
      Serial.println("PRIMARY DNS writing:: ");
      writeEEPROM(WFPD_START, P_dns);
    }

    if (S_dns.length() > 0) {
      clearEEPROM(WFSD_START, WFSD_END);
      Serial.println("SECONDAY DNS writing:: ");
      writeEEPROM(WFSD_START, S_dns);
    }
  }
  request->send(200, "text/plain", "ok");
}
//###############################  RESTARTING DEVICE ON REBOOT BUTTON ####################################
void onReboot(AsyncWebServerRequest* request) {
  if (debug) Serial.println("**rebootbtnfunction**");
  if (request->getParam("reboot_btn")->value() == "reboot_device") {
    if (debug) Serial.print("restarting device");
    request->send(200, "text/plain", "ok");
    delay(5000);
    ESP.restart();
  }
}
//######################################    RESET to Default  ############################################
void onReset(AsyncWebServerRequest* request) {
  if (debug) Serial.println("**resetbtnfunction**");
  if (request->getParam("reset_btn")->value() == "reset_device") {
    for (int i = 66; i < 103; ++i) {
      EEPROM.write(i, 0);  // wsid- wpass eeprom erase
    }
    for (int i = 116; i < 220; ++i) {
      EEPROM.write(i, 0);  // wifi mode and parameters are cleared
    }
    EEPROM.commit();

    JSONVar root = JSON.parse(master_config);
    if (JSON.typeof(root) == "undefined") {
      Serial.println("Parsing input failed!");
      Serial.println(master_config);
      return;
    }

    String apsid = root["AP_name"];
    String appass = root["AP_pass"];
    String apip = root["AP_IP"];

    clearEEPROM(APSSID_START, APSSID_END);  //AP ssid
    if (debug) Serial.print("WRITING  default AP ID  :: ");
    if (debug) Serial.println(apsid.c_str());
    writeEEPROM(APSSID_START, apsid);

    clearEEPROM(APPWD_START, APPWD_END);  //AP ssid
    if (debug) Serial.print("WRITING  default AP PWD  :: ");
    if (debug) Serial.println(appass.c_str());
    writeEEPROM(APPWD_START, appass);

    clearEEPROM(APIP_START, APIP_END);  //AP ssid
    if (debug) Serial.print("WRITING  default AP IP  :: ");
    if (debug) Serial.println(apip.c_str());
    writeEEPROM(APIP_START, apip);
  }
  request->send(200, "text/plain", "ok");
}

//############################### RECEIVING DATA SEND METHODS HTTP-MQTT-TCP ##############################
void onApplyService(AsyncWebServerRequest* request) {
  if (debug) Serial.println("**onApplyService**");
  String parameters = request->getParam("parameters")->value();
  if (debug) Serial.println(parameters);

  File f = LittleFS.open(s_data_file, "w");

  if (!f) {
    Serial.println("file open failed");
  } else {
    Serial.println("File Writing");
    f.print(parameters);
    f.close();  //Close file
    Serial.println("File closed");
  }

  readServiceFile();
  request->send(200, "text/plain", "ok");
}

//################################   ADMIN password change function   ######################################
void onAdminPassword(AsyncWebServerRequest* request) {
  Serial.println("**adminpasswordfunction**");
  String confirmpassword = request->getParam("confirmpassword")->value();
  Serial.print(confirmpassword);
  //TODO: save the changes
  request->send_P(200, "text/plain", "OK");
}
void readServiceFile() {
  std::string servicefile = ReadFileToString(s_data_file);

  JSONVar root = JSON.parse(servicefile.c_str());

   if (JSON.typeof(root) == "undefined") {
    Serial.println("Parsing input failed!");
    Serial.println(servicefile.c_str());
    return;
  }

  JSONVar colors = root["colors"];
  if (JSON.typeof(colors) == "undefined") {
    Serial.println("Parsing colors input failed!");
    return;
  }
  for (int i = 0; i < colors.length(); i++) {
    if (i < boxes) {
      sectionManager->fillSectionWithColor(i, (uint32_t)colors[i], FillStyle(ALL_AT_ONCE));
    }
  }
  if (debug) {
    Serial.println(JSON.typeof(colors));
  }
}

//################################  Clear EEPROM function   ######################################
void clearEEPROM(int start, int end) {
  for (int i = start; i < end; ++i) {
    EEPROM.write(i, 0);  // wifi mode and parameters are cleared
  }
  EEPROM.commit();
}
//################################   write EEPROM function   ######################################
void writeEEPROM(int start, const String& param) {
  EEPROM.writeString(start, param);
  EEPROM.commit();
}

//################################  Read file to string  #####################################
std::string ReadFileToString(const char* filename) {
  if (debug) {
    Serial.print("Reading file:");
    Serial.println(filename);
  }
  auto file = LittleFS.open(filename, "r");
  size_t filesize = file.size();
  // Read into temporary Arduino String
  String data = file.readString();
  // Don't forget to clean up!
  file.close();
  if (debug) Serial.println(data.c_str());
  return std::string(data.c_str(), data.length());
}
//################################  Read system file  #####################################
void readSysFile() {
  std::string sysfile = ReadFileToString(sys_data_file);

  JSONVar root = JSON.parse(sysfile.c_str());

  if (JSON.typeof(root) == "undefined") {
    Serial.println("Parsing input failed!");
    Serial.println(sysfile.c_str());
    return;
  }

  debug = root["debug"];
  boxes = root["boxes"];
  numLeds_per_box = root["led_per"];

  if (debug) {
    Serial.print(" debug:");
    Serial.println(debug);
  }
}



void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}
