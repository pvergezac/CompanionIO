#include <Arduino.h>

const String TITRE = "MSunPV CompanionIO";
const String TITRE_LONG = "CompanionIO (Nouvelle Génération)";
const String Version = "1.01";

//*************************************************
//                @pzac                          **
//                @jjhontebeyrie                 **
/**************************************************
**        Companion d'affichage déporté          **
**         pour routeur solaire MSunPV           **
**    fonctionnant sur LILYGO T-Display S3       **   
***************************************************
**           Repository du Companion_NG          **
**  https://github.com/pvergezac/Companion_ng    **
**                                               **
***************************************************/
#include <esp_wifi.h>
#include <WiFi.h>

//+++++++++++++++++++++++++++++++
#define USE_LITTLEFS    true
#define USE_SPIFFS      false
#include "FS.h"
#include <LittleFS.h>                       // https://github.com/espressif/arduino-esp32/tree/master/libraries/LittleFS

FS* filesystem =      &LittleFS;
#define FileFS        LittleFS
#define FS_Name       "LittleFS"

#define DOUBLERESETDETECTOR_DEBUG   true
#define ESP_DRD_USE_LITTLEFS        true
#define ESP_DRD_USE_SPIFFS          false
#define ESP_DRD_USE_EEPROM          false
#include <ESP_DoubleResetDetector.h>        //https://github.com/khoih-prog/ESP_DoubleResetDetector
#define DRD_TIMEOUT                 10      // Time in seconds after reset during which a subseqent reset will be considered a double reset.
#define DRD_ADDRESS                 0       // RTC Memory Address for the DoubleResetDetector to use
DoubleResetDetector* drd;
bool initialConfig = false;

#include <WiFiMulti.h>
WiFiMulti wifiMulti;

#define NUM_WIFI_CREDENTIALS        1
#define USE_AVAILABLE_PAGES         true
#define USE_STATIC_IP_CONFIG_IN_CP  false
#define USING_CORS_FEATURE          false

#define USING_AFRICA                false
#define USING_AMERICA               false
#define USING_ANTARCTICA            false
#define USING_ASIA                  false
#define USING_ATLANTIC              false
#define USING_AUSTRALIA             false
#define USING_EUROPE                true
#define USING_INDIAN                false
#define USING_PACIFIC               false
#define USING_ETC_GMT               false

//+++++++++++++++++++++++++++++++
#include <ESPAsync_WiFiManager.h>
// Correctif pour conflit de redefinition de HTTP_**** entre WiFiManager et ESPAsyncWebServer
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>

#include <ESPmDNS.h>                // Pour le mDNS
#include "NTP_Time.h"               // Time Synchronisation
#include <RemoteDebug.h>            // Debug via Wifi
#include <esp_task_wdt.h>           // watchdog en cas de déconnexion ou blocage
#include <TFT_eSPI.h>               // Gestion Ecran
#include <OneButton.h>              // OneButton par Matthias Hertel (dans le gestionnaire de bibliothèque)
#include <ArduinoJson.h>            // JSon
#include <OpenWeather.h>            // https://github.com/Bodmer/OpenWeather
#include <InfluxDbClient.h>         // InfluxDB
#include <InfluxDbCloud.h>          // InfluxDB  pour le certificat
#include <PubSubClient.h>           // gestion MQTT

// Additional functions
#include "GfxUi.h"                  // Attached to this sketch
#include "StreamUtils.h"
#include "time.h"

//+++++++++++++++++++++++++++++++
#include "logo.h"                   // Logo de départ
#include "images.h"                 // Images affichées sur l'écran
#include "meteo.h"                  // Icones météo

#define LCD_WIDTH  320
#define LCD_HEIGHT 170
#define LCD_CENTER_X (LCD_WIDTH /2)
#define LCD_CENTER_Y (LCD_HEIGHT /2)

#define AA_FONT_SMALL "fonts/NSBold15"            // 15 point Noto sans serif bold
#define AA_FONT_LARGE "fonts/NSBold36"            // 36 point Noto sans serif bold
#define AA_FONT_SMALL2 "fonts/NotoSansBold15"
#define AA_FONT_LARGE2 "fonts/NotoSansBold36"

// Couleurs bargraph
#define color0  0x10A2          // Sombre
#define color00 0x7FB8          // Vert clair
#define color1  0x07E0          // Vert
#define color2  0x26FB          // Bleu
#define color3  0xF780          // Jaune
#define color4  0xFD8C          // Orange
#define color45 0xFC10          // Rouge leger
#define color5  0xF9A6          // Rouge
#define color6  0xE6D8          //
#define color7  0xEF5D          //
#define color8  0x16DA          //

// Couleurs de fond Tempo
#define FOND_BLEU TFT_NAVY      // auparavant 0x0017
#define FOND_BLANC 0xE71C
#define FOND_ROUGE 0xB800

// Rayon d'arrondi des rectangles
#define RECT_RADIUS  4

#define topbutton    0
#define lowerbutton  14
#define PIN_POWER_ON 15         // LCD and battery Power Enable
#define PIN_LCD_BL   38         // BackLight enable pin (see Dimming.txt)

#define PWM_CHANNEL  0          // Canal PWM pour luminosité écran
#define PWM_FREQ     5000       // 5Khz
#define PWM_RESOL    8          // 8 bits (0 - 255)

//#define WDT_TIMEOUT 10        // Délai d'activation du watchdog en secondes
#define WDT_TIMEOUT 300         // Délai d'activation du watchdog en secondes

const String Months[13] = { "Mois", "Jan", "Fev", "Mars", "Avril", "Mai", "Juin", "Juill", "Aout", "Sept", "Oct", "Nov", "Dec" };

//+++++++++++++++++++++++++++++++++++
// Variables pour dimmer PWM de luminosité d'affichage
int backlight[6] = {0, 10, 30, 60, 120, 255}; // Tableau des ratios PWM des niveaux de luminosité écran
int backligthLevel = 3;                       // Luminosité ecran

#define TEMPS_VEILLE (5* 60 * 1000)           // Veille après 60s
bool veille_on = false;                       // Veille encour
ulong veille_deb;                             // Temps de debut de mise en veille

//+++++++++++++++++++++++++++++++++++
// Update toutes les 15 minutes, jusqu'à 1000 requêtes par jour gratuit (soit ~40 par heure)
#define UPDATE_TIME_INTERVAL   (1*1000)       // 1 secondes
#define UPDATE_MSUNPV_INTERVAL (15*1000)      // 15 secondes
#define UPDATE_METEO_INTERVAL  (15*60*1000)   // 15 minutes    (1000 requêtes/jour gratuit)
#define UPDATE_METEO_SOLAIRE_INTERVAL  (15*60*1000)   // 15 minutes    (1000 requêtes/jour gratuit)

ulong uptime = 0;                             // Heure de demarrage (reset)

#define TIMEZONE euCET              // Voir NTP_Time.h tab pour d'autres "Zone references", UK, usMT etc

//+++++++++++++++++++++++++++++++++++
// Gestion des pages écran
#define PAGE_DEFAULT  0
#define PAGE_SOLAR    1
#define PAGE_TEMPO    2
#define PAGE_CUMULS   3
#define PAGE_LUMI     4
#define PAGE_METEO    5
#define PAGE_METEO2   6
#define PAGE_LAST     4
#define DUREE_AFF     (5* 60 * 1000)          // Durée affichage page, avant retour à la page par défaut

int page = PAGE_DEFAULT;                      // Page courante affichée
ulong tempsAffPage = 0;                       // Temps d'affichage page, avant retour à la page par défaut

bool updateAff = false;
bool firstAff = true;

//+++++++++++++++++++++++++++++++++++
ESPAsync_WiFiManager *wm;
AsyncWebServer webserver(80);                 //  Serveur web on port 80
RemoteDebug Debug;                            // Debug via WIFI instead of Serial, Connect a Telnet terminal on port 23

//+++++++++++++++++++++++++++++++++++
const char* lumiConfigFileName = "/lumi.json";
const char* configFileName = "/config.json";
bool shouldSaveConfig = false;

//+++++++++++++++++++++++++++++++++++
OneButton boutonBacklight(topbutton, true);   // Bouton éclairage
OneButton boutonPage(lowerbutton, true);      // Bouton de selection de page
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);       // Tout l'écran
const int rotation = 3;                       // Orientation affichage - (3 pour prise à gauche, 1 pour prise à droite)

//+++++++++++++++++++++++++++++++++++
// MSunPV
String msunpv_server;                         // Adresse du routeur MSunPV

// Seuil minimum d'affichage des mesures PV et conso (en W)
const int residuel = 10;

int pCaractPV = 3000;           // puissance Crete installé des panneaux solaire en watt
int pCaractBallon = 2500;       // puissance du ballon cumulus en watt

bool withBatt = false;          // Alimentation par batterie
bool withRad = false;           // Radiateurs électriques
bool withTBal = false;          // Sonde Température installé sur le cumulus
bool withVeille = true;         // Mise en veille affichage

// Variables affichant les valeurs reçues depuis le MSunPV
int powpv = 0;            // Puissance panneaux (W)
int powreso = 0;          // Puissance consomé/injectée (W)
int outbal = 0;           // Dimmer ballon (0 - 100%)
int outrad = 0;           // Dimmer radiateur
int powbal = 0;           // Puissance injection ballon, recalculée a partir de dimmer_ballon
int voltres = 0;          // Tension réseau (V)

float tbal = 0.;          // Température ballon (°C)
float tsdb = 0.;          // Température Salle de bain (°C)
float tamb = 0.;          // Température ambiante (°C)

float cumulConso = 0.;    // Cumul Consomation
float cumulInj = 0.;      // Cumul Injection
float cumulPV_J = 0.;     // Cumul Panneaux
float cumulPV_P = 0.;     // Cumul Ballon cumulus

String msunpv_time;
String msunpv_date;
bool msunpv_enrgsd;
int msunpv_surv[16];
int msunpv_cmd[8];
int msunpv_out[16];
int msunpv_chout[8];

bool cmdManuBal, cmdAutoBal, cmdManuRad, cmdAutoRad;
bool cmdInject, cmdZero, cmdMoyen, cmdFort;


float vBatt;              // Voltage batterie

//+++++++++++++++++++++++++++++++++++
// Données de Openweather
OW_Weather ow;                      // Weather forecast librairie instance
String ow_latitude;                 // 90.0000 to -90.0000 negative for Southern hemisphere
String ow_longitude;                // 180.000 to -180.000 negative for West
String ow_api_key;

String lever = "", coucher = "", icone = "", ID = "";
float tempExt = 0.;

struct Prevision {
  String  dt;           // Heure de la prévi

  float   temp;         // Température
  float   feels_like;   // Température resentie
  float   temp_min;     // Température min.
  float   temp_max;     // Température max.
  float   pressure;     // Pression (hPa)
  uint    humidity;     // Humidité (%)

  uint    clouds;       // Nébulosité (%)
  //float uvi;
  float   visibility;   // Visibilité moyenne (km)
  float   rain;         // Pluie (mm ou l/m2)
  uint    prob;         // Probability of precipitation (%)
  float   snow;         // Neige (mm)

  float   wind_speed;   // Vent (m/s)
  float   wind_gust;    // Rafale (m/s)
  uint    wind_deg;     // Direction vent (°)

  String  main;         // Main weather condition
  String  description;  // Weather condition description
  String  icon;         // Weather icon id
  String  ID;           // Weather condition id
};

struct MeteoData {
  // Meteo courante
  String  dt;           // Heure de la météo (UTC)

  float   temp;         // Température
  float   feels_like;   // Température resentie
  float   temp_min;     // Température min.
  float   temp_max;     // Température max.
  float   pressure;     // Pression (hPa)
  uint    humidity;     // Humidité (%)
  //float   dew_point;    // Température du point de rosé

  uint    clouds;       // Nébulosité (%)
  //float   uvi;
  float   visibility;   // Visibilité moyenne (km)

  float   rain;         // Pluie sur 1h (mm ou l/m2)
  float   snow;         // Neige sur 1h (mm)

  float   wind_speed;   // Vent (m/s)
  float   wind_gust;    // Rafale (m/s)
  uint    wind_deg;     // Direction vent (°)

  String  wmain;         // Main weather condition
  String  description;  // Weather condition description
  String  icon;         // Weather icon id
  String  ID;           // Weather condition id
  String  cityName;     // City name
  String  base;         // Station météo

  String  lever;        // Heure de levé du soleil (UTC)
  String  coucher;      // Heure de couché du soleil (UTC)

  // Prévisions
  Prevision previ[8];   // 8 prévi à 3h sur 24h
};

//MeteoData *myMeteo = new MeteoData;
MeteoData myMeteo;


//+++++++++++++++++++++++++++++++++++
// Données Météo Solaire
int forecast_watts[48];
int forecast_wattshours[48];
time_t forecast_times[48];
int forecast_size = 0;
int forecast_whday[2] = {0,0};
int ratelimit_remaining = 0;

//+++++++++++++++++++++++++++++++++++
// Données Tempo
const uint16_t tempo_colors[] = {TFT_BLACK, TFT_BLUE, TFT_WHITE, TFT_RED};
const String tempo_str[] = {"??", "Bleu", "Blanc", "Rouge"};
int codeTempoJ = 0; 
int codeTempoJ1 = 0;

bool updateTempo = false;

float tarif_bleu_hp= 0.;
float tarif_bleu_hc= 0.;
float tarif_blanc_hp= 0.;
float tarif_blanc_hc= 0.;
float tarif_rouge_hp= 0.;
float tarif_rouge_hc= 0.;

//+++++++++++++++++++++++++++++++++++
// InfluxDB
bool withInfluxDB = false;
String influxDbServerUrl;
String influxDbToken;
String influxDbOrg;
String influxDbBucket;
InfluxDBClient clientInfluxDB;

// Declare Data point
Point sensor("measurements");
  
//+++++++++++++++++++++++++++++++++++
// MQTT - Home Assistant
bool withMqtt = false;
// Broker MQTT (Home Assistant) adrs et login / password
String mqtt_server;
String mqtt_user;
String mqtt_pwd;

WiFiClient mqttWifiClient;
PubSubClient mqttClient(mqttWifiClient);
int mqttCounterConn = 0;                      // connections counter

const char* topicHAStatus = "homeassistant/status";
const String base_topic = "homeassistant/sensor/";
const String uidPrefix = "msunpv-comp";       // Prefix for unique ID generation (limit to 20 chars)
String devUniqueID;                           // Generated Unique ID for this device (uidPrefix + last 6 MAC characters) 

//Auto-discover enable/disable option
bool auto_discovery = false;                  //default to false and provide end-user interface to allow toggling 

//===============================================================
// Prototypes des fonctions
void log_print(String msg);
void log_println(String msg);
void initWifi();
void handleClick();
void doubleClick();
void handlePage();
void handlePageDef();
void reqMeteo();
void reqMeteo2();
void reqtMeteoPrevi();
bool reqMSunPV(ulong ms);
void reqTempo();
void reqTempoTarifs();
void reqMeteoSolaire(ulong ms);
void doAffichage();
void AffichageDefault();
void AffichageCumuls();
void AfficheTempo();
void AfficheMeteo();
void AfficheMeteo2();
void AfficheSolar();
void AfficheLumi();
String formatHeure(time_t unixTime);
String formatDateTime(time_t time);
void barregraphV (int xx, int yy, int barres, int barresMax, int largeur, int pas_v, int pas_esp, uint32_t couleur);
void split(String values[], int dimArray, String content, char separator);
void signalwifi();
float wh_to_kwh(float wh);
void listDir(const char * dirname, uint8_t levels);
void readVbatt();
void initWebServer();
String processor(const String& var);
String readFile(const char * path);
void writeFile(const char * path, const char * message);
void check_status();
bool loadFileFSConfigFile();
bool saveFileFSConfigFile();
void loadLumiParam();
void saveLumiParam();
void sendInfluxDB();
void mqttSendMesures();
void mqttSendTempo();
void mqttReconnect();
void mqttReceiverCallback(char* topic, byte* payload, unsigned int length);
void mqttHaDiscovery();


/***************************************************************************************
**                                Routine SETUP
** 
***************************************************************************************/
void setup() {

  esp_sleep_wakeup_cause_t source_reveil;
  source_reveil = esp_sleep_get_wakeup_cause();  //0= boot, 2=interruption capteur IR, 4 = interruption timer. Valeurs différentes de la doc Espressif ?
  uptime = millis();                    // Heure de démarrage

  pinMode(lowerbutton, INPUT_PULLUP);   // Right button pulled up, push = 0
  pinMode(topbutton, INPUT_PULLUP);     // Left button  pulled up, push = 0
  pinMode(PIN_LCD_BL, OUTPUT);          // BackLight enable pin
  pinMode(PIN_POWER_ON, OUTPUT);        // Batterie power on
  if (withBatt) {
    digitalWrite(PIN_POWER_ON, HIGH);   // Activation du port batterie interne
  }

  //++++++++++++++++++++++++++++++++++++++++++++++++
  // Initialisation port série et attente ouverture
  Serial.end();
  delay(100);
  Serial.begin(115200);
  // Test si le cable USB est connecté, et un terminal actif, pour eviter les blocages
  ulong ms = millis() + 3000;           // timeout 3s, pour eviter le blocage si port serie non connecté
  while (true) {
    // If Serial OK. Use it.
    if (Serial) {                       
      Serial.setDebugOutput(true);      // Debug Wifi
      break;
    }                            

    // If Serial cable not connected. Stop Serial for not Tx enging. 
    if (millis() > ms) {
      Serial.end();                     
      break;
    }  

    delay(10);
  }

  delay(100);

  //++++++++++++++++++++++++++++++++++++++++++++++++
  Serial.printf("****************************************************************\n");
  Serial.printf("[Init] - %s - V%s\n", TITRE.c_str(), Version.c_str());
  Serial.printf("[Init] - Wake up - source_reveil= %d\n", source_reveil);

  //++++++++++++++++++++++++++++++++++++++++++++++++
  if (!LittleFS.begin(true)) {
    Serial.println("Error while mounting LittleFS\n");
    while (true) {
      delay(10);
    }
  }

  drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);
  if (drd->detectDoubleReset()) {
    initialConfig = true;
  }

  listDir("/", 0);
  Serial.printf("[LittleFS] - totalBytes: %d, , usedBytes: %d\n", LittleFS.totalBytes(), LittleFS.usedBytes());

  //++++++++++++++++++++++++++++++++++++++++++++++++
  // Initialisation ecran
  tft.init();                                 // TFT_HEIGHT=320 , TFT_WIDTH=170
  tft.setRotation(rotation);
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_WHITE);

  // Affichage logo depart
  tft.setSwapBytes(true);
  tft.pushImage(10, 20, 300, 146, image);     // Image d'acceuil Companion MSunPV
  tft.setTextColor(TFT_BLUE, TFT_WHITE);
  tft.setTextDatum(4);
  tft.drawCentreString("Version " + (Version), LCD_CENTER_X, 10, 2);

  digitalWrite(PIN_LCD_BL, HIGH);             // Allume l'éclairage ecran (luminosité 100%)

  // Creation des sprites affichage
  sprite.createSprite(320, 170);              // Tout l'ecran
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);  // Ecriture blanc sur fonf noir
  sprite.setTextDatum(4);                     // Alignement texte au centre du rectangle le contenant
  sprite.setSwapBytes(true);

  //++++++++++++++++++++++++++++++++++++++++++++++++
  // Initialisation du WIFI
  initWifi();

  //++++++++++++++++++++++++++++++++++++++++++++++++
  // Affichage de la connexion sur écran de départ
  tft.setTextColor(TFT_BLUE, TFT_WHITE);
  tft.setTextDatum(4);
  tft.drawCentreString(WiFi.localIP().toString() +"  " + String(WiFi.RSSI()) +"dB", LCD_CENTER_X, 136, 4);

  //++++++++++++++++++++++++++++++++++++++++++++++++
  // Activation du mDNS responder: encore idée de Bellule
  // Utiliser l'url : http://companion pour y accéder
  if (!MDNS.begin("companion")) {
    while (1) { 
      Serial.printf("[Init] - Error setting up MDNS responder!\n");
      delay(1000); 
    }                                // endless loop
  }

  // Ajout du service to MDNS-SD
  MDNS.addService("http", "tcp", 80);                       // Serveur Web
  MDNS.addService("telnet", "tcp", 23);                     // Remote Debug telnet
  
  Serial.printf("[Init] - mDNS responder started\n");

  //++++++++++++++++++++++++++++++++++++++++++++++++
  // Attente pour permettre de lancer la console ou Telnet pour le debug
  delay(3000);

  //++++++++++++++++++++++++++++++++++++++++++++++++
  // Init Remote Debug
  Serial.printf("****************************************************************\n");
  Serial.printf("[Init] - Starting Remote Debug\n");

  Debug.begin("companion", Debug.DEBUG);
  //Debug.setPassword("r3m0t0.");       // Password on telnet connection ?
	Debug.setResetCmdEnabled(true);       // Enable the reset command
	//Debug.showProfiler(true);           // Profiler (Good to measure times, to optimize codes)
	Debug.showColors(true);               // Colors
  if (Serial) Debug.setSerialEnabled(true);         // if you wants serial echo - only recommended if ESP is plugged in USB

  //if you get here you have connected to the WiFi
  debugI("================================================================");
  debugI("[Init] - Remote debug started");
  debugI("[Wifi] - Connected...yeey : local ip : %s", WiFi.localIP().toString().c_str());

  //++++++++++++++++++++++++++++++++++++++++++++++++
  // Initialisation du Watchdog
  debugI("[Init] - Configuraton du WDT...");

  esp_task_wdt_deinit();                                // MODIFICATION POUR VERSION ESP32 3.0.5 le watchdog est activé par défaut ; il faut d'abord le désactiver avant de le configurer
  int err_code = esp_task_wdt_init(WDT_TIMEOUT, true);  //enable panic so ESP32 restarts
  if (err_code != 0) 
    debugE("[Init] - esp_task_wdt_init error: %d", err_code);

  esp_task_wdt_add(NULL);  //add current thread to WDT watch

  //++++++++++++++++++++++++++++++++++++++++++++++++
  // Synchronisation de l'heure (NTP)
  debugI("[Init] - Synchronisation de l'heure (NTP)");
  udp.begin(localPort);
  syncTime();  // Synchronisation de l'heure systeme via NTP
  esp_task_wdt_reset();

  //++++++++++++++++++++++++++++++++++++++++++++++++
  // Attache les handler de boutons
  boutonBacklight.attachClick(handleClick);
  boutonBacklight.attachDoubleClick(doubleClick);
  boutonPage.attachClick(handlePage);
  boutonPage.attachDoubleClick(handlePageDef);

  //++++++++++++++++++++++++++++++++++++++++++++++++
  // Configuration dimmer PWM de reglage luminosité écran
  loadLumiParam();
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOL);                 // Fréquence PWM de 1 KHz, Résolution de 8 bits, 256 valeurs possibles
  ledcAttachPin(PIN_LCD_BL, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, backlight[backligthLevel]);

  //++++++++++++++++++++++++++++++++++++++++++++++++
  // Lancement du serveur web
  initWebServer();

  //++++++++++++++++++++++++++++++++++++++++++++++++
  if (withInfluxDB) {
    debugI("[Init] - Connecting to InflufDB server= %s, org= %s, bucket= %s", influxDbServerUrl.c_str(), influxDbOrg.c_str(), influxDbBucket.c_str());
    clientInfluxDB.setConnectionParams(influxDbServerUrl, influxDbOrg, influxDbBucket, influxDbToken, InfluxDbCloud2CACert);

    // Check InfluxDb connection
    if (!clientInfluxDB.validateConnection()) {
      debugE("InfluxDB connection failed: %s\n", clientInfluxDB.getLastErrorMessage().c_str());
    }

    // Add tags to the data point
    sensor.addTag("device", "ESP32");
    sensor.addTag("SSID", WiFi.SSID());
  }

  //++++++++++++++++++++++++++++++++++++++++++++++++
  // Configuration du broker MQTT (Home Assistant)
  if (withMqtt) {
    mqttClient.setBufferSize(512);
    mqttClient.setServer(mqtt_server.c_str(), 1883);
    mqttClient.setCallback(mqttReceiverCallback);

    // Id unique à partir de l'adresse MAC
    byte mac[6];
    WiFi.macAddress(mac);
    //devUniqueID = uidPrefix +String(mac[0], HEX) +String(mac[1], HEX) +String(mac[2], HEX) +String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
    devUniqueID = uidPrefix +String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);

    mqttReconnect();
  }

  //++++++++++++++++++++++++++++++++++++++++++++++++
  veille_deb = millis() + TEMPS_VEILLE;
  esp_task_wdt_reset();
  debugI("[Init] - Setup fin.");
}

/***************************************************************************************
**                                Routine LOOP 
** 
***************************************************************************************/
void loop() {
  static ulong nextWeatherUpdate = 0UL;

  //+++++++++++++++++++++++++++++++
  Debug.handle();
  
  //+++++++++++++++++++++++++++++++
  // Gestion des boutons
  boutonBacklight.tick();
  boutonPage.tick();
  drd->loop();                        // Gestion double reset

  //+++++++++++++++++++++++++++++++
  // Gestion MQTT
  if (withMqtt && (WiFi.status() == WL_CONNECTED)) {
    if (!mqttClient.connected()) {
      mqttReconnect();                  // Force la reconnexion au broker MQTT en cas de perte
    }
     
    mqttClient.loop();
  }

  //+++++++++++++++++++++++++++++++
  ulong ms = millis();

  // si le Wifi est connecté uniquement
  if (WiFi.status() == WL_CONNECTED) {        

    // Requète des données Météo (15mn)
    if (ms > nextWeatherUpdate) {
      nextWeatherUpdate = ms + UPDATE_METEO_INTERVAL;

      //reqMeteo();                   // Requète la Météo via la blbliothèque de OpenWeather
      reqMeteo2();                 // Requète la Météo sans bibliothèque

      syncTime();                   // Synchronisation de l'heure systeme via NTP
      readVbatt();                  // Lecture tension batterie

      updateAff = true;
    }

    // Requete TEMPO
    reqTempo();
    reqTempoTarifs();

    // Requete Previ Meteo Solaire
    reqMeteoSolaire(ms);

    // Requète des données MSunPV (15sec)
    bool isUpdate = reqMSunPV(ms);
    if (isUpdate) {
      updateAff = true;

      //+++++++++++++++++++++++
      // Envoi des mesures vers InfluxDB
      if (withInfluxDB)
        sendInfluxDB();

      //++++++++++++++++++++++++++++++++++++++++++
      // Gestion MQTT - Envoi des mesures en Json
      if (withMqtt){
        mqttSendMesures();

        if (updateTempo) {
          mqttSendTempo();
          updateTempo = false;
        }
      }
    }
  } 
 
  doAffichage();

  // reset watchdog uniquement si WIFI OK
  esp_task_wdt_reset();
}


void sendInfluxDB() {

  if (!clientInfluxDB.isConnected()) {
    return;
  }

  // Store measured value into point
  // Report RSSI of currently connected network
  sensor.addField("rssi", WiFi.RSSI());
  sensor.addField("runtime", (millis() - uptime) /1000);      // Secondes

  sensor.addField("puis_panneaux", powpv);                    // W
  sensor.addField("puis_conso", powreso);                     // W
  sensor.addField("dimmer_ballon", outbal);                   // %
  sensor.addField("puis_ballon", powbal);                     // W
  sensor.addField("temp_ballon", tbal);                       // °C
  
  sensor.addField("cumul_conso", cumulConso);                 // kWh
  sensor.addField("cumul_inj", cumulInj);                     // kWh
  sensor.addField("cumul_pvj", cumulPV_J);                    // kWh
  sensor.addField("cumul_pvp", cumulPV_P);                    // kWh
  
  sensor.addField("previ_whday0", forecast_whday[0]);         // W
  sensor.addField("previ_whday1", forecast_whday[1]);         // W

  sensor.addField("couleur_tempo", codeTempoJ);               // 0 - 3
  sensor.addField("couleur_tempo2", codeTempoJ1);

  sensor.addField("temp", myMeteo.temp);
  sensor.addField("hum", myMeteo.humidity);                  // %
  sensor.addField("clouds", myMeteo.clouds);                 // %
  sensor.addField("wind_speed", myMeteo.wind_speed * 3.6);   // km/h
  //sensor.addField("wind_gust", myMeteo.wind_gust * 3.6);     // km/h
  sensor.addField("wind_deg", myMeteo.wind_deg);
  sensor.addField("pressure", myMeteo.pressure);             // pa
  sensor.addField("rain", myMeteo.rain);                     // mm/h ou l/m2 pat heure
  sensor.addField("snow", myMeteo.snow);                     // mm/h ou l/m2 pat heure
  sensor.addField("visibility", myMeteo.visibility);         // km

  debugD("[InfluxDb] write - %s", sensor.toLineProtocol().c_str());

  // Write point
  if (!clientInfluxDB.writePoint(sensor)) {
    debugE("InfluxDB write failed: %s", clientInfluxDB.getLastErrorMessage().c_str());
  }

  clientInfluxDB.checkBuffer();           // Flush buffer

  // Clear fields for reusing the point. Tags will remain the same as set above.
  sensor.clearFields();
}


// Variable used for MQTT Discovery
const String mqtt_deviceName  = "CompanionIO";                          // Device name
const char* mqtt_deviceModel = "MSunPV Companion";                      // Hardware model
const char* mqtt_swVersion   = "1.0";                                   // Firmware version
const char* mqtt_manufacturer = "DIY Pzac";                             // Manufecturer name
const String mqtt_statusTopic = "esp32iotsensor/" + mqtt_deviceName;    // MQTT topic

/*****************************************************************************************
**                    Reconnexion au broker MQTT en cas de perte                        **
*****************************************************************************************/
void mqttSendMesures() {
  const String topic = base_topic + devUniqueID + "/state";

  if (withMqtt && mqttClient.connected()) {
    JsonDocument doc;
    String buff;

    doc["powpv"]= String(powpv);
    doc["powreso"]= String(powreso);
    doc["outbal"]= String(outbal);
    doc["tbal"]= String(tbal, 2);
    doc["enconso"]= String(cumulConso);                   // Conso reso net (import) en kWh (positif)
    doc["eninj"]= String(0-cumulInj);                     // Export en kWh (positif)
    doc["enpvj"]= String(0-cumulPV_J);                    // Prod PV jour en kWh (positif)
    doc["enpvp"]= String(0-cumulPV_P);                    // Prod PV totale en kWh (positif)
    doc["enconsototale"]= String(cumulConso -cumulPV_J);  // Conso totale (PV + Reso) en kWh (positif)

    serializeJson(doc, buff);

    debugD("[MQTT] - send : %s", buff.c_str());

    mqttClient.publish(topic.c_str(), buff.c_str());
  }
}


void mqttSendTempo() {
  const String topic = base_topic + devUniqueID + "/state";

  if (withMqtt && mqttClient.connected()) {
    JsonDocument doc;
    String buff;

    doc["tarif_bleu_hp"]= String(tarif_bleu_hp, 3);       // Tarif Tempo
    doc["tarif_bleu_hc"]= String(tarif_bleu_hc, 3);       // Tarif Tempo
    doc["tarif_blanc_hp"]= String(tarif_blanc_hp, 3);     // Tarif Tempo
    doc["tarif_blanc_hp"]= String(tarif_blanc_hc, 3);     // Tarif Tempo
    doc["tarif_rouge_hp"]= String(tarif_rouge_hp, 3);     // Tarif Tempo
    doc["tarif_rouge_hp"]= String(tarif_rouge_hc, 3);     // Tarif Tempo

    time_t local_time = TIMEZONE.toLocal(now(), &tz1_Code);       // Convert UTC to local time, returns zone code in tz1_Code, e.g "GMT"
    int hh = hour(local_time);
    bool is_hc = ((hh >= 22) || (hh < 6));
    switch (codeTempoJ) {
        case 1:
          doc["tempo_tarif_current"]= (is_hc ? tarif_bleu_hc : tarif_bleu_hp);
        break;
        case 2:
          doc["tempo_tarif_current"]= (is_hc ? tarif_blanc_hc : tarif_blanc_hp);
        break;
        case 3:
          doc["tempo_tarif_current"]= (is_hc ? tarif_rouge_hc : tarif_rouge_hp);
        break;
        default:
          doc["tempo_tarif_current"]= "?";
        break;
    }

    doc["tempo_j"]= codeTempoJ;
    doc["tempo_j1"]= codeTempoJ1;
    doc["tempo_hc"]= is_hc ? 1 : 0;

    serializeJson(doc, buff);

    debugD("[MQTT] - send : %s", buff.c_str());

    mqttClient.publish(topic.c_str(), buff.c_str());
  }
}
 

/*****************************************************************************************
**                    Reconnexion au broker MQTT en cas de perte                        **
*****************************************************************************************/
void mqttReconnect() {
  static ulong nextReconnect = 0l;

  if (withMqtt && !mqttClient.connected()) {

    ulong ms = millis();

    if (ms > nextReconnect) {
      mqttCounterConn++;
      if (mqttCounterConn > 5 ) {
        nextReconnect = ms + 60*1000l;
      }
      else {
        nextReconnect = ms + 5000l;
      }

      debugI("[MQTT] - Connection au broker !");

      if (mqttClient.connect(mqtt_deviceName.c_str(), mqtt_user.c_str(), mqtt_pwd.c_str())) {
        debugD("[MQTT] - Reconnecté au broker !");
        delay(100);

        mqttHaDiscovery();     // Send Discovery Data

        mqttClient.subscribe(topicHAStatus);
        delay(100);
        mqttCounterConn=0;
      }
      else {
        debugE("[MQTT] - Broker MQTT indisponible. Status= %d", mqttClient.state());
      }
    }
  }
}

void discovSensor(const char* name, const char* devClass, const char* unit, const char* icon = nullptr, bool cmd_t = false);

void discovSensor(const char* name, const char* devClass, const char* unit, const char* icon, bool cmd_t) {
  JsonDocument payload;
  String buf;
  String nameStr = String(name);
  String id = devUniqueID + String("_") + nameStr;
  const String bt = "homeassistant/sensor/";
  String topic = bt + id + "/config";
    
  payload["name"] = "MSunPV-" + nameStr;
  payload["uniq_id"] = id;
  payload["state_topic"] = bt + devUniqueID + "/state";
  if (cmd_t) payload["command_topic"] = bt + id + "/set";
  if (devClass) payload["dev_cla"] = devClass;
  if (icon) payload["icon"] = icon;
  if (unit) payload["unit_of_meas"] = unit;

  payload["val_tpl"] = "{{ value_json." +nameStr +" | is_defined }}";

  //JsonObject device = payload.createNestedObject("device");
  JsonObject device = payload["device"].to<JsonObject>();
  device["name"] = mqtt_deviceName;
  //JsonArray identifiers = device.createNestedArray("identifiers");
  JsonArray identifiers = device["identifiers"].to<JsonArray>();
  identifiers.add(devUniqueID);

  serializeJson(payload, buf);
  debugD("HA dh - topic= %s", topic.c_str());
  debugD("HA dh - payload= %s", buf.c_str());
  mqttClient.publish(topic.c_str(), buf.c_str());

}

void discovBinSensor(String name, String devClass, String unit, String icon, bool cmd_t) {
  JsonDocument payload;
  String buf;
  String id = devUniqueID + "_" + name;
  String topic = base_topic + devUniqueID + "/config";
  String st = base_topic + devUniqueID + "/state";
    
  payload["name"] = "MSunPV-" +name;
  payload["uniq_id"] = id;
  payload["state_topic"] = st;
  if (cmd_t) payload["command_topic"] = base_topic + devUniqueID + "/set";
  if (devClass) payload["dev_cla"] = devClass;
  if (icone) payload["icon"] = icon;
  if (unit) payload["unit_of_meas"] = unit;

  payload["val_tpl"] = "{{ value_json." +name +" | is_defined }}";

  //JsonObject device = payload.createNestedObject("device");
  JsonObject device = payload["device"].to<JsonObject>();
  device[name] = mqtt_deviceName;
  //JsonArray identifiers = device.createNestedArray("identifiers");
  JsonArray identifiers = device["identifiers"].to<JsonArray>();
  identifiers.add(devUniqueID);

  serializeJson(payload, buf);
  debugD("HA dh - topic= %s", topic.c_str());
  debugD("HA dh - payload= %s", buf.c_str());
  mqttClient.publish(topic.c_str(), buf.c_str());

}


/*****************************************************************************************
**                      MQTT Home Assistant Discovery                                   **
*****************************************************************************************/
void mqttHaDiscovery() {
  if (withMqtt && mqttClient.connected())
  {
    debugI("[MQTT] - Send Discovery !!!");

    discovSensor("powpv", "power", "W");
    discovSensor("powreso", "power", "W");
    discovSensor("outbal", nullptr, "%");
    discovSensor("outrad", nullptr, "%");

    discovSensor("tbal", "temperature", "°C");
    discovSensor("enconso", "energy", "kWh");
    discovSensor("eninj", "energy", "kWh");
    discovSensor("enpvj", "energy", "kWh");
    discovSensor("enpvp", "energy", "kWh");
    discovSensor("enconsototale", "energy", "kWh");

    discovSensor("tempo_tarif_bleu_hp", "tarif", "€");
    discovSensor("tempo_tarif_bleu_hc", "tarif", "€");
    discovSensor("tempo_tarif_blanc_hp", "tarif", "€");
    discovSensor("tempo_tarif_blanc_hc", "tarif", "€");
    discovSensor("tempo_tarif_rouge_hp", "tarif", "€");
    discovSensor("tempo_tarif_rouge_hc", "tarif", "€");

    discovSensor("tempo_tarif_current", "tarif", "€");
    discovSensor("tempo_hp_on", "tarif", "€");

    discovSensor("tempo_j", "", "");
    discovSensor("tempo_j1", "", "");
    discovSensor("tempo_hc", "", "");
  }
}


/*****************************************************************************************
**                                   Routine callback                                   **
*****************************************************************************************/
void mqttReceiverCallback(char* topic, byte* inFrame, unsigned int length) 
{
    debugD("[MQTT] - Message receved on topic: %s", topic);
    String msg;
    
    for (int i = 0; i < length; i++) {
        msg += (char)inFrame[i];
    }

    debugD("[MQTT] - receved message : ", msg.c_str());

    if (strcmp(topicHAStatus, topic) == 0) {
      // Si message de démarrage de Home Assistant
      if (msg == "online") {
        mqttHaDiscovery();
      }
    }
}

/***************************************************************************************
**                      Serveur web (idée et conception Bellule)
**
***************************************************************************************/
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr">
<head>
 <meta name="viewport" content="width=device-width, initial-scale=1" charset="UTF-8" />
 <title>%TITRE_LONG%</title>
 <link rel="stylesheet" href="https://www.w3schools.com/w3css/4/w3.css">
 <link rel="stylesheet" href="https://fonts.googleapis.com/css?family=Allerta+Stencil">
 <script src="https://code.jquery.com/jquery-3.6.4.js" integrity="sha256-a9jBBRygX1Bh5lt8GZjXDzyOB+bWve9EiO7tROUtj/E=" crossorigin="anonymous"></script>
 <script>
  $(document).ready( function() { $('#div_refresh').load(document.URL +' #div_refresh');
        setInterval( function() { $('#div_refresh').load(document.URL +' #div_refresh'); }, 5000); });
 </script>
</head>
<body>
  <script>
    function toggleBottom() { var bottomDiv = document.getElementById("bottom");
      if (bottomDiv.style.display === "none") { bottomDiv.style.display = "block"; } 
      else { bottomDiv.style.display = "none"; } }
  </script>
  
  <div class= "w3-container w3-black w3-center w3-allerta">
  <h1>"%TITRE_LONG%"</h1></div>
  
  <div id="div_refresh">
    <div class="w3-card-4 w3-teal w3-padding-16 w3-xxxlarge w3-center">
      <p>Production Solaire</p>
      %PUIS_PANNEAU%&nbsp;W
    </div>
    <div class="w3-card-4 w3-blue-grey w3-padding-16 w3-xxxlarge w3-center">
      <p>Routage vers le cumulus</p>
      %DIMMER_BALLON%&#37;&nbsp;&nbsp;(%PUIS_BALLON%&nbsp;W)
    </div>
    <div class="w3-card-4 w3-padding-16 w3-xxxlarge w3-center">
      <p>Consommation RESEAU</p>
      %PUIS_CONSO%&nbsp;W
    </div>
  </div>
  
  <center>
    <button class="w3-bar w3-black w3-button w3-large w3-hover-white" onclick="toggleBottom()">Informations journalières</button>
  </center>
  
  <div id="bottom" style="display:none;">
    <div class="w3-card-4 w3-teal w3-padding-16 w3-xxxlarge w3-center">
      <p>Consommation EDF journalière</p>
      %CUMUL_CONSO%&nbsp;kWh
    </div>
    <div class="w3-card-4  w3-blue-grey w3-padding-16 w3-xxxlarge w3-center">
      <p>Production Solaire journalière</p>
      %CUMUL_PVJ%&nbsp;kWh
    </div>
    <div class="w3-card-4 w3-dark-grey w3-padding-16 w3-xxxlarge w3-center">
      <p>Production Solaire totale</p>
      %CUMUL_PVP%&nbsp;kWh
    </div>
    <div class="w3-card-4 w3-padding-16 w3-xxxlarge w3-center">
      <p>Injection sur EDF</p>
      %CUMUL_INJ%&nbsp;kWh
    </div>
  </div>
</body></html>
)rawliteral";

// Replaces placeholder with button section in your web page
String processor(const String& var) {
  debugD("process : %s", var.c_str());
  if (var == "TITRE_LONG") {
    return TITRE_LONG;
  }
  else if (var == "PUIS_PANNEAU") {
    return String(powpv);
  }
  else if (var == "DIMMER_BALLON") {
    return String(outbal);
  }
  else if (var == "PUIS_BALLON") {
    return String(powbal);
  }
  else if (var == "PUIS_CONSO") {
    return String(powreso);
  }
  else if (var == "CUMUL_CONSO") {
    return String(cumulConso, 1);
  }
  else if (var == "CUMUL_PVJ") {
    return String(cumulPV_J, 1);
  }
  else if (var == "CUMUL_PVP") {
    return String(cumulPV_P, 1);
  }
  else if (var == "CUMUL_INJ") {
    return String(cumulInj, 1);
  }
 
  return String();
}

void initWebServer() {
  // Route for root / web page
  webserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  // ATTENTION Danger - Permet l'acces a tous les fichiers du Filesystem
  webserver.serveStatic("/", LittleFS, "/");

  //Handle web callbacks for enabling or disabling discovery (using this method is just one of many ways to do this)
  webserver.on("/discovery_on", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", "<h1>Discovery ON...<h1><h3>Home Assistant MQTT Discovery enabled</h3>");
    delay(200);
    auto_discovery = true;
    mqttHaDiscovery();
  });
  
  webserver.on("/discovery_off", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", "<h1>Discovery OFF...<h1><h3>Home Assistant MQTT Discovery disabled. Previous entities removed.</h3>");
    delay(200);
    auto_discovery = false;
    mqttHaDiscovery();
  });

  webserver.begin();
}

/***************************************************************************************
**                             Gestion des Affichages
**
***************************************************************************************/
void doAffichage()
{
  static ulong nextTimeUpdate = 0UL;
  ulong ms = millis();

  // Retour à la page par defaut, apres le temp d'affichage
  if ((page != PAGE_DEFAULT) && (ms > tempsAffPage)) {
    page = PAGE_DEFAULT;
    updateAff = true;
  }

  // Mise en veille écran
  if (withVeille && !veille_on && (veille_deb < ms)) {
    veille_on = true;
    ledcWrite(PWM_CHANNEL, 0);      // Etteind l'éclairage ecran (luminosité 0%)
  }

  if (updateAff) {
    updateAff= false;

    if (page == PAGE_DEFAULT) {
      AffichageDefault();
    }
    else if (page == PAGE_CUMULS) {
      AffichageCumuls();
    }
    else if (page == PAGE_TEMPO) {
      AfficheTempo();
    }
    else if (page == PAGE_METEO) {
      AfficheMeteo();
    }
    else if (page == PAGE_METEO2) {
      AfficheMeteo2();
    }
    else if (page == PAGE_SOLAR) {
      AfficheSolar();
    }
    else if (page == PAGE_LUMI) {
      AfficheLumi();
    }
    else {
      page = PAGE_DEFAULT;
      AffichageDefault();
    }
  }
}

/***************************************************************************************
**                             Affichage principal (par défault)
**
***************************************************************************************/
void AffichageDefault() {
  String valStr;

  //  Dessin fenêtre principale
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);                      // Retour texte normal
  sprite.setFreeFont(&Orbitron_Light_24);                         // police d'affichage
  //sprite.setFreeFont(&Roboto_Thin_24);

  uint16_t couleurfond = TFT_BLACK;
  uint16_t couleurtexte = TFT_WHITE;
  if (codeTempoJ == 1) {
    couleurfond = FOND_BLEU;
    couleurtexte = TFT_WHITE;
  } 
  else if (codeTempoJ == 2) {
    couleurfond = FOND_BLANC;
    couleurtexte = TFT_BLACK;
  } 
  else if (codeTempoJ == 3) {
    couleurfond = FOND_ROUGE;
    couleurtexte = TFT_WHITE;
  } 

  //++++++++++++++++++++++++++++++++++++++++++++++++++++
  // Affichage Météo
  const short unsigned int *img = unknown;
  if (icone == "01d") img = clear_day;
  else if (icone == "01n") img = clear_night;
  else if (icone == "02d") img = partly_cloudy_day;
  else if (icone == "02n") img = partly_cloudy_night;
  else if ((icone == "03d") or (icone == "03n")) img = few_clouds;
  else if ((icone == "04d") or (icone == "04n")) img = cloudy;
  else if ((icone == "09d") or (icone == "09n")) img = rain;
  else if ((icone == "10d") or (icone == "10n")) img = lightRain;
  else if ((icone == "11d") or (icone == "11n")) img = thunderstorm;
  else if ((icone == "13d") or (icone == "13n")) img = snow;
  else if ((icone == "50d") or (icone == "50n")) img = fog;
  else if (icone == "80d") img = splash;
  else if (ID == "301") img = drizzle;
  else if (ID == "221") img = wind;
  sprite.pushImage(235, 120, 50, 50, img);

  // Affiche température extérieure
  if (tempExt < 3.0) {
    sprite.setTextColor(TFT_CYAN, TFT_BLACK);   // Risque de gel, on affiche en bleu clair
  }
  sprite.setTextDatum(MR_DATUM);                // centre droit
  valStr = String(tempExt, 1) + "`C";           // °C (probleme de police TFT_eSPY)
  sprite.drawString(valStr, 320, 160, 2);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++
  // Affichage heure et date
  sprite.drawRoundRect(234,  0, 86, 54, RECT_RADIUS, TFT_WHITE);
  sprite.setTextDatum(MC_DATUM);                                  // retour au centre milieu
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);                      // Retour texte normal
  time_t timeNow = now();
  String timeStr = formatHeure(timeNow);                          // La convertion en local est faite au formatage
  time_t local_time = TIMEZONE.toLocal(timeNow, &tz1_Code);       // Convert UTC to local time, returns zone code in tz1_Code, e.g "GMT"
  String dateStr = String(day(local_time)) + " " +String(Months[month(local_time)]);
  sprite.drawString(timeStr, 277, 20, 4);
  sprite.drawString(dateStr, 277, 42, 2);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++
  // Affichage de la température Ballon (si withTBal validée)
  if (withTBal && (tbal >= 1.)) {
    uint16_t couleur;
    if (tbal >= 60.)      couleur = color5;
    else if (tbal >= 50.) couleur = color4;
    else if (tbal >= 40.) couleur = color8;
    else                  couleur = color1;
    sprite.fillCircle(27, 84, 24, couleur);

    sprite.setTextColor(TFT_WHITE);                               // Retour texte normal
    sprite.setTextDatum(MC_DATUM);                                // retour au centre milieu
    sprite.drawString(String(tbal, 0) + "`", 26, 80);             //°C (probleme de police TFT_eSPY)
  }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++
  // Affichage Panneaux  Solaire
  sprite.drawRoundRect(0, 0, 226, 55, RECT_RADIUS, TFT_WHITE);    // Entourage de la zone
  sprite.drawString("PANNEAUX PV", 115, 10, 2);                   // Titre de la zone

  if (powpv >= residuel) {
    valStr = String(powpv) + " W";
    sprite.drawString(valStr, 115, 35);
  } 
  else {
    // Hors service & lever/coucher
    sprite.setSwapBytes(true);
    sprite.pushImage(3, 20, 220, 29, Soleil);                     // Image "Hors service" avec levé et coucher du soleil

    sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
    sprite.drawString(lever, 23, 15, 2);                          // texte size to 2 (medium size)
    sprite.drawString(coucher, 203, 15, 2);
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
  }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++
  // Affichage valeur Cumulus
  sprite.drawRoundRect(0, 57, 226, 55, RECT_RADIUS, TFT_WHITE);   // Zone Cumulus
  sprite.drawString("ROUTAGE CUMULUS", 115, 68, 2);
  valStr = String(outbal) + " %";
  sprite.drawString(valStr, 115, 92);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++
  // Affichage valeur Consommation
  sprite.setTextColor(couleurtexte, couleurfond);
  sprite.fillRoundRect(0, 114, 226, 55, RECT_RADIUS, couleurfond);    // Affiche Couleur Tempo sur fond Zone Conso
  sprite.drawRoundRect(0, 114, 226, 55, RECT_RADIUS, couleurtexte);   // Zone Conso
  sprite.drawString((powreso < 0) ? "INJECTION RESEAU" : "CONSOMMATION RESEAU", 115, 126, 2);
  valStr = String(powreso) + " W";
  sprite.drawString(valStr, 115, 150);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++
  // Affichage couleur Tempo J et J+1
  sprite.setTextDatum(MC_DATUM);
  sprite.fillRoundRect(5, 120, 20, 20, 4, tempo_colors[codeTempoJ]);
  sprite.setTextColor(tempo_colors[codeTempoJ] != TFT_WHITE ? TFT_WHITE : TFT_BLUE);
  sprite.drawString("J" , 5+10, 120+10, 2);
  
  sprite.fillRoundRect(5, 144, 20, 20, 4, tempo_colors[codeTempoJ1]);
  sprite.setTextColor(tempo_colors[codeTempoJ1] != TFT_WHITE ? TFT_WHITE : TFT_BLUE);
  sprite.drawString("+1", 5+10, 144+10, 2);

  sprite.setTextColor(TFT_WHITE, TFT_BLACK);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++
  // Voyant assistant de consommation
  //int puis_disp = puis_ballon - puis_conso; 
  int puis_disp = 0;
  if ((powreso <0) && (outbal >90)) 
    puis_disp = 0 - powreso;                                // Ballon plein, coupé par thermostat mécanique
  else if ((powreso >0) && (outbal >10))
    puis_disp = powbal;                                      // ballon en chauffe 
  
  if      (puis_disp < 200) img = BtnR;
  else if ((puis_disp < 1000)) img = BtnO;
  else img = BtnV;
  sprite.pushImage(230, 55, 68, 68, img);

  if (puis_disp > 100) {
    sprite.setTextDatum(MC_DATUM);
    sprite.setTextColor(TFT_BLACK);
    sprite.drawString(String(((float)puis_disp /1000.),1), 230+34, 55+30);      // en kW
  }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++
  // En cas de chauffage électrique
  if (withRad && ((powreso + powpv) > 3000) && (powbal < 100)) {   // Si forte conso et ballon coupé
    sprite.pushImage(6, 122, 40, 40, chauffage);
  }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++
  // Affichage des Bargraphes latéraux
  int nbbarres;

  //++++++++++++++++++++++++++++++++++++++++
  // Bargraphe Panneaux PV
  if (powpv > residuel) {
    nbbarres = (8 * powpv) / pCaractPV;
    barregraphV(200, 50, nbbarres, 8, 20, 5, 1, color1);    // en vert
  }

  //++++++++++++++++++++++++++++++++++++++++
  // Bargraphe Cumulus
  nbbarres = (8 * outbal) / 100;
  barregraphV(200, 107, nbbarres, 8, 20, 5, 1, color4);     // en orange

  //++++++++++++++++++++++++++++++++++++++++
  // Bargraphe Consommation / Injection EDF
  if (powreso < 0) {
    nbbarres = (-8 * powreso) / pCaractPV;
    barregraphV(200, 164, nbbarres, 8, 20, 5, 1, color3);
  }
  else {
    nbbarres = (8 * powreso) / pCaractPV;
    if (nbbarres > 8) nbbarres = 8;

    // Barregraph arc-en-ciel
    const uint16_t couleurs[] = {color1, color2, TFT_CYAN, color3, color4, color45, color5, color6};
    const int barresMax= 8, largeur= 20, pas_v= 5, pas_esp= 1;

    // Efface la zone
    sprite.fillRect(200, 164 - (barresMax * pas_v), largeur, barresMax * pas_v, TFT_BLACK);
    // Trace les barres
    for (int ii=0 ; ii < nbbarres ; ii++) {
      sprite.fillRect(200, 164 - ((ii+1) * pas_v), largeur, (pas_v - pas_esp), couleurs[ii]);
    }
  }

  //+++++++++++++++++++++++++++++++++++
  // Niveau du signal WiFi
  int level = WiFi.RSSI();

  if (level > -70)      img= signal4;
  else if (level > -75) img= signal3;
  else if (level > -80) img= signal2;
  else if (level > -90) img= signal1;
  else                  img= signal0;
  sprite.pushImage(296, 57, 24, 23, img);

  //+++++++++++++++++++++++++++++++++++
  // Bargraph du niveau d'ecrairage de l'ecran 
  //barregraphV(302, 109, backligthLevel, 5, 12, 5, 1, color3);

  //+++++++++++++++++++++++++++++++++++
  // Bargraph du niveau de la batterie
  if (withBatt) {
    sprite.pushImage(296, 121, 24, 24, pile);

    // Voltage pour batterie, les chiffres sont à modifier suivant votre batterie
    level = ((vBatt - 3.) * 14.);
    if (level < 0)  level = 0;
    if (level > 15) level = 14;
    sprite.fillRect(296+7, 141 - level, 10, level, color1);
  }

  //+++++++++++++++++++++++++++++++++++
  // Rafraichissement de tout l'écran
  sprite.pushSprite(0, 0);

  firstAff = false;
}

/***************************************************************************************
**                        Affichage de la page des cumuls
**
***************************************************************************************/
void AffichageCumuls() {
  //  Dessin fenêtre noire et titre
  sprite.fillSprite(TFT_BLACK);
  sprite.fillRoundRect(0, 2, 320, 25, 0, TFT_DARKGREY);
  sprite.setTextDatum(MC_DATUM);
  sprite.setTextColor(TFT_WHITE);
  sprite.drawString("INDEX JOURNALIERS (kWh)", 160, 15, 2);

  sprite.setTextColor(TFT_WHITE);
  sprite.drawRoundRect(0, 32, 159, 55, RECT_RADIUS, TFT_LIGHTGREY);
  sprite.drawString("PROD. PV J", 82, 42, 2);

  sprite.drawRoundRect(0, 89, 159, 55, RECT_RADIUS, TFT_LIGHTGREY);
  sprite.drawString("PROD. PV P", 82, 99, 2);

  sprite.drawRoundRect(161, 32, 159, 55, RECT_RADIUS, TFT_LIGHTGREY);
  sprite.drawString("CONSOMMATION", 241, 42, 2);

  sprite.drawRoundRect(161, 89, 159, 55, RECT_RADIUS, TFT_LIGHTGREY);
  sprite.drawString("INJECTION", 241, 99, 2);

  sprite.drawString(String(cumulPV_J, 1),   82, 65);
  sprite.drawString(String(cumulPV_P, 1),  82, 122);
  sprite.drawString(String(cumulConso, 1),  241, 65);
  sprite.drawString(String(cumulInj, 1), 241, 122);

  // Rafraichissement écran (indispensable après avoir tout dessiné)
  sprite.pushSprite(0, 0);
}

/***************************************************************************************
**                            Affichage page de Tempo
**
***************************************************************************************/
void AfficheTempo() {
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
  sprite.drawCentreString("Tempo", LCD_CENTER_X, 0, 4);

  sprite.setTextDatum(4);
  sprite.drawCentreString("J",    80, 40, 4);
  sprite.drawCentreString("J+1", 240, 40, 4);
  
  uint16_t coul = tempo_colors[codeTempoJ];
  sprite.fillRoundRect(80-40, 80, 80, 40, 5, coul);
  sprite.setTextColor((coul == TFT_WHITE) ? TFT_BLACK : TFT_WHITE, coul);
  sprite.drawCentreString(tempo_str[codeTempoJ],  80, 80+8, 4);

  coul = tempo_colors[codeTempoJ1];
  sprite.fillRoundRect(240-40, 80, 80, 40, 5, coul);
  sprite.setTextColor((coul == TFT_WHITE) ? TFT_BLACK : TFT_WHITE, coul);
  sprite.drawCentreString(tempo_str[codeTempoJ1], 240, 80+8, 4);

  sprite.pushSprite(0, 0);
}

/***************************************************************************************
** Affichage page Météo
**
***************************************************************************************/
void AfficheMeteo() {
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
  sprite.drawCentreString("Meteo", LCD_CENTER_X, 0, 2);

  sprite.setTextDatum(TL_DATUM);  // top left
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);

  int x1= 0, x2= 40, x3= 140, x4= 180, x5= 240, x6= 300 ; 
  int yy= 20;

  sprite.drawString("Temp.", x1, yy, 2);  sprite.drawString(String(myMeteo.temp,1 ) +"`C", x2, yy, 2);   
    sprite.drawString("Humi", x3, yy, 2); sprite.drawString(String(myMeteo.humidity) +"%", x4, yy, 2);
    yy +=16;

  sprite.drawString("Vent", x1, yy, 2);   sprite.drawString(String(myMeteo.wind_speed, 1) +"/" +String(myMeteo.wind_gust, 1) +"m/s", x2, yy, 2);
    sprite.drawString("Dir", x3, yy, 2);  sprite.drawString(String(myMeteo.wind_deg) +"`", x4, yy, 2);
    yy +=16;

  sprite.drawString("Pres", x1, yy, 2);   sprite.drawString(String(myMeteo.pressure, 0)+"hPa", x2, yy, 2);
    yy +=16;

  sprite.drawString("Nuage", x1, yy, 2);   sprite.drawString(String(myMeteo.clouds) +"%", x2, yy, 2);
    sprite.drawString("Pluie", x3, yy, 2); sprite.drawString(String(myMeteo.rain, 3) +"mm", x4, yy, 2);
    yy +=16;

  //sprite.drawString("Fiab.", x1, yy, 2);  sprite.drawString(String(meteoFeels, 1), x2, yy, 2);
  //  yy +=14;

  sprite.pushSprite(0, 0);
}


const char* getMeteoconIcon(uint16_t id, bool today)
{
  if ( today && (id/100 == 8) 
    && (myMeteo.dt < myMeteo.lever || myMeteo.dt > myMeteo.coucher)) 
    id += 1000; 

  if (id/100 == 2) return "thunderstorm";
  if (id/100 == 3) return "drizzle";
  if (id/100 == 4) return "unknown";
  if (id == 500)   return "lightRain";
  if (id == 511)   return "sleet";
  if (id/100 == 5) return "rain";
  if (id >= 611 && id <= 616) return "sleet";
  if (id/100 == 6) return "snow";
  if (id/100 == 7) return "fog";
  if (id == 800)   return "clear-day";
  if (id == 801)   return "partly-cloudy-day";
  if (id == 802)   return "cloudy";
  if (id == 803)   return "cloudy";
  if (id == 804)   return "cloudy";
  if (id == 1800)  return "clear-night";
  if (id == 1801)  return "partly-cloudy-night";
  if (id == 1802)  return "cloudy";
  if (id == 1803)  return "cloudy";
  if (id == 1804)  return "cloudy";

  return "unknown";
}

GfxUi ui = GfxUi(&tft); // Jpeg and bmpDraw functions

/***************************************************************************************
**                          Determine place to split a line line
***************************************************************************************/
// determine the "space" split point in a long string
int splitIndex(String text)
{
  uint16_t index = 0;
  while ( (text.indexOf(' ', index) >= 0) && ( index <= text.length() / 2 ) ) {
    index = text.indexOf(' ', index) + 1;
  }
  if (index) index--;
  return index;
}


/***************************************************************************************
**                         
**
***************************************************************************************/
void AfficheMeteo2() {

  sprite.fillSprite(TFT_BLACK);

  sprite.loadFont(AA_FONT_LARGE2, LittleFS);
  sprite.setTextDatum(BC_DATUM);
  sprite.setTextColor(TFT_YELLOW, TFT_BLACK);

  int x1 = 0;
  int x2 = 319;
  int yy = 40;

  //sprite.drawFastHLine(10, yy +5, 320 - 2 * 10, 0x4228);

  sprite.setTextDatum(BL_DATUM);

  // Temperature
  sprite.drawString("T " + String(myMeteo.temp, 1) +"°C", x1, yy);

  // Humidité
  sprite.setTextDatum(BR_DATUM);
  sprite.drawString("H " + String(myMeteo.humidity) +"%", x2, yy);
  yy+=34;

  // Pression
  sprite.drawString("P " + String(myMeteo.pressure, 0) +"hPa", x1, yy);
  yy+=34;

  // Nébulosité
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString("Neb " + String(myMeteo.clouds) +"%", x1, yy);
  yy+=34;

  // Pluie
  sprite.drawString("Plu " + String(myMeteo.rain, 1) +"mm", x1, yy);
  yy+=34;

  // Vent
  sprite.drawString("W " + String(myMeteo.wind_speed * 3.6, 0) +"km/h - " +myMeteo.wind_deg +"°", x1, yy);

  //sprite.setTextColor(TFT_ORANGE, TFT_BLACK);

  // Icone
  String weatherIcon = getMeteoconIcon(myMeteo.ID.toInt(), true);
  ui.drawBmp(&sprite, "/icon/" + weatherIcon + ".bmp", 219, 35);

/*
  // Description
  sprite.loadFont(AA_FONT_SMALL2, LittleFS);
  sprite.setTextDatum(BR_DATUM);
  String weatherText = myMeteo.description;
  weatherText.toLowerCase();

  sprite.setTextColor(TFT_ORANGE, TFT_BLACK);
  int xpos = 235;
  int splitPoint =  splitIndex(weatherText);
  sprite.setTextPadding(xpos - 100);  // xpos - icon width

  if (splitPoint) 
        sprite.drawString(weatherText.substring(0, splitPoint), xpos, 69);
  else  sprite.drawString(" ", xpos, 69);
  sprite.drawString(weatherText.substring(splitPoint), xpos, 86);
*/

  sprite.setTextPadding(0);
  sprite.unloadFont();
  sprite.pushSprite(0, 0);
}

/***************************************************************************************
** Affichage page dePrevision des productions Solaires
**
***************************************************************************************/
void AfficheSolar() {
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);
  sprite.setTextDatum(BL_DATUM);

  if (pCaractPV == 0) {
    debugE("[AfficheSolar] - pCaractPV == 0");
    return;
  }

  if (forecast_size == 0) {
    debugE("[AfficheSolar] - forecast_size == 0");
    sprite.setTextDatum(BC_DATUM);
    sprite.drawString("No data", LCD_CENTER_X, LCD_CENTER_Y, 4);
    sprite.pushSprite(0, 0);
    return;
  }

  // Graduations verticale tous les 1Kw
  int kw = pCaractPV / 1000;
  for (int ii=0 ; ii < kw ; ii++) {
    sprite.drawFastHLine(0, 150 -((150*ii)/kw), 319, TFT_DARKGREY);
    if (ii >0)
      sprite.drawString(String(ii)+"kw", 0, 150 -((150*ii)/kw)+1, 2);
  }

  int lg = 320 / forecast_size;

  for (int ii=0 ; ii < forecast_size; ii++) {
    int xx = lg * ii;
    int hh = (150 * forecast_watts[ii]) / pCaractPV;
    uint16_t coul = forecast_watts[ii] >1500 ? TFT_RED : TFT_BROWN;

    //sprite.drawRect(xx, 150 -hh, lg -1, hh, TFT_RED);
    sprite.drawRect(xx, 150 -hh, lg -1, hh, coul);

    int hh2 = (150 * forecast_wattshours[ii]) / pCaractPV;
    sprite.drawRect(xx, 150 -hh2, lg -1, hh2, TFT_GREEN);

    // Graduation horizontale toutes les 3 heures
    int heure = hour(forecast_times[ii]);
    int min = minute(forecast_times[ii]);
    if (((heure % 3) == 0) && (min == 0)) {
      sprite.drawString(String(heure), xx, 169, 2);
    }
  }

  // Valeurs journalieres
  sprite.setTextDatum(BR_DATUM);
  sprite.drawString(String(float(forecast_whday[0]) /1000., 1) +"kWh", 149, 15, 2);
  sprite.drawString(String(float(forecast_whday[1]) /1000., 1) +"kWh", 319, 15, 2);

  // Nb interogation restantes
  sprite.drawString(String(ratelimit_remaining), 319, 35, 2);

  sprite.pushSprite(0, 0);
}

/***************************************************************************************
** Affichage page de reglage de la luminosité écran
**
***************************************************************************************/
void AfficheLumi() {
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);
  sprite.drawCentreString("Eclairage", LCD_CENTER_X, 0, 4);

  // Bargraph du niveau d'ecrairage de l'ecran 
  barregraphV(300, 170, backligthLevel, 5, 20, 30, 2, color3);

  sprite.pushSprite(0, 0);
}

/***************************************************************************************
** Affiche un barregraphe vertical
**   (xx,yy)   coordoné de la base (en bas, a gauche)
**   barres    le nombre de barres visibles
**   barresMax le nombre de barres maximum
**   largeur   la largeur du barregraph en pixels
**   pas_v     le pas vertical des bassres en pixels
**   pas_esp   l'espacement entre barres en pixels (compris dans le pas)
***************************************************************************************/
void barregraphV (int xx, int yy, int barres, int barresMax, int largeur, int pas_v, int pas_esp, uint32_t couleur)
{
  if (barres > barresMax) 
    barres = barresMax;

  // Efface la zone
  sprite.fillRect(xx, yy - (barresMax * pas_v), largeur, barresMax * pas_v, TFT_BLACK);

  // Trace les barres
  for (int ii=0 ; ii < barres ; ii++) {
    sprite.fillRect(xx, yy - ((ii+1) * pas_v), largeur, (pas_v - pas_esp), couleur);
  }
}

/***************************************************************************************
**                           Réception des données météo
**                 Valeurs issues de Open Weather (Gestion Forecast)
***************************************************************************************/
void reqMeteo() {
  debugI("[Meteo] - Weather from OpenWeather");

  OW_forecast *forecast = new OW_forecast;
  ow.partialDataSet(false); // Collect a subset of the data available

  // Previsions à 3H sur 24h (API Openweather Map 2.5 Forecast (gratuit))
  if (!ow.getForecast(forecast, ow_api_key, ow_latitude, ow_longitude, "metric", "fr")) {
    debugE("[Meteo] - getForecast error");
    return;
  }

  debugD("[Meteo] - Position : Lat: %f, Long: %f, Ville: ", ow.lat, ow.lon, forecast->city_name.c_str());
  debugD("[Meteo] - Soleil   : Lever: %s - Coucher: %s", formatHeure(forecast->sunrise), formatHeure(forecast->sunset));
  String msg = "[Meteo] - Heures previsions :";
  for (int ii=0 ; ii<8; ii++) msg += " " +formatHeure(forecast->dt[ii]);
  debugD("%s", msg.c_str());

  for (int ii=0 ; ii<8; ii++) {
    debugD("[Meteo] - Prévis. de %s : T: %.1f°C, H: %d%%, Vent: %.2fm/s, Dir: %d°, P: %.0fhPa, Main: %s, Desc: %s, Icon: %s - ID: %d"
      , formatHeure(forecast->dt[ii])
      , forecast->temp[ii] 
      , forecast->humidity[ii]
      , forecast->wind_speed[ii] 
      , forecast->wind_deg[ii]
      , forecast->pressure[ii]
      , forecast->main[ii].c_str() 
      , forecast->description[ii].c_str()
      , forecast->icon[ii].c_str() 
      , forecast->id[ii]);
  }

  lever = formatHeure(forecast->sunrise);
  coucher = formatHeure(forecast->sunset);
  tempExt = forecast->temp[0];
  icone = (forecast->icon[0]);
  ID = (forecast->id[0]);

  esp_task_wdt_reset();
}

/***************************************************************************************
**                         Requete OpenWeather
**
***************************************************************************************/
void reqMeteo2() {
  const String urlCourante = "https://api.openweathermap.org/data/2.5/weather?lat=" + ow_latitude + "&lon=" + ow_longitude +"&units=metric&lang=fr&appid=" + ow_api_key;

  debugI("[Meteo2] - GET %s", urlCourante.c_str());

  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(3000);
  http.useHTTP10(true);
  http.addHeader("accept", "application/json");
  http.begin(urlCourante);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY) {
    debugE("[Meteo2] - Courante - Failed to GET - error: %s", http.errorToString(httpCode).c_str());
    http.end();
    return; 
  }

  // Parse response
  JsonDocument doc;
  String payload = http.getString();
  debugD("[Meteo2] - payload : %s", payload.c_str());
  auto err = deserializeJson(doc, payload);
  //auto err = deserializeJson(doc, http.getStream());
  if (err) {
    debugE("[Meteo2] - Courante - deserializeJson failed");
    http.end();
    esp_task_wdt_reset();
    return;
  }

  debugD("[Meteo2] - AAA0 - myMeteo adrs: %p", &myMeteo);


  debugD("[Meteo2] - AAA1");
  // Read values
  time_t val = doc["dt"].as<long>();
  debugD("[Meteo2] - AAA1-1a");
  String str = formatHeure(val);
  debugD("[Meteo2] - AAA1-1b - str= %s", str.c_str());

  debugD("[Meteo2] - AAA1-1c - old dt= %s", myMeteo.dt.c_str());

  myMeteo.dt = str;
  debugD("[Meteo2] - AAA1-1d");


  myMeteo.dt         = formatHeure(doc["dt"].as<long>());
  debugD("[Meteo2] - AAA1-1e");
  myMeteo.lever      = formatHeure(doc["sys"]["sunrise"].as<long>());
  debugD("[Meteo2] - AAA1-2");
  myMeteo.coucher    = formatHeure(doc["sys"]["sunset"].as<long>());

  debugD("[Meteo2] - AAA2");
  myMeteo.temp       = doc["main"]["temp"].as<float>();
  myMeteo.feels_like = doc["main"]["feels_like"].as<float>();
  myMeteo.temp_min   = doc["main"]["temp_min"].as<float>();
  myMeteo.temp_max   = doc["main"]["temp_max"].as<float>();
  myMeteo.pressure   = doc["main"]["pressure"].as<float>();
  myMeteo.humidity   = doc["main"]["humidity"].as<uint>();

  debugD("[Meteo2] - AAA3");
  myMeteo.clouds     = doc["clouds"]["all"].as<uint>();
  myMeteo.visibility = doc["visibility"].as<float>() /1000.;
  myMeteo.rain       = (doc["rain"] && doc["rain"]["1h"]) ? doc["rain"]["1h"].as<float>() : 0.;
  myMeteo.snow       = (doc["snow"] && doc["snow"]["1h"]) ? doc["snow"]["1h"].as<float>() : 0.;

  debugD("[Meteo2] - AAA4");
  myMeteo.wind_speed = doc["wind"]["speed"].as<float>();
  myMeteo.wind_gust  = (doc["wind"]["gust"]) ? doc["wind"]["gust"].as<float>() : 0.;
  myMeteo.wind_deg   = doc["wind"]["deg"].as<uint>();

  debugD("[Meteo2] - AAA5");
  myMeteo.icon       = doc["weather"][0]["icon"].as<String>();
  myMeteo.ID         = doc["weather"][0]["id"].as<String>();
  myMeteo.wmain       = doc["weather"][0]["main"].as<String>();
  myMeteo.description = doc["weather"][0]["description"].as<String>();

  debugD("[Meteo2] - AAA6");
  myMeteo.cityName   = doc["name"].as<String>();

  debugD("[Meteo2] - AAA7");
  lever   = myMeteo.lever;
  coucher = myMeteo.coucher;
  tempExt = myMeteo.temp;
  icone   = myMeteo.icon;
  ID      = myMeteo.ID;

  debugD("[Meteo2] - AAA8");

  debugD("[Meteo2] -     dt= %s >> T= %.1f°, Tres= %.1f°, Tmin= %.1f°, Tmax= %.1f°, H= %d%%, P= %.0fhPa, Clouds= %d%%\
, Visi= %.1fkm, Pluie= %.1fmm, Neige= %.1fmm, proba: -, Vent= %.1fm/s, Rafale= %.1fm/s, Dir= %d°, icone= %s, id= %s; main= %s, desc= %s, name= %s, lever= %s, coucher= %s"
    , myMeteo.dt.c_str() 
    , myMeteo.temp
    , myMeteo.feels_like
    , myMeteo.temp_min
    , myMeteo.temp_max
    , myMeteo.humidity
    , myMeteo.pressure
    , myMeteo.clouds
    , myMeteo.visibility
    , myMeteo.rain
    , myMeteo.snow
    , myMeteo.wind_speed
    , myMeteo.wind_gust
    , myMeteo.wind_deg
    , myMeteo.icon.c_str()
    , myMeteo.ID.c_str()
    , myMeteo.wmain.c_str()
    , myMeteo.description.c_str()
    , myMeteo.cityName.c_str() 
    , myMeteo.lever.c_str() 
    , myMeteo.coucher.c_str()
  );

  http.end();
  esp_task_wdt_reset();

  reqtMeteoPrevi();

  return;
}

/***************************************************************************************
**                         Requete OpenWeather
**
***************************************************************************************/
void reqtMeteoPrevi() {
  String urlPrevi = "https://api.openweathermap.org/data/2.5/forecast?cnt=1&lat=" + ow_latitude 
    + "&lon=" + ow_longitude 
    +"&units=metric&lang=fr&appid=" + ow_api_key;

  debugI("[MeteoPrevi] - GET %s", urlPrevi.c_str());

  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(3000);
  http.useHTTP10(true);
  http.addHeader("accept", "application/json");
  http.begin(urlPrevi);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY) {
    debugE("[MeteoPrevi] - Failed to GET - error: %s", http.errorToString(httpCode).c_str()); 
    http.end();
    return;
  }

  // Parse response
  JsonDocument doc;
  String payload = http.getString();
  debugD("[MeteoPrevi] - payload : %s", payload.c_str());
  auto err = deserializeJson(doc, payload);
  //auto err = deserializeJson(doc, http.getStream());
  if (err) {
    debugE("[MeteoPrevi] - deserializeJson failed");
    http.end();
    esp_task_wdt_reset();
    return;
  }

  int nb = doc["cnt"].as<int>();
  if (nb > 8) nb= 8;

  JsonArray previs = doc["list"];
  for (int ii=0 ; ii< nb; ii++) {
    JsonObject prev = previs[ii];
    
    // Read values
    myMeteo.previ[ii].dt         = formatHeure(prev["dt"].as<long>());

    myMeteo.previ[ii].temp       = prev["main"]["temp"].as<float>();
    myMeteo.previ[ii].feels_like = prev["main"]["feels_like"].as<float>();
    myMeteo.previ[ii].temp_min   = prev["main"]["temp_min"].as<float>();
    myMeteo.previ[ii].temp_max   = prev["main"]["temp_max"].as<float>();

    myMeteo.previ[ii].humidity   = prev["main"]["humidity"].as<uint>();
    myMeteo.previ[ii].pressure   = prev["main"]["pressure"].as<float>();

    myMeteo.previ[ii].clouds     = prev["clouds"]["all"].as<int>();
    myMeteo.previ[ii].visibility = prev["visibility"].as<float>() /1000.;

    // La pluie n'est pas toujour presente, et peut avoir 2 parametres
    if      ((prev["rain"]) && (prev["rain"]["1h"])) myMeteo.previ[ii].rain = prev["rain"]["1h"].as<float>();
    else if ((prev["rain"]) && (prev["rain"]["3h"])) myMeteo.previ[ii].rain = prev["rain"]["3h"].as<float>();
    else  myMeteo.previ[ii].rain = 0.;

    // La pluie n'est pas toujour presente, et peut avoir 2 parametres
    if      ((prev["snow"]) && (prev["snow"]["1h"])) myMeteo.previ[ii].snow = prev["snow"]["1h"].as<float>();
    else if ((prev["snow"]) && (prev["snow"]["3h"])) myMeteo.previ[ii].snow = prev["snow"]["3h"].as<float>();
    else  myMeteo.previ[ii].snow = 0.;

    myMeteo.previ[ii].prob       = (prev["pop"]) ? (uint)(100.* prev["pop"].as<float>()) : 0;

    myMeteo.previ[ii].wind_deg   = prev["wind"]["deg"].as<uint>();
    myMeteo.previ[ii].wind_gust  = (prev["wind"]["gust"]) ? prev["wind"]["gust"].as<float>() : 0.;
    myMeteo.previ[ii].wind_speed = prev["wind"]["speed"].as<float>();

    myMeteo.previ[ii].icon       = prev["weather"][0]["icon"].as<String>();
    myMeteo.previ[ii].ID         = prev["weather"][0]["id"].as<String>();
    myMeteo.previ[ii].main       = prev["weather"][0]["main"].as<String>();
    myMeteo.previ[ii].description = prev["weather"][0]["description"].as<String>();
     
    debugD("[MeteoPrevi] - dt= %s >> T= %.1f°, Tres= %.1f°, Tmin= %.1f°, Tmax= %1.f°, H= %d%%, P= %.0fhPa, Clouds= %d%%\
, Visi= %.1fkm, Pluie= %.1fmm, Neige= %.1fmm, Proba= %d, Vent= %.1fm/s, Rafale= %.1fm/s, Dir= %d°, icone= %s, id= %s; main= %s, desc= %s"
      , myMeteo.previ[ii].dt.c_str() 
      , myMeteo.previ[ii].temp
      , myMeteo.previ[ii].feels_like
      , myMeteo.previ[ii].temp_min
      , myMeteo.previ[ii].temp_max    
      , myMeteo.previ[ii].humidity
      , myMeteo.previ[ii].pressure
      , myMeteo.previ[ii].clouds
      , myMeteo.previ[ii].visibility
      , myMeteo.previ[ii].rain
      , myMeteo.previ[ii].snow
      , myMeteo.previ[ii].prob
      , myMeteo.previ[ii].wind_speed
      , myMeteo.previ[ii].wind_gust
      , myMeteo.previ[ii].wind_deg
      , myMeteo.previ[ii].icon.c_str()
      , myMeteo.previ[ii].ID.c_str()
      , myMeteo.previ[ii].main.c_str()
      , myMeteo.previ[ii].description.c_str()  );
  }

  http.end();
  esp_task_wdt_reset();
}

/***************************************************************************************
**                         Requete Forecast.solar
**
***************************************************************************************/
void reqMeteoSolaire (ulong ms) {
  static ulong next = 0UL;
  static ulong lastSolar = 0UL;

  if ((ms < next) || (WiFi.status() != WL_CONNECTED))
    return; 

  next = ms + UPDATE_METEO_SOLAIRE_INTERVAL;

  // Cf API doc : https://doc.forecast.solar/api
  //String horizon = "horizon=0,0,0,0,0,0,10,20,20,20,20,20";
  int inclinaison = 23;
  int azimuth = -20;      // (cap 160°)
  float damping = 0.5;
  
  const String url = "https://api.forecast.solar/estimate/" + ow_latitude + "/" + ow_longitude 
    +"/" + String(inclinaison) 
    + "/" + String(azimuth) 
    + "/" + String(pCaractPV/1000) + ".json"
    + "?damping=" + String(damping, 2)
    + "&no_sun=1";

  debugI("[MeteoSolaire] - GET %s", url.c_str());

  forecast_size = 0;

  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(3000);
  //http.useHTTP10(true);
  http.addHeader("accept", "application/json");
  //http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "Companion MSunPV ESP32");

  http.begin(url);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY) {
    debugE("[MeteoSolaire] - Failed to GET - code: %d, error: %s", httpCode, http.errorToString(httpCode).c_str()); 
    http.end();
    return;
  }

  // Parse response
  JsonDocument doc;
  String payload = http.getString();
  debugD("[MeteoSolaire] - payload : %s", payload.c_str());
  auto err = deserializeJson(doc, payload);
  if (err) {
    debugE("[MeteoSolaire] - deserializeJson failed");
    http.end();  // Ferme la connexion
    return;
  }

  JsonObject watts = doc["result"]["watts"];
  debugD("[MeteoSolaire] - watts size= %d", watts.size());
  if (watts.size() > 48) {
    debugE("[MeteoSolaire] - trop de données reçues");
    return;
  }
  forecast_size = watts.size();
  int ii = 0;
  for (JsonPair item : watts) {
    //debugD ("Item: key= %s, value= %d", item.key().c_str(), item.value().as<int>());
    struct tm tm = {0};
    strptime(item.key().c_str(), "%Y-%m-%d %H:%M:%S", &tm);
    forecast_times[ii] = mktime(&tm);
    forecast_watts[ii] = item.value();
    ii++;
  }

  JsonObject wattshours = doc["result"]["watt_hours_period"];
  debugD("[MeteoSolaire] - wattshours size= %d", wattshours.size());
  if (wattshours.size() > 48) {
    debugE("[MeteoSolaire] - trop de données reçues");
    return;
  }
  ii = 0;
  for (JsonPair item : wattshours) {
    forecast_wattshours[ii] = item.value();
    ii++;
  }

  JsonObject whday = doc["result"]["watt_hours_day"];
  debugD("[MeteoSolaire] - whday size= %d", whday.size());
  if (whday.size() > 2) {
    debugE("[MeteoSolaire] - trop de données reçues");
    return;
  }
  ii = 0;
  for (JsonPair item : whday) {
    forecast_whday[ii] = item.value();
    ii++;
  }

  ratelimit_remaining = doc["message"]["ratelimit"]["remaining"];
  debugD("[MeteoSolaire] - ratelimit_remaining= %d", ratelimit_remaining);

  lastSolar = ms;
  next = ms + UPDATE_METEO_SOLAIRE_INTERVAL;
  debugI("[MeteoSolaire] - OK, now: %s, next: %s", formatHeure(ms), formatHeure(next));

  http.end();  // Ferme la connexion
  esp_task_wdt_reset();
}

/***************************************************************************************
**                    Decryptage des valeurs lues dans le xml
**
***************************************************************************************/
String parseSubString(String str, String debTag, String finTag) {
  int deb = str.indexOf(debTag);
  int fin = str.indexOf(finTag);
  if ((deb <0) || (fin<0))
    return "";

  return str.substring( deb + debTag.length(), fin);
}

int hexa2int(String hexa) {
  char tbl[16];
  int iVal;

  hexa.toCharArray(tbl, 16);
  sscanf(tbl, "%x", &iVal);

  return iVal;
}

float hexa2float(String hexa) {
  return float(hexa2int(hexa));
}

/***************************************************************************************
**                         Décodage des données MSunPV
**
***************************************************************************************/
bool decodeXML(String dataStr) {
  String str;
  String vals[16];  // 16 valeurs à récupérer pour les consos et températures

  //+++++++++++++++++++++++++++++
  // Les mesures ou sondes
  str = parseSubString(dataStr, "<paramSys>", "</paramSys>");
  debugD("[Decode] - <paramSys> : %s", str.c_str());
  split(vals, 10, str, ';');
  msunpv_time = vals[0];
  msunpv_date = vals[1];
  msunpv_enrgsd = (strcmp("On", vals[2].c_str()) == 0);

  str = parseSubString(dataStr, "<inAns>", "</inAns>");
  debugD("[Decode] - <inAns> %s: ", str.c_str());
  str.replace(",", ".");
  split(vals, 16, str, ';');
  powreso = vals[0].toInt();                // Consommation (si positif), ou Injection (si négatif)
  powpv = 0 - vals[1].toInt();              // Panneaux PV
  powpv = (powpv > residuel) ? powpv : 0;
  outbal = vals[2].toInt() / 4;             // Dimmer Cumulus (0 à 400%) ==> 0-100%
  outrad = vals[3].toInt() / 4;             // Dimmer Radiateur (0 à 400%) ==> 0-100%
  voltres = vals[4].toInt();                // Tension réseau (V)
  tbal = vals[5].toFloat();                 // Sonde température Cumulus (en degrés, avec une decimale)
  tsdb = vals[6].toFloat();                 // Sonde température Salle de bain (en degrés, avec une decimale)
  tamb = vals[7].toFloat();                 // Sonde température Ambiente (en degrés, avec une decimale)

  powbal = (outbal * pCaractBallon) /100;
  powbal = (powbal > residuel) ? powbal : 0;

  debugD("[Decode] - <inAns> ==> powreso= %dW, powpv= %dW, outbal= %d%, outrad= %d%, voltres= %dV, tbal= %.1f°C, tsdb= %.1f°C, tamb= %.1f°C"
      , powreso
      , powpv
      , outbal
      , outrad
      , voltres
      , tbal
      , tsdb
      , tamb );

  //+++++++++++++++++++++++++++++
  // Surveillance des sondes: 0 pas de dépassement, 1 dépassement maxi, 2 dépassement mini ou sonde déconnectée.
  str = parseSubString(dataStr, "<survMm>", "</survMm>");
  debugD("[Decode] - <survMm> : %s", str.c_str()); 
  split(vals, 16, str, ';');
  for(int ii=0; ii<16 ; ii++) {
    msunpv_surv[ii] = vals[ii].toInt();
  }

  //+++++++++++++++++++++++++++++
  // Position des 8 commandes, en binaire sur 4 bits.
  str = parseSubString(dataStr, "<cmdPos>", "</cmdPos>");
  debugD("[Decode] - <cmdPos> : %s", str.c_str()); 
  split(vals,  8, str, ';');
  for (int ii=0 ; ii<8 ; ii++) {
    msunpv_cmd[ii] = vals[ii].toInt();
  }

  int val =  hexa2int(vals[0]);
  cmdManuBal = ((val & 0x01) != 0);
  cmdAutoBal = ((val & 0x02) != 0);
  cmdManuRad = ((val & 0x04) != 0);
  cmdAutoRad = ((val & 0x08) != 0);

  val = hexa2int(vals[7]);
  cmdInject = ((val & 0x01) != 0);
  cmdZero = ((val & 0x02) != 0);
  cmdMoyen = ((val & 0x04) != 0);
  cmdFort = ((val & 0x08) != 0);

  //+++++++++++++++++++++++++++++
  // Valeurs des 16 sorties de 0 à 100%.
  str = parseSubString(dataStr, "<outStat>", "</outStat>");
  debugD("[Decode] - <outStat> : %s", str.c_str()); 
  split(vals, 16, str, ';');
  for (int ii = 0 ; ii<16 ; ii++) {
    msunpv_out[ii] = vals[ii].toInt();
  }

  //+++++++++++++++++++++++++++++
  // Valeurs des 8 compteurs en hexadécimal.
  str = parseSubString(dataStr, "<cptVals>", "</cptVals>");
  debugD("[Decode] - <cptVals> : %s", str.c_str()); 
  split(vals,  8, str, ';');
  cumulConso = hexa2float(vals[0]) /10000.;       // Consomation réso (import) journalièer en kWh
  cumulInj   = hexa2float(vals[1]) /10000.;       // Injection réso (export) journalière en kWh
  cumulPV_J  = hexa2float(vals[2]) /10000.;       // Production Panneaux journalère en kWh
  cumulPV_P  = hexa2float(vals[3]) /10.;          // Production Panneaux totale en kWh
  
  debugD("[Decode] - <cptVals> ==> cumulConso= %.3fkWh, cumulInj= %.3fkWh, cumulPV_J= %.3fkWh, cumulPV_P= %.3fkWh"
    , cumulConso
    , cumulInj
    , cumulPV_J
    , cumulPV_P ); 

  //+++++++++++++++++++++++++++++
  // Valeurs calculées en sortie des modules Chauffage. 
  str = parseSubString(dataStr, "<chOutVal>", "</chOutVal>");
  debugD("[Decode] - <chOutVal> : %s", str.c_str()); 
  split(vals, 8, str, ';');
  for (int ii=0 ; ii<8 ; ii++) {
    msunpv_chout[ii] = hexa2int(vals[ii]);
  }

  return true;
}

/***************************************************************************************
**                         Réception des données MSunPV
**
***************************************************************************************/
bool reqMSunPV(ulong ms) {
  static ulong next = 0UL;
  static ulong lastMsunPV = 0UL;

  bool isOk = false;

  if (WiFi.status() != WL_CONNECTED) {
    /*
    // En cas de perte de WIFI > 10mn
    if ((next !=0) && (ms > next + 600000)) {
      ESP.restart();
    }
    */

    return isOk;
  }

  if (ms < next)
    return isOk;

  next = ms + UPDATE_MSUNPV_INTERVAL;

  // Affichage indicateur de rafraichissement des données en haut à gauche : démarrage
  if ((page == PAGE_DEFAULT) && (!firstAff)) {
    sprite.drawSmoothCircle(8, 8, 4, TFT_GREENYELLOW, TFT_TRANSPARENT); 
    sprite.pushSprite(0, 0);
  }

  String url = "http://" + msunpv_server + "/status.xml"; 
  debugI("[MSunPV] - GET %s", url.c_str());

  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(3000);
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY) {
    debugE("[MSunPV] - Failed to GET - error: %s", http.errorToString(httpCode).c_str());
    http.end();
    return isOk;
  }

  String data = http.getString();
  //debugD("[MSunPV] - GET [Ok], data : [%s]", data);
  if (decodeXML(data)) {
    isOk = true;
    updateAff = true;
    lastMsunPV = ms;                    // Dernière mise à jour éffective
    
    // Indicateur de rafraichissement des données => statut terminé (il sera effacé lors du rafraichissement de l'écran)
    if ((page == PAGE_DEFAULT) && (!firstAff)) {
      sprite.fillSmoothCircle(8, 8, 4, TFT_GREENYELLOW);
      sprite.pushSprite(0, 0);
    }
  }
  
  http.end();
  esp_task_wdt_reset();
  return isOk;
}

/***************************************************************************************
**                         Réception des données Tempo
**
***************************************************************************************/
void reqTempo() {
  static time_t next = 0L;
  static time_t lastTempo = 0L;
  static const String urlTempoJour   = "https://www.api-couleur-tempo.fr/api/jourTempo/today";
  static const String urlTempoDemain = "https://www.api-couleur-tempo.fr/api/jourTempo/tomorrow";

  time_t utcTime = now();
  if ((utcTime < next) || (WiFi.status() != WL_CONNECTED)) 
    return;

  next = utcTime + 30;             // En cas d'erreur

  JsonDocument doc;
  codeTempoJ = codeTempoJ1 = 0;

  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(3000);
  http.addHeader("accept", "application/json");

  debugI("[Tempo] - GET %s", urlTempoJour.c_str());
  http.begin(urlTempoJour);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY) {
    debugE("[Tempo] - TempoJour, Failed to GET - error: %s", http.errorToString(httpCode).c_str()); 
    http.end();
    return;
  }
  else {
    DeserializationError err = deserializeJson(doc, http.getString());
    if (err) {
      debugE("[Tempo] - deserializeJson() failed: %p", err);
      http.end();
      return;
    }

    codeTempoJ = doc["codeJour"];
    if ((codeTempoJ < 0) || codeTempoJ > 3) 
      codeTempoJ = 0;
  }

  http.end();

  //------------------------
  debugI("[Tempo] - GET %s", urlTempoDemain.c_str());
  http.begin(urlTempoDemain);
  httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY) {
    debugE("[Tempo] - TempoDemain, Failed to GET - error: %s", http.errorToString(httpCode).c_str()); 
    http.end();
    return;
  }

  DeserializationError err = deserializeJson(doc, http.getString());
  if (err) {
    debugE("[Tempo] - deserializeJson() failed: %p", err);
    http.end();
    return;
  }

  codeTempoJ1 = doc["codeJour"];
  if ((codeTempoJ1 < 0) || codeTempoJ1 > 3) 
    codeTempoJ1 = 0;

  lastTempo = utcTime;                             // Derniere mise à jour effective
  updateTempo = true;

  // Mise a jour à : 6h00, 11h00, 22h00 et 0h00
  time_t lt = TIMEZONE.toLocal(utcTime, &tz1_Code);
  tmElements_t tt;
  breakTime(lt, tt);

  if(tt.Hour < 6)       tt.Hour = 6;
  else if(tt.Hour < 11) tt.Hour = 11;
  else if(tt.Hour < 22) tt.Hour = 22;
  else {
    tt.Hour = 0;
    tt.Day += 1;
  }

  tt.Minute = 0;
  tt.Second = 10;
  next = TIMEZONE.toUTC(makeTime(tt));

  debugI("[Tempo] - OK now= %s (%ld), next= %s (%ld), delai= %lds", 
    formatDateTime(utcTime).c_str(), utcTime, 
    formatDateTime(next).c_str(), next, 
    (next - utcTime));
  
  http.end();
  esp_task_wdt_reset();
}

/***************************************************************************************
**                         Réception des Tarifs Tempo
**
***************************************************************************************/
void reqTempoTarifs() {
  static time_t next = 0L;
  static time_t lastTempoTarifs = 0L;

  time_t utcTime = now();
  if ((utcTime < next) || (WiFi.status() != WL_CONNECTED)) 
    return;

  next = utcTime + 30;             // En cas d'erreur

  const String url = "https://www.myelectricaldata.fr/edf/tempo/price";
  JsonDocument doc;

  tarif_bleu_hp= tarif_bleu_hc= 0.;
  tarif_blanc_hp= tarif_blanc_hc= 0.;
  tarif_rouge_hp= tarif_rouge_hc= 0.;

  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(3000);
  http.addHeader("accept", "application/json");

  debugI("[TempoTarifs] - GET %s", url.c_str());
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY) {
    debugE("[TempoTarifs] - Failed to GET - error: %s", http.errorToString(httpCode).c_str());
    http.end();
    return; 
  }

  DeserializationError err = deserializeJson(doc, http.getString());
  if (err) {
    debugE("[TempoTarifs] - deserializeJson() failed: %p", err);
    http.end();
    return;
  }

  tarif_bleu_hp = doc["blue_hp"];
  tarif_bleu_hc = doc["blue_hc"];
  tarif_blanc_hp = doc["white_hp"];
  tarif_blanc_hc = doc["white_hc"];
  tarif_rouge_hp = doc["red_hp"];
  tarif_rouge_hc = doc["red_hc"];

  lastTempoTarifs = utcTime;                             // Derniere mise à jour effective
  updateTempo = true;

  // Mise a jour à : 6h00, 11h00, 22h00 et 0h00
  time_t lt = TIMEZONE.toLocal(utcTime, &tz1_Code);
  tmElements_t tt;
  breakTime(lt, tt);

  if(tt.Hour < 6)       tt.Hour = 6;
  else if(tt.Hour < 11) tt.Hour = 11;
  else if(tt.Hour < 22) tt.Hour = 22;
  else {
    tt.Hour = 0;
    tt.Day += 1;
  }

  tt.Minute = 0;
  tt.Second = 10;
  next = TIMEZONE.toUTC(makeTime(tt));

  debugI("[TempoTarifs] - OK now= %s (%ld), next= %s (%ld), delai= %lds", 
    formatDateTime(utcTime).c_str(), utcTime, 
    formatDateTime(next).c_str(), next, 
    (next - utcTime));

  http.end();
  esp_task_wdt_reset();
}

/***************************************************************************************
**         Lecture de la tension batterie
**
***************************************************************************************/
void readVbatt() {
  //vBatt = (float)(analogRead(4)) * 2. * 3300. / 4096.;         // Vbatt = 3.3V, 12bits
  vBatt = (float)(analogRead(4)) * 7.26 / 4096.;         // Vbatt = 3.3V, (12bits, Vref= 1,1V, analogInputDivider= 2) ==>  Val = (Read) * 1,1 * 2 /4096 
  debugD("[vBat] - vBat = %.3fV", vBatt);
}

/***************************************************************************************
**         Découpe la partie du xml voulue en autant de valeurs que trouvées
**
***************************************************************************************/
void split(String values[], int dimArray, String content, char separator) {
  // Clear le tableau de valeurs
  for (int ii = 0; ii < dimArray; ii++) {
    values[ii] = "";
  }

  if (content.length() == 0)
    return;

  content = content + separator;
  int posSep = 0;
  int deb = 0;

  for (int ii = 0; ii < dimArray; ii++) {
    posSep = content.indexOf(separator, posSep);
    if (posSep < 0)
      break;

    values[ii] = content.substring(deb, posSep);
    posSep = posSep + 1;
    deb = posSep;
  }
}

/***************************************************************************************
**             Formatage Unix time en String "12:34" (heure locale)
**
***************************************************************************************/
String formatHeure(time_t unixTime) {
  time_t local_time = TIMEZONE.toLocal(unixTime, &tz1_Code);
  char buffer[10];
  sprintf(buffer, "%02d:%02d", hour(local_time), minute(local_time));
  return String(buffer);
}

String formatDateTime(time_t unixTime) {
  time_t local_time = TIMEZONE.toLocal(unixTime, &tz1_Code);
  char buffer[25];
  sprintf(buffer, "%04d/%02d/%02d %02d:%02d:%02d", year(local_time), month(local_time), day(local_time), hour(local_time), minute(local_time), second(local_time));
  return String(buffer);
}

/***************************************************************************************
** Transforme les W en kW (par Bellule, encore lui!)
**
***************************************************************************************/
float wh_to_kwh(float wh) {
  return wh / 1000.0;
}

/***************************************************************************************
**                               Gestion du bouton éclairage (Simple click)
**
***************************************************************************************/
void handleClick() {

  if ((!veille_on) && (page == PAGE_LUMI)) {
    // Fait varier l'intensité d'éclairage de 1  5
    backligthLevel++;
    if (backligthLevel >= 5) {
      backligthLevel= 5;
    }

    // Réglage de la luminosité
    ledcWrite(PWM_CHANNEL, backlight[backligthLevel]);
    saveLumiParam();

    tempsAffPage = millis() + DUREE_AFF;
    updateAff = true;
    veille_deb = millis() + TEMPS_VEILLE;
  }
  else if (withVeille) {
    if (!veille_on) {
      veille_on = true;
      ledcWrite(PWM_CHANNEL, 0);                  // Eteind l'écran
    }
    else {
      veille_on = false;
      ledcWrite(PWM_CHANNEL, backlight[backligthLevel]);
      veille_deb = millis() + TEMPS_VEILLE;
    }
  }
 
  delay(10);     // Anti-rebond
}

/***************************************************************************************
**                               Gestion du bouton éclairage (Double click)
**
***************************************************************************************/
void doubleClick() {

  if ((!veille_on) && (page == PAGE_LUMI)) {
    // Fait varier l'intensité d'éclairage de 5 à 1
    backligthLevel--;
    if (backligthLevel <= 1) {
      backligthLevel = 1;
    }
    // Réglage de la luminosité
    ledcWrite(PWM_CHANNEL, backlight[backligthLevel]);
    saveLumiParam();

    tempsAffPage = millis() + DUREE_AFF;
    updateAff = true;
  }

  veille_deb = millis() + TEMPS_VEILLE;
  delay(10);     // Anti-rebond
}

/***************************************************************************************
**                               Gestion du Pages (Simple click)
**
***************************************************************************************/
void handlePage() {
  if (!veille_on) {
    page++;
    if (page > PAGE_LAST)
      page = PAGE_DEFAULT;

    veille_deb = millis() + TEMPS_VEILLE;
    tempsAffPage = millis() + DUREE_AFF;
    updateAff = true;
    delay(10);
  }
}

/***************************************************************************************
**                               Gestion du Pages (Double click)
**
***************************************************************************************/
void handlePageDef() {
  if (!veille_on) {
    page = 0;       // retour à la page par defaut

    veille_deb = millis() + TEMPS_VEILLE;
    tempsAffPage = millis() + DUREE_AFF;
    updateAff = true;
    delay(10);
  }
}


/***************************************************************************************
** Log message sur le port serie, sans risquer de blocker le programme
** si celui-ci est saturé ou non connecté.
**
***************************************************************************************/
void log_print(String msg) {
  Debug.print(msg);
}

void log_println(String msg) {
  Debug.println(msg);
}

/***************************************************************************************
**                      Check Wifi status
** 
***************************************************************************************/
void check_status()
{
#define WIFICHECK_INTERVAL    1000L

  static ulong checkwifi_timeout = 0;
  ulong ms = millis();

    // Check WiFi every WIFICHECK_INTERVAL (1) seconds.
    if (ms > checkwifi_timeout)
    {
    if (WiFi.status() != WL_CONNECTED)
    {
      debugE("WiFi lost. Call connectMultiWiFi in loop");
//      connectMultiWiFi();
    }

    checkwifi_timeout = ms + WIFICHECK_INTERVAL;
  }
}

/***************************************************************************************
**            Callback notifying strarting of ConfigPortal AP mode
** 
***************************************************************************************/
void apCallback(ESPAsync_WiFiManager* myWiFiManager) {

  debugD("[apCallback] - debut");

  WiFi_AP_IPConfig apIp;
  myWiFiManager->getAPStaticIPConfig(apIp);
  
  String msg = "Connectez vous au Wifi <" + myWiFiManager->getConfigPortalSSID() +">,\r\n puis a http://" +apIp._ap_static_ip.toString()
    + ",\r\n et saisissez les identifiants de votre Wifi,\r\n et les autres parametres de configuration.";

  tft.fillScreen(TFT_BLUE);
  tft.setTextColor(TFT_YELLOW);
  tft.setTextDatum(4);
  tft.drawCentreString("Mode configuration ", LCD_CENTER_X, 10, 4);

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 40);
  tft.print(msg);
}

/***************************************************************************************
**                      Callback notifying us of the need to save config
** 
***************************************************************************************/
void saveConfigCallback()
{
  debugI("Should save config");
  shouldSaveConfig = true;
}

/***************************************************************************************
**                              Initialisaton du Wifi
** 
***************************************************************************************/
void initWifi() {

  debugI("[Wifi] - Connecting to WIFI...");

  btStop();  // Stop Bluetooth
  WiFi.mode(WIFI_MODE_STA);

  AsyncWebServer webServer(80);
  AsyncDNSServer dnsServer;
  ESPAsync_WiFiManager ESPAsync_wifiManager(&webServer, &dnsServer, "Companion-IO");
  
  //ESPAsync_wifiManager.resetSettings();               // Reset settings - for testing
  ESPAsync_wifiManager.setDebugOutput(false);

  ESPAsync_wifiManager.setConfigPortalTimeout(300);     // 300s timeout before AP mode stop
  ESPAsync_wifiManager.setMinimumSignalQuality(-1);     // No min
  ESPAsync_wifiManager.setConfigPortalChannel(0);       // Use 0 => random channel from 1-13
  ESPAsync_wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  ESPAsync_wifiManager.setAPCallback(apCallback);       // Callback for User message when starting AP mode
  //ESPAsync_wifiManager.setCustomHeadElement();

  // Set config save notify callback
  ESPAsync_wifiManager.setSaveConfigCallback(saveConfigCallback);

  //+++++++++++++++++++++++++++++++++++++++++
  // Custom config
  loadFileFSConfigFile();

  // Custom html for checkbox (checked/unchecked)
  const char htmlCB[24] = "type=\"checkbox\"";
  const char htmlCB_on[24] = "type=\"checkbox\" checked";

  ESPAsync_WMParameter p_msunpv_server("msunpv_server", "MSunPV (adrs. IP)", msunpv_server.c_str(), 16);
  ESPAsync_WMParameter p_puisPV("puisPV", "Puis. Panneaux (W)", String(pCaractPV).c_str(), 6);
  ESPAsync_WMParameter p_puisBal("puisBal", "Puis. Ballon (W)", String(pCaractBallon).c_str(), 6);
  
  ESPAsync_WMParameter p_withTBal("withTBal", "Temp. Ballon",  "T", 2, (withTBal ? htmlCB_on : htmlCB), WFM_LABEL_AFTER);
  ESPAsync_WMParameter p_withVeille("withVeille", "Mode Veille", "T", 2, (withVeille ? htmlCB_on : htmlCB), WFM_LABEL_AFTER);
  ESPAsync_WMParameter p_withBatt("withBatt", "Alim par Batt", "T", 2, (withBatt ? htmlCB_on : htmlCB), WFM_LABEL_AFTER);

  ESPAsync_WMParameter p_sepMeteo("<br/><p>M&eacute;teo OpenWeatherMap");
  ESPAsync_WMParameter p_ow_latitude("ow_latitude", "Latitude", ow_latitude.c_str(), 9);
  ESPAsync_WMParameter p_ow_longitude("ow_longitude", "Longitude", ow_longitude.c_str(), 9);
  ESPAsync_WMParameter p_ow_apikey("ow_api_key", "Cl&eacute; API", ow_api_key.c_str(), 33);

  ESPAsync_WMParameter p_sepMqtt("<br/><p>HA MQTT Int&eacute;gration");
  ESPAsync_WMParameter p_withMqtt("withMqtt", "MQTT", "T", 2, (withMqtt ? htmlCB_on : htmlCB), WFM_LABEL_AFTER);
  ESPAsync_WMParameter p_mqtt_server("mqtt_server", "MQTT (adrs. IP)", mqtt_server.c_str(), 16);
  ESPAsync_WMParameter p_mqtt_user("mqtt_user", "MQTT user", mqtt_user.c_str(), 16);
  ESPAsync_WMParameter p_mqtt_pwd("mqtt_pwd", "MQTT password", mqtt_pwd.c_str(), 16);

  ESPAsync_WMParameter p_sepInflux("<br/><p>InfluxDB Int&eacute;gration");
  ESPAsync_WMParameter p_withInflux("withInflux", "InfluxDB", "T", 2, (withInfluxDB ? htmlCB_on : htmlCB), WFM_LABEL_AFTER);
  ESPAsync_WMParameter p_Influx_server("influx_server", "InfluxDB (adrs. IP)", influxDbServerUrl.c_str(), 50);
  ESPAsync_WMParameter p_Influx_org("influx_org", "Org", influxDbOrg.c_str(), 20);
  ESPAsync_WMParameter p_Influx_bucket("Influx_bucket", "Bucket", influxDbBucket.c_str(), 20);
  ESPAsync_WMParameter p_Influx_token("Influx_token", "Token", influxDbToken.c_str(), 90);


  ESPAsync_wifiManager.addParameter(&p_msunpv_server);
  ESPAsync_wifiManager.addParameter(&p_puisPV);
  ESPAsync_wifiManager.addParameter(&p_puisBal);
  
  ESPAsync_wifiManager.addParameter(&p_withTBal);
  ESPAsync_wifiManager.addParameter(&p_withVeille);
  ESPAsync_wifiManager.addParameter(&p_withBatt);
  
  ESPAsync_wifiManager.addParameter(&p_sepMeteo);
  ESPAsync_wifiManager.addParameter(&p_ow_latitude);
  ESPAsync_wifiManager.addParameter(&p_ow_longitude);
  ESPAsync_wifiManager.addParameter(&p_ow_apikey);

  ESPAsync_wifiManager.addParameter(&p_sepMqtt);
  ESPAsync_wifiManager.addParameter(&p_withMqtt);
  ESPAsync_wifiManager.addParameter(&p_mqtt_server);
  ESPAsync_wifiManager.addParameter(&p_mqtt_user);
  ESPAsync_wifiManager.addParameter(&p_mqtt_pwd);

  ESPAsync_wifiManager.addParameter(&p_sepInflux);
  ESPAsync_wifiManager.addParameter(&p_withInflux);
  ESPAsync_wifiManager.addParameter(&p_Influx_server);
  ESPAsync_wifiManager.addParameter(&p_Influx_org);
  ESPAsync_wifiManager.addParameter(&p_Influx_bucket);
  ESPAsync_wifiManager.addParameter(&p_Influx_token);

  String stored_SSID = ESPAsync_wifiManager.WiFi_SSID();
  String stored_Pass = ESPAsync_wifiManager.WiFi_Pass();
  debugD("ESP Self-Stored: SSID = %s, Pass = ", stored_SSID.c_str(), stored_Pass.c_str());
  
  if ((stored_SSID == "") || (msunpv_server == ""))
  {
    debugI("No previous AP credentials and/or config");
    initialConfig = true;
  }
/*  
  else {
    debugD(" ==> Add this SSID/PW on wifiMulti");
    wifiMulti.addAP(stored_SSID.c_str(), stored_Pass.c_str());
    ESPAsync_wifiManager.setConfigPortalTimeout(120);
  }
*/

  if (initialConfig) { 
    // Start AP mode       
    debugI("Start ConfigPortal AP mode");
    ESPAsync_wifiManager.startConfigPortal("CompanionIO", NULL);

    if (WiFi.status() == WL_CONNECTED) {
      debugE("WiFi connected... after ConfigPortal AP mode");
    }
    else {
      debugE("ERREUR - WiFi not connected after ConfigPortal AP mode");
    }
  }
  else {
    // Auto connect with multiWifi
    //debugI("ConnectMultiWiFi in setup");
    //connectMultiWiFi();

    debugI("Start autoconnect mode");
    ESPAsync_wifiManager.autoConnect("CompanionIO");       // en cas d'echec, autoconnect lance egalement le ConfigPortal (mode AP)

    if (WiFi.status() == WL_CONNECTED) {
      debugI("WiFi connected... after autoconnect");
    }
    else {
      debugE("ERREUR - WiFi not connected after autoconnect");
    }
  }

  // Save the custom parameters to FS
  if (shouldSaveConfig) {
    shouldSaveConfig = false;

    // Read updated parameters
    msunpv_server = p_msunpv_server.getValue();
    pCaractPV = String(p_puisPV.getValue()).toInt();
    pCaractBallon = String(p_puisBal.getValue()).toInt();

    withTBal = (String(p_withTBal.getValue()) == "T");
    withVeille = (String(p_withVeille.getValue()) == "T");
    withBatt = (String(p_withBatt.getValue()) == "T");
    
    ow_latitude = String(p_ow_latitude.getValue());
    ow_longitude = String(p_ow_longitude.getValue());
    ow_api_key = String(p_ow_apikey.getValue());

    withMqtt = (String(p_withMqtt.getValue()) == "T");
    mqtt_server = p_mqtt_server.getValue();
    mqtt_user = p_mqtt_user.getValue();
    mqtt_pwd = p_mqtt_pwd.getValue();

    withInfluxDB = (String(p_withInflux.getValue()) == "T");
    influxDbServerUrl = p_Influx_server.getValue();
    influxDbOrg = p_Influx_org.getValue();
    influxDbBucket = p_Influx_bucket.getValue();
    influxDbToken = p_Influx_token.getValue();

    debugD("Sauvegarde de la config : server= %s, pCaractPV= %d, pCaractBallon= %d, withTBal= %d, withVeille= %d, withBatt= %d\
      , ow_latitude= %s, ow_longitude= %s, ow_api_key= %s, withMqtt= %d, mqtt_server= %s, mqtt_user= %s, mqtt_pwd= %s\
      , withInfluxDB= %d, influxDbServerUrl= %s, influxDbOrg= %s, influxDbBucket= %s, influxDbToken= %s"
      , msunpv_server 
      , pCaractPV
      , pCaractBallon
      , withTBal
      , withVeille
      , withBatt
      , ow_latitude.c_str()
      , ow_longitude.c_str()
      , ow_api_key.c_str() 
      , withMqtt 
      , mqtt_server.c_str() 
      , mqtt_user.c_str() 
      , mqtt_pwd.c_str()
      , withInfluxDB 
      , influxDbServerUrl.c_str() 
      , influxDbOrg.c_str() 
      , influxDbBucket.c_str() 
      , influxDbToken.c_str() );

    saveFileFSConfigFile();
  }

  WiFi.mode(WIFI_MODE_STA);

  debugI("WiFi connected...yeey 2");
}

/***************************************************************************************
** Liste le contenu de LittleFS
** 
***************************************************************************************/
void listDir(const char* dirname, uint8_t levels) {
    debugD("Listing directory: %s", dirname);

    File dir = LittleFS.open(dirname);
    if (!dir || !dir.isDirectory()) {
      debugE("Failed to open directory");
      return;
    }

    File file = dir.openNextFile();
    while (file) {
      if (file.isDirectory()) {
        debugD("  DIR : %s", file.name());
        if (levels) {
          listDir(file.name(), levels -1);
        }
      } 
      else {
        debugD("  FILE: %s,  size: %d", file.name(), file.size());
      }
      file = dir.openNextFile();
    }
}

/***************************************************************************************
** Read File from LittleFS 
** 
***************************************************************************************/
String readFile(const char* filePath) {
 debugD("Reading file: %s", filePath);

  File file = LittleFS.open(filePath);
  if (!file || file.isDirectory()) {
    debugE("- failed to open file for reading");
    return String();
  }
  
  String buff;
  while (file.available()) {
    buff = file.readStringUntil('\n');
    break;     
  }
  return buff;
}

/***************************************************************************************
** Write file to LittleFS 
** 
***************************************************************************************/
void writeFile (const char *filePath, const char *data) {
  debugD("Writing file: %s", filePath);

  File fs = LittleFS.open(filePath, FILE_WRITE);
  if (!fs) {
    debugE("- failed to open file for writing");
    return;
  }

  if (!fs.print(data)) {
    debugE("- write failed");
  }

  fs.close();
}

/***************************************************************************************
** Load config file
** 
***************************************************************************************/
bool loadFileFSConfigFile() {
  debugI("Reading config file - %s", configFileName);

  if (!LittleFS.begin()) {
    debugE("ERROR - failed to mount FS");
    return false;
  }    

  File fs = LittleFS.open(configFileName, "r");
  if (!fs) {
    debugE("ERROR - Failed to open config file for reading");
    return false;
  }

  JsonDocument doc;
  auto err = deserializeJson(doc, fs.readString());
  if (err) {
    debugE("deserializeJson failed");
    fs.close();
    return false;
  }

  if (doc["msunpv_server"]) msunpv_server = doc["msunpv_server"].as<String>();
  if (doc["pMaxPanneaux"]) pCaractPV = doc["pMaxPanneaux"];
  if (doc["pMaxBallon"]) pCaractBallon = doc["pMaxBallon"];

  if (doc["withTBal"]) withTBal = doc["withTBal"];
  if (doc["withVeille"]) withVeille = doc["withVeille"];
  if (doc["withBatt"]) withBatt = doc["withBatt"];

  if (doc["ow_latitude"]) ow_latitude = doc["ow_latitude"].as<String>();
  if (doc["ow_longitude"]) ow_longitude = doc["ow_longitude"].as<String>();
  if (doc["ow_api_key"]) ow_api_key = doc["ow_api_key"].as<String>();

  if (doc["withMqtt"]) withMqtt = doc["withMqtt"];
  if (doc["mqtt_server"]) mqtt_server = doc["mqtt_server"].as<String>();
  if (doc["mqtt_user"]) mqtt_user= doc["mqtt_user"].as<String>();
  if (doc["mqtt_pwd"]) mqtt_pwd = doc["mqtt_pwd"].as<String>();

  if (doc["withInfluxDB"]) withInfluxDB = doc["withInfluxDB"];
  if (doc["influxDbServerUrl"]) influxDbServerUrl = doc["influxDbServerUrl"].as<String>();
  if (doc["influxDbOrg"]) influxDbOrg = doc["influxDbOrg"].as<String>();
  if (doc["influxDbBucket"]) influxDbBucket = doc["influxDbBucket"].as<String>();
  if (doc["influxDbToken"]) influxDbToken = doc["influxDbToken"].as<String>();

  fs.close();

  debugD("Lecture config OK : msunpv_server= %s, pCaractPV= %d, pCaractBallon= %d, withTBal= %d, withVeille= %d, withBatt= %d\
    , ow_latitude= %s, ow_longitude= %s, ow_api_key= %s, withMqtt= %d, mqtt_server= %s, mqtt_user= %s, mqtt_pwd= %s\
    , withInfluxDB= %d, influxDbOrg= %s, influxDbBucket= %s, influxDbToken= %s"
    , msunpv_server.c_str() 
    , pCaractPV
    , pCaractBallon
    , withTBal
    , withVeille
    , withBatt
    , ow_latitude.c_str()
    , ow_longitude.c_str()
    , ow_api_key.c_str() 
    , withMqtt 
    , mqtt_server.c_str() 
    , mqtt_user.c_str() 
    , mqtt_pwd.c_str() 
    , withInfluxDB 
    , influxDbOrg.c_str() 
    , influxDbBucket.c_str() 
    , influxDbToken.c_str() );

  return true;
}

/***************************************************************************************
** Save config file
** 
***************************************************************************************/
bool saveFileFSConfigFile() {
  debugI("Saving config to file : %s", configFileName);

  File fs = LittleFS.open(configFileName, "w");
  if (!fs) {
    debugE("ERROR - Failed to open config file for writing");
    return false;
  }

  JsonDocument json;
  json["msunpv_server"] = msunpv_server;
  json["pMaxPanneaux"] = pCaractPV;
  json["pMaxBallon"] = pCaractBallon;

  json["withTBal"] = withTBal;
  json["withVeille"] = withVeille;
  json["withBatt"] = withBatt;

  json["ow_latitude"] = ow_latitude;
  json["ow_longitude"] = ow_longitude;
  json["ow_api_key"] = ow_api_key;

  json["withMqtt"] = withMqtt;
  json["mqtt_server"] = mqtt_server;
  json["mqtt_user"] = mqtt_user;
  json["mqtt_pwd"] = mqtt_pwd;

  json["withInfluxDB"] = withInfluxDB;
  json["influxDbServerUrl"] = influxDbServerUrl;
  json["influxDbOrg"] = influxDbOrg;
  json["influxDbBucket"] = influxDbBucket;
  json["influxDbToken"] = influxDbToken;

  debugD("saveFileFSConfigFile - serialyze");
  serializeJsonPretty(json, Serial);    // debug

  // Write data to file and close it
  serializeJson(json, fs);
  debugD("saveFileFSConfigFile - serialyze OK");
  fs.close();
  
  return true;
}

/***************************************************************************************
** Load Luminosité config file
** 
***************************************************************************************/
void loadLumiParam() {

  if (!LittleFS.begin()) {
    debugE("ERROR - failed to mount FS");
    return;
  }

  File fs = LittleFS.open(lumiConfigFileName, "r");
  if (!fs) {
    debugE("ERROR - Failed to open lumi.json file for reading");
    return;
  }

  JsonDocument doc;
  auto err = deserializeJson(doc, fs.readString());
  if (err) {
    debugE("deserializeJson failed");
  }
  else {
    if (doc["lumi"]) backligthLevel = doc["lumi"].as<int>();
  }

  fs.close();
}

/***************************************************************************************
** Save Luminosité config file
** 
***************************************************************************************/
void saveLumiParam() {
  File fs = LittleFS.open(lumiConfigFileName, "w");
  if (!fs) {
    debugE("ERROR - Failed to open lumi.json file for writing");
    return;
  }

  JsonDocument json;
  json["lumi"] = backligthLevel;

  // Write data to file and close it
  serializeJson(json, fs);
  fs.close();
  return;
}

/***************************************************************************************
** 
** 
***************************************************************************************/
