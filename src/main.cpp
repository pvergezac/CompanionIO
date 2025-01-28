#include <Arduino.h>

const String TITRE = "MSunPV Companion_NG";
const String TITRE_LONG = "Companion_NG (Nouvelle Génération)";
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
#include "NTP_Time.h"               // Attached to this sketch, see that tab for library needs
#include <esp_task_wdt.h>           // watchdog en cas de déconnexion ou blocage
#include <TFT_eSPI.h>
#include <OneButton.h>              // OneButton par Matthias Hertel (dans le gestionnaire de bibliothèque)
#include <ArduinoJson.h>
#include <OpenWeather.h>            // https://github.com/Bodmer/OpenWeather
#include <InfluxDbClient.h>         // InfluxDB
#include <InfluxDbCloud.h>          // 
#include <PubSubClient.h>           // Librairie pour la gestion Mqtt

// Additional functions
#include "GfxUi.h"                  // Attached to this sketch
#include "StreamUtils.h"
#include "time.h"

//+++++++++++++++++++++++++++++++
#include "perso.h"                  // Données personnelles à modifier dans le fichier (voir en haut de cet écran)
#include "logo.h"                   // Logo de départ
#include "images.h"                 // Images affichées sur l'écran
#include "meteo.h"                  // Icones météo

#define LCD_WIDTH  320
#define LCD_HEIGHT 170
#define LCD_CENTER_X (LCD_WIDTH /2)
#define LCD_CENTER_Y (LCD_HEIGHT /2)

#define AA_FONT_SMALL "fonts/NSBold15" // 15 point Noto sans serif bold
#define AA_FONT_LARGE "fonts/NSBold36" // 36 point Noto sans serif bold
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
#define FOND_BLEU TFT_NAVY  // auparavant 0x0017
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
int backlight[6] = {0, 10, 30, 60, 120, 255};
int backligthLevel = 3;         // Luminosité ecran

#define TEMPS_VEILLE (5* 60 * 1000)     // Veille apres 60s
bool veille_on = false;                 // veille encour
ulong veille_deb;                       // temps de debut de mise en veille

//+++++++++++++++++++++++++++++++++++
// Update toutes les 15 minutes, jusqu'à 1000 requêtes par jour gratuit (soit ~40 par heure)
#define UPDATE_TIME_INTERVAL   (1*1000)        // 1 secondes
#define UPDATE_MSUNPV_INTERVAL (15*1000)       // 15 secondes
#define UPDATE_METEO_INTERVAL  (15*60*1000)    // 15 minutes    (1000 requêtes/jour gratuit)


//+++++++++++++++++++++++++++++++++++
#define INFLUXDB_URL "https://influxdb.pvergezac.ovh"
#define INFLUXDB_TOKEN "X-1zp9i_mL9Wi4wzIYWJMWcKKlKX8toAWzHC4GYTZs59xaARE3T2CfcZNaNxVdEG0-fGZKCYiDMHk0xmaXdzDg=="
#define INFLUXDB_ORG "042b7e8882771b98"
#define INFLUXDB_BUCKET "test1"

// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Declare Data point
Point sensor("measurements");
  
//+++++++++++++++++++++++++++++++++++
// MQTT
bool withMqtt = false;
WiFiClient clientMQTT;
PubSubClient mqttClient(clientMQTT);
int mqttCounterConn = 0;      // connections counter

// Broker MQTT (Home Assistant) adrs et login / password
String mqtt_server     = "192.168.0.200";
String mqtt_user       = "admin";
String mqtt_pwd        = "Qq123456+1";

const char* topicHAStatus = "homeassistant/status";
const String base_topic = "homeassistant/sensor/";
const String uidPrefix = "msunpv-comp";       // Prefix for unique ID generation (limit to 20 chars)
String devUniqueID;                           // Generated Unique ID for this device (uidPrefix + last 6 MAC characters) 


//+++++++++++++++++++++++++++++++++++
// Gestion des pages écran
#define PAGE_DEFAULT  0
#define PAGE_METEO    1
#define PAGE_METEO2   2
#define PAGE_SOLAR    3
#define PAGE_TEMPO    4
#define PAGE_CUMULS   5
#define PAGE_LUMI     6
#define PAGE_LAST     PAGE_LUMI
#define DUREE_AFF     (5* 60 * 1000)            // Durée affichage page, avant retour à la page par défaut

int page = PAGE_DEFAULT;                      // Page courante affichée
ulong tempsAffPage = 0;                       // Temps d'affichage page, avant retour à la page par défaut

bool updateAff = false;
bool firstAff = true;

//+++++++++++++++++++++++++++++++++++
ESPAsync_WiFiManager *wm;
AsyncWebServer webserver(80);                    //  Serveur web on port 80
//DNSServer dns;

//+++++++++++++++++++++++++++++++++++
const char* configFileName = "/config.json";
bool shouldSaveConfig = false;

//+++++++++++++++++++++++++++++++++++
OneButton boutonBacklight(topbutton, true);   // Bouton éclairage
OneButton boutonPage(lowerbutton, true);      // Bouton de selection de page
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);       // Tout l'écran

// Orientation affichage - (3 pour prise à gauche, 1 pour prise à droite)
const int rotation = 3;

//+++++++++++++++++++++++++++++++++++
String msunpv_server;                         // Adresse du routeur MSunPV

// Seuil minimum d'affichage des mesures PV et conso (en W)
const int residuel = 10;

int pCaractPV = 3000;           // puissance Crete installé des panneaux solaire en watt
int pCaractBallon = 2500;       // puissance du ballon cumulus en watt

bool withBatt = true;           // Alimentation par batterie
bool withRad = true;            // Radiateurs électriques
bool withTBal = true;           // Sonde Température installé sur le cumulus
bool withVeille = true;         // Mise en veille affichage

// Variables affichant les valeurs reçues depuis le MSunPV
int powpv = 0;            // Puissance panneaux (W)
int powreso = 0;          // Puissance consomé/injectée (W)
int outbal = 0;           // Dimmer ballon (0 - 100%)
int outrad =0;            // Dimmer radiateur
int powbal =0;            // Puissance injection ballon, recalculée a partir de dimmer_ballon
int voltres =0;           // Tension réseau (V)

float tbal = 0.;          // Température ballon (°C)
float tsdb = 0.;          // Température Salle de bain (°C)
float tamb = 0.;          // Température ambiante (°C)

float cumulConso = 0.;    // Cumul Consomation
float cumulInj = 0.;      // Cumul Injection
float cumulPV_J = 0.;     // Cumul Panneaux
float cumulPV_P = 0.;     // Cumul Ballon cumulus

float vBatt;              // Voltage batterie

//+++++++++++++++++++++++++++++++++++
// Données de openweather
#define TIMEZONE euCET    // Voir NTP_Time.h tab pour d'autres "Zone references", UK, usMT etc
OW_Weather ow;            // Weather forecast librairie instance

// Localisation de votre ville pour la météo et horaires de levé et couché de soleil
String ow_latitude  = "";           // 90.0000 to -90.0000 negative for Southern hemisphere
String ow_longitude = "";           // 180.000 to -180.000 negative for West
String ow_api_key = "";
String ow_units = "metric";  // ou "imperial"
String ow_language = "fr";

String lever = "", coucher = "", icone = "", ID = "";
float tempExt = 0.;

struct MeteoData {
  String   dt = "";
  String   lever="";
  String   coucher="";

  float    temp = 0;
  float    feels_like = 0;
  float    pressure = 0;
  uint8_t  humidity = 0;
  //float    dew_point = 0;

  uint8_t  clouds = 0;
  //float    uvi = 0;
  uint32_t visibility = 0;

  float    wind_speed = 0;
  float    wind_gust = 0;
  uint16_t wind_deg = 0;

  float    rain = 0;

  // current.weather
  String   main="";
  String   description="";
  String   icone="";
  String   name="";
  String   ID="";
};

MeteoData *myMeteo = new MeteoData;

//+++++++++++++++++++++++++++++++++++
// Données Météo Solaire
int forecast_watts[24];
int forecast_wattshours[24];
time_t forecast_times[24];
int forecast_size = 0;
int forecast_whday[2] = {0,0};
int ratelimit_remaining = 0;

//+++++++++++++++++++++++++++++++++++
// Données Tempo
const uint16_t tempo_colors[] = {TFT_BLACK, TFT_BLUE, TFT_WHITE, TFT_RED};
const String tempo_str[] = {"??", "Bleu", "Blanc", "Rouge"};
int codeTempoJ = 0; 
int codeTempoJ1 = 0;

//===============================================================
// Prototypes des fonctions
void log_print(String msg);
void log_println(String msg);
void init_wifi();
void handleClick();
void doubleClick();
void handlePage();
void handlePageDef();
void requestMeteo();
void requestMeteo2();
void requestMeteo3();
bool requestMSunPV(ulong ms);
void requestTempo(ulong ms);
void reqMeteoSolaire(ulong ms);
void doAffichage();
void AffichageDefault();
void AffichageCumuls();
void AfficheTempo();
void AfficheMeteo();
void AfficheMeteo2();
void AfficheSolar();
void AfficheLumi();
String strTime(time_t unixTime);
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
  ulong ms = millis() + 2000;           // timeout 2s, pour eviter le blocage si port serie non connecté
  while (!Serial && (millis() < ms)) {
    delay(10);                          // wait for serial port to connect. Needed for native USB port only
  }

  //++++++++++++++++++++++++++++++++++++++++++++++++
  log_println("[Init] - " +TITRE +" - V" + Version);
  log_println("[Init] - Wake up : " + String(source_reveil));

  //++++++++++++++++++++++++++++++++++++++++++++++++
  if(!LittleFS.begin(true)){
    log_println("An Error has occurred while mounting LittleFS");
    while (true) {
      delay(10);
    }
  }

  drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);
  if (drd->detectDoubleReset()) {
    initialConfig = true;
  }

  listDir("/", 0);
  log_println("[LittleFS] - totalBytes: " +String(LittleFS.totalBytes()) +", usedBytes: " +String(LittleFS.usedBytes()));
  //log_println("read wm_config.dat - [" +readFile("/wm_config.dat") +"]");

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
  init_wifi();

  //if you get here you have connected to the WiFi
  log_println("[Wifi] - Connected...yeey : local ip : " +WiFi.localIP().toString());

  // Affichage de la connexion sur écran de départ
  tft.setTextColor(TFT_BLUE, TFT_WHITE);
  tft.setTextDatum(4);
  tft.drawCentreString(WiFi.localIP().toString() +"  " + String(WiFi.RSSI()) +"dB", LCD_CENTER_X, 136, 4);
  delay(1000);

  //++++++++++++++++++++++++++++++++++++++++++++++++
  // Initialisation du Watchdog
  log_println("[Init] - Configuraton du WDT...");

  esp_task_wdt_deinit();                                // MODIFICATION POUR VERSION ESP32 3.0.5 le watchdog est activé par défaut ; il faut d'abord le désactiver avant de le configurer
  int err_code = esp_task_wdt_init(WDT_TIMEOUT, true);  //enable panic so ESP32 restarts
  if (err_code != 0) 
    log_println("[Init] - esp_task_wdt_init error: " + err_code);

  esp_task_wdt_add(NULL);  //add current thread to WDT watch

  //++++++++++++++++++++++++++++++++++++++++++++++++
  // Synchronisation de l'heure (NTP)
  log_println("[Init] - Synchronisation de l'heure (NTP)");
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
  // Check InfluxDb connection
  if (client.validateConnection()) {
    String msg = "Connected to InfluxDB: " + client.getServerUrl();
    log_println(msg);
  } else {
    String msg = "InfluxDB connection failed: " + client.getLastErrorMessage();
    log_println(msg);
  }

  // Add tags to the data point
  sensor.addTag("device", "ESP32");
  sensor.addTag("SSID", WiFi.SSID());

  //++++++++++++++++++++++++++++++++++++++++++++++++
  // Activation du mDNS responder: encore idée de Bellule
  // Utiliser l'url : http://companion pour y accéder
  if (!MDNS.begin("companion")) {
    log_println("[Init] - Error setting up MDNS responder!");
    while (1) { delay(1000); }                                // endless loop
  }
  // Ajout du service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
  log_println("[Init] - mDNS responder started");

  //++++++++++++++++++++++++++++++++++++++++++++++++
  // Configuration du broker MQTT (Home Assistant)
  if (withMqtt) {
    mqttClient.setBufferSize(512);
    mqttClient.setServer(mqtt_server.c_str(), 1883);
    mqttClient.setCallback(mqttReceiverCallback);

    byte mac[6];
    WiFi.macAddress(mac);
    //devUniqueID = uidPrefix +String(mac[0], HEX) +String(mac[1], HEX) +String(mac[2], HEX) +String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
    devUniqueID = uidPrefix +String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);

    mqttReconnect();
  }

  //++++++++++++++++++++++++++++++++++++++++++++++++
  veille_deb = millis() + TEMPS_VEILLE;

  esp_task_wdt_reset();

  log_println("Setup ended.");
}

/***************************************************************************************
**                                Routine LOOP 
** 
***************************************************************************************/
void loop() {
  static ulong nextWeatherUpdate = 0UL;

  //+++++++++++++++++++++++++++++++
  // Gestion MQTT
  if(withMqtt && (WiFi.status() == WL_CONNECTED)) {
    if(!mqttClient.connected()) {
      mqttReconnect();                  // Force la reconnexion au broker MQTT en cas de perte
    }
    else {
      mqttClient.loop();
    }
  }

  //+++++++++++++++++++++++++++++++
  // Gestion des boutons
  boutonBacklight.tick();
  boutonPage.tick();
  drd->loop();                        // Gestion double reset

  ulong ms = millis();

  // si le Wifi est connecté uniquement
  if (WiFi.status() == WL_CONNECTED) {        

    // Requète des données Météo (15mn)
    if (ms > nextWeatherUpdate) {
      nextWeatherUpdate = ms + UPDATE_METEO_INTERVAL;

      requestMeteo();               // Requète la Météo
      requestMeteo2();
      requestMeteo3();
      syncTime();                   // Synchronisation de l'heure systeme via NTP
      readVbatt();                  // Lecture tension batterie
      updateAff = true;
    }

    // Requete TEMPO
    requestTempo(ms);

    // Requete Previ Meteo Solaire
    reqMeteoSolaire(ms);

    // Requète des données MSunPV (15sec)
    bool isUpdate = requestMSunPV(ms);
    if (isUpdate) {
      updateAff = true;

      //+++++++++++++++++++++++
      // Envoi des mesures vers InfluxDB
      sendInfluxDB();

      //++++++++++++++++++++++++++++++++++++++++++
      // Gestion MQTT - Envoi des mesures en Json
      if (withMqtt)
        mqttSendMesures();
    }
  } 
 
  doAffichage();

  // reset watchdog uniquement si WIFI OK
  esp_task_wdt_reset();
}


void sendInfluxDB() {
  // Store measured value into point
  // Report RSSI of currently connected network
  sensor.addField("rssi", WiFi.RSSI());

  sensor.addField("puis_panneaux", powpv);                    // W
  sensor.addField("puis_conso", powreso);                     // W
  sensor.addField("dimmer_ballon", outbal);                   // %
  sensor.addField("puis_ballon", powbal);                     // W
  sensor.addField("temp_ballon", tbal);                       // °C
  
  sensor.addField("cumul_conso", cumulConso);                 // W
  sensor.addField("cumul_inj", cumulInj);                     // W
  sensor.addField("cumul_conso", cumulPV_J);                  // W
  
  sensor.addField("previ_whday0", forecast_whday[0]);         // W
  sensor.addField("previ_whday1", forecast_whday[1]);         // W

  sensor.addField("couleur_tempo", codeTempoJ);               // 0 - 3
  sensor.addField("couleur_tempo2", codeTempoJ1);

  sensor.addField("temp", myMeteo->temp);
  sensor.addField("hum", myMeteo->humidity);                  // %
  sensor.addField("clouds", myMeteo->clouds);                 // %
  sensor.addField("wind_speed", myMeteo->wind_speed * 3.6);   // km/h
  sensor.addField("wind_gust", myMeteo->wind_gust * 3.6);     // km/h
  sensor.addField("wind_deg", myMeteo->wind_deg);
  sensor.addField("pressure", myMeteo->pressure);             // pa
  sensor.addField("rain", myMeteo->rain);                     // mm/h ou l/m2 pat heure
  sensor.addField("visibility", myMeteo->visibility);

  //log_println("InfluxDb write - " +sensor.toLineProtocol());

  // Write point
  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  // Clear fields for reusing the point. Tags will remain the same as set above.
  sensor.clearFields();
}


// Variable used for MQTT Discovery
const String mqtt_deviceName  = "CompanionIO";                           // Device name

const char* mqtt_deviceModel = "MSunPV Companion";                 // Hardware model
const char* mqtt_swVersion   = "1.0";                              // Firmware version
const char* mqtt_manufacturer = "DIY Pzac";                        // Manufecturer name
const String mqtt_statusTopic = "esp32iotsensor/" + mqtt_deviceName;     // MQTT topic

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
    doc["tbal"]= String(tbal);
    doc["enconso"]= String(cumulConso);
    doc["eninj"]= String(cumulInj);
    doc["enpvj"]= String(cumulInj);
    doc["enpvp"]= String(cumulPV_P);

    serializeJson(doc, buff);
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

      Serial.println("[MQTT] - Connection au broker !");

      if (mqttClient.connect(mqtt_deviceName.c_str(), mqtt_user.c_str(), mqtt_pwd.c_str())) {
        Serial.println("[MQTT] - Reconnecté au broker !");
        delay(100);

        mqttHaDiscovery();     // Send Discovery Data

        mqttClient.subscribe(topicHAStatus);
        delay(100);
        mqttCounterConn=0;
      }
      else {
        Serial.print(F("[MQTT] - Broker MQTT indisponible. Status= "));
        Serial.println(mqttClient.state());
      }
    }
  }
}

/*****************************************************************************************
**                      MQTT Home Assistant Discovery                                   **
*****************************************************************************************/
void mqttHaDiscovery()
{
  if(withMqtt && mqttClient.connected())
  {
    log_println("[MQTT] - Send Discovery !!!");

    JsonDocument payload;
    JsonObject device;
    JsonArray identifiers;
    String buf;
    String topic;
    String id;
    String st;
 
    //----------------------------------------------------------------
    id = devUniqueID + "_powpv";
    topic = base_topic + id + "/config";
    st = base_topic + devUniqueID + "/state";
    
    payload["name"] = "MSunPV-PowPV";
    payload["uniq_id"] = id;
    payload["state_topic"] = st;
    payload["dev_cla"] = "power";
    payload["unit_of_meas"] = "W";
    payload["val_tpl"] = "{{ value_json.powpv | is_defined }}";

    device = payload.createNestedObject("device");
    device["name"] = mqtt_deviceName;
    device["model"] = mqtt_deviceModel;
    device["sw_version"] = mqtt_swVersion;
    device["manufacturer"] = mqtt_manufacturer;
    identifiers = device.createNestedArray("identifiers");
    identifiers.add(devUniqueID);

    serializeJson(payload, buf);
    log_println("HA - Topic= " +topic);
    log_println("HA - Payload= " +buf);
    mqttClient.publish(topic.c_str(), buf.c_str());

    //----------------------------------------------------------------
    id = devUniqueID + "_powreso";
    topic = base_topic + id + "/config";
    st = base_topic + devUniqueID + "/state";
    
    payload.clear();
    payload["name"] = "MSunPV-PowReso";
    payload["uniq_id"] = id;
    payload["state_topic"] = st;
    payload["dev_cla"] = "power";
    payload["unit_of_meas"] = "W";
    payload["val_tpl"] = "{{ value_json.powreso | is_defined }}";

    device = payload.createNestedObject("device");
    identifiers = device.createNestedArray("identifiers");
    identifiers.add(devUniqueID);

    serializeJson(payload, buf);
    log_println("HA - Topic= " +topic);
    log_println("HA - Payload= " +buf);
    mqttClient.publish(topic.c_str(), buf.c_str());

    //----------------------------------------------------------------
    id = devUniqueID + "_outbal";
    topic = base_topic + id + "/config";
    st = base_topic + devUniqueID + "/state";
    
    payload.clear();
    payload["name"] = "MSunPV-OutBal";
    payload["uniq_id"] = id;
    payload["state_topic"] = st;
    //payload["dev_cla"] = "power";
    payload["unit_of_meas"] = "%";
    payload["val_tpl"] = "{{ value_json.outbal | is_defined }}";

    device = payload.createNestedObject("device");
    identifiers = device.createNestedArray("identifiers");
    identifiers.add(devUniqueID);

    serializeJson(payload, buf);
    log_println("HA - Topic= " +topic);
    log_println("HA - Payload= " +buf);
    mqttClient.publish(topic.c_str(), buf.c_str());

    //----------------------------------------------------------------
    id = devUniqueID + "_tbal";
    topic = base_topic + id + "/config";
    st = base_topic + devUniqueID + "/state";

    payload.clear();
    payload["name"] = "MSunPV-TBal";
    payload["uniq_id"] = id;
    payload["state_topic"] = st;
    payload["dev_cla"] = "temperature";
    payload["unit_of_meas"] = "°C";
    payload["val_tpl"] = "{{ value_json.tbal | is_defined }}";

    device = payload.createNestedObject("device");
    identifiers = device.createNestedArray("identifiers");
    identifiers.add(devUniqueID);

    serializeJson(payload, buf);
    log_println("HA - Topic= " +topic);
    log_println("HA - Payload= " +buf);
    mqttClient.publish(topic.c_str(), buf.c_str());

    //----------------------------------------------------------------
    id = devUniqueID + "_enconso";
    topic = base_topic + id + "/config";
    st = base_topic + devUniqueID + "/state";
    
    payload.clear();
    payload["name"] = "MSunPV-EnConso";
    payload["uniq_id"] = id;
    payload["state_topic"] = st;
    payload["dev_cla"] = "energy";
    payload["unit_of_meas"] = "Wh";
    payload["val_tpl"] = "{{ value_json.enconso | is_defined }}";

    device = payload.createNestedObject("device");
    identifiers = device.createNestedArray("identifiers");
    identifiers.add(devUniqueID);

    serializeJson(payload, buf);
    log_println("HA - Topic= " +topic);
    log_println("HA - Payload= " +buf);
    mqttClient.publish(topic.c_str(), buf.c_str());

    //----------------------------------------------------------------
    id = devUniqueID + "_eninj";
    topic = base_topic + id + "/config";
    st = base_topic + devUniqueID + "/state";
    
    payload.clear();
    payload["name"] = "MSunPV-EnInj";
    payload["uniq_id"] = id;
    payload["state_topic"] = st;
    payload["dev_cla"] = "energy";
    payload["unit_of_meas"] = "Wh";
    payload["val_tpl"] = "{{ value_json.eninj | is_defined }}";

    device = payload.createNestedObject("device");
    identifiers = device.createNestedArray("identifiers");
    identifiers.add(devUniqueID);

    serializeJson(payload, buf);
    log_println("HA - Topic= " +topic);
    log_println("HA - Payload= " +buf);
    mqttClient.publish(topic.c_str(), buf.c_str());

    //----------------------------------------------------------------
    id = devUniqueID + "_enpvj";
    topic = base_topic + id + "/config";
    st = base_topic + devUniqueID + "/state";
    
    payload.clear();
    payload["name"] = "MSunPV-EnPV_J";
    payload["uniq_id"] = id;
    payload["state_topic"] = st;
    payload["dev_cla"] = "energy";
    payload["unit_of_meas"] = "Wh";
    payload["val_tpl"] = "{{ value_json.enpvj | is_defined }}";

    device = payload.createNestedObject("device");
    identifiers = device.createNestedArray("identifiers");
    identifiers.add(devUniqueID);

    serializeJson(payload, buf);
    log_println("HA - Topic= " +topic);
    log_println("HA - Payload= " +buf);
    mqttClient.publish(topic.c_str(), buf.c_str());

    //----------------------------------------------------------------
    id = devUniqueID + "_enpvp";
    topic = base_topic + id + "/config";
    st = base_topic + devUniqueID + "/state";
    
    payload.clear();
    payload["name"] = "MSunPV-EnPV_P";
    payload["uniq_id"] = id;
    payload["state_topic"] = st;
    payload["dev_cla"] = "energy";
    payload["unit_of_meas"] = "kWh";
    payload["val_tpl"] = "{{ value_json.enpvp | is_defined }}";

    device = payload.createNestedObject("device");
    identifiers = device.createNestedArray("identifiers");
    identifiers.add(devUniqueID);

    serializeJson(payload, buf);
    log_println("HA - Topic= " +topic);
    log_println("HA - Payload= " +buf);
    mqttClient.publish(topic.c_str(), buf.c_str());
  }
}

/*****************************************************************************************
**                                   Routine callback                                   **
*****************************************************************************************/
void mqttReceiverCallback(char* topic, byte* inFrame, unsigned int length) 
{
    Serial.print("MQTT - Message arrived on topic: ");
    Serial.print(topic);
    Serial.print(". Message: ");
    byte state = 0;
    String messageTemp;
    
    for (int i = 0; i < length; i++) {
        Serial.print((char)inFrame[i]);
        messageTemp += (char)inFrame[i];
    }
    Serial.println();

    if(strcmp(topicHAStatus, topic) == 0) {
      if(messageTemp == "online") {
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
String processor(const String& var){
  //log_println(var);
  if(var == "TITRE_LONG"){
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
    //log_println("doAffichage");

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

  //test(); // teste les affichages

  //  Dessin fenêtre principale
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);

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
    sprite.setTextColor(TFT_CYAN, TFT_BLACK);   // Rique de gel, on affiche en bleu clair
  }
  sprite.setTextDatum(MR_DATUM);                // centre droit
  valStr = String(tempExt, 1) + "`C";           // °C (probleme de police TFT_eSPY)
  sprite.drawString(valStr, 320, 160, 2);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++
  // Affichage heure et date
  sprite.drawRoundRect(234,  0, 86, 54, RECT_RADIUS, TFT_WHITE);
  sprite.setTextDatum(MC_DATUM);  // retour au centre milieu
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);  // Retour texte normal
  time_t timeNow = now();
  String timeStr = strTime(timeNow);                              // La convertion en local est faite au formatage
  time_t local_time = TIMEZONE.toLocal(timeNow, &tz1_Code);       // Convert UTC to local time, returns zone code in tz1_Code, e.g "GMT"
  String dateStr = String(day(local_time)) + " " +String(Months[month(local_time)]);
  sprite.drawString(timeStr, 277, 20, 4);
  sprite.drawString(dateStr, 277, 42, 2);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++
  // Affichage de la température Ballon (si withTBal validée)
  if (withTBal && (tbal >= 1)) {

    uint16_t couleur;
    if (tbal >= 60)      couleur = color5;
    else if (tbal >= 50) couleur = color4;
    else if (tbal >= 40) couleur = color8;
    else                        couleur = color1;
    sprite.fillCircle(27, 84, 24, couleur);

    sprite.setTextColor(TFT_WHITE);             // Retour texte normal
    sprite.setTextDatum(MC_DATUM);              // retour au centre milieu
    //sprite.drawString(String(temp_ballon, 0) + "`", 26, 85, 2);   //°C (probleme de police TFT_eSPY)
    sprite.drawString(String(tbal, 0) + "`", 26, 80);   //°C (probleme de police TFT_eSPY)
  }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++
  // Affichage des valeurs des compteurs
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);  // Retour texte normal
  sprite.setFreeFont(&Orbitron_Light_24);  // police d'affichage
  //sprite.setFreeFont(&Roboto_Thin_24);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++
  // Affichage Panneaux  Solaire
  sprite.drawRoundRect(0, 0, 226, 55, RECT_RADIUS, TFT_WHITE);    // Entourage de la zone
  sprite.drawString("PANNEAUX PV", 115, 10, 2);                   // Titre de la zone

  if (powpv >= residuel) {
    valStr = String(powpv) + " W";
    sprite.drawString(valStr, 115, 35);
  } else {
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
  sprite.fillRoundRect(0, 114, 226, 55, RECT_RADIUS, couleurfond);  // Zone Conso
  sprite.drawRoundRect(0, 114, 226, 55, RECT_RADIUS, couleurtexte);  // Zone Conso
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
    puis_disp = -1 * powreso;                                  // Ballon plein, coupé par thermostat mécanique
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
  if (withRad && ((powreso + powpv) > 3000) && (powbal < 100))   // Si forte conso et ballon coupé
  {
    sprite.pushImage(6, 122, 40, 40, chauffage);
  }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++
  // Affichage des graphes latéraux
  int nbbarres;

  //++++++++++++++++++++++++++++++++++++++++
  // Bargraphe Panneaux PV
  if (powpv > residuel) {
    nbbarres = (8 * powpv) / pCaractPV;
    barregraphV(200, 50, nbbarres, 8, 20, 5, 1, color1);    // en vert
  }

  //++++++++++++++++++++++++++++++++++++++++
  // Bargraphe Cumulus
  nbbarres = (8 * outbal) / 100;                     // puis_balllon est en pourcent (0-100%)
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

  sprite.drawString("Temp.", x1, yy, 2);  sprite.drawString(String(myMeteo->temp,1 ) +"`C", x2, yy, 2);   
    sprite.drawString("Humi", x3, yy, 2); sprite.drawString(String(myMeteo->humidity) +"%", x4, yy, 2);
    yy +=16;

  sprite.drawString("Vent", x1, yy, 2);   sprite.drawString(String(myMeteo->wind_speed, 1) +"/" +String(myMeteo->wind_gust, 1), x2, yy, 2);
    sprite.drawString("Dir", x3, yy, 2);  sprite.drawString(String(myMeteo->wind_deg) +"`", x4, yy, 2);
    yy +=16;

  sprite.drawString("Pres", x1, yy, 2);   sprite.drawString(String(myMeteo->pressure, 0)+"hPa", x2, yy, 2);
    yy +=16;

  sprite.drawString("Nuage", x1, yy, 2);   sprite.drawString(String(myMeteo->clouds), x2, yy, 2);
    sprite.drawString("Pluie", x3, yy, 2); sprite.drawString(String(myMeteo->rain, 1), x4, yy, 2);
    yy +=16;

  //sprite.drawString("Fiab.", x1, yy, 2);  sprite.drawString(String(meteoFeels, 1), x2, yy, 2);
  //  yy +=14;

  sprite.pushSprite(0, 0);
}


const char* getMeteoconIcon(uint16_t id, bool today)
{
  if ( today && (id/100 == 8) 
    && (myMeteo->dt < myMeteo->lever || myMeteo->dt > myMeteo->coucher)) 
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
  sprite.drawString(String(myMeteo->temp, 1) +"°C", x1, yy);

  // Humidité
  sprite.setTextDatum(BR_DATUM);
  sprite.drawString("H " +String(myMeteo->humidity) +"%", x2, yy);
  yy+=34;

  // Nébulosité
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString("Neb " +String(myMeteo->clouds) +"%", x1, yy);
  yy+=34;

  // Pluie
  sprite.drawString("P " +String(myMeteo->rain, 1) +"mm/h", x1, yy);
  yy+=34;

  // Pression
  sprite.drawString(String(myMeteo->pressure, 0) +"hPa", x1, yy);
  yy+=34;

  // Vent
  sprite.drawString(String(myMeteo->wind_speed * 3.6, 0) +"km/h", x1, yy);
  sprite.setTextDatum(BR_DATUM);
  sprite.drawString(String(myMeteo->wind_deg) +"°", x2, yy);

  //sprite.setTextColor(TFT_ORANGE, TFT_BLACK);

  // Icone
  String weatherIcon = getMeteoconIcon(myMeteo->ID.toInt(), true);
  ui.drawBmp(&sprite, "/icon/" + weatherIcon + ".bmp", 219, 35);

/*
  // Description
  sprite.loadFont(AA_FONT_SMALL2, LittleFS);
  sprite.setTextDatum(BR_DATUM);
  String weatherText = myMeteo->description;
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
    log_println ("pCaractPV = 0");
    return;
  }

  if (forecast_size == 0) {
    log_println ("forecast_size = 0");
    sprite.setTextDatum(BC_DATUM);
    sprite.drawString("No data", LCD_CENTER_X, LCD_CENTER_Y, 4);
    sprite.pushSprite(0, 0);
    return;
  }

  // Graduations verticale tous les 1Kw
  int kw = pCaractPV/1000;
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

  sprite.drawString(String(ratelimit_remaining), 319, 35, 2);

  sprite.pushSprite(0, 0);
}

/***************************************************************************************
** Affichage page de de reglage de la luminosité écran
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
void requestMeteo() {

  // Valeurs issues de Open Weather (Gestion Forecast)
  OW_forecast *forecast = new OW_forecast;

  ow.partialDataSet(false); // Collect a subset of the data available

  // Interogation de la météo

  // Previsions 3H sur 24h (API Openweather Map 2.5 Forecast (gratuit))
  bool parsed  = ow.getForecast(forecast, ow_api_key, ow_latitude, ow_longitude, ow_units, ow_language);
  if (parsed) {
    log_println("[Meteo] - Weather from OpenWeather");
    String msg = "[Meteo] - Heures previsions :";
    for (int ii=0 ; ii<8; ii++)
      msg += " " +strTime(forecast->dt[ii]);
    log_println(msg);

    log_println("[Meteo] - Position : Lat: " +String(ow.lat) +", Long: " +String(ow.lon));
    log_println("[Meteo] - Ville    : " +String(forecast->city_name) +"  - Lat: " +String(ow.lat) +", Long: " +String(ow.lon));
    log_println("[Meteo] - Soleil   : Lever: " +strTime(forecast->sunrise) +" - Coucher: " +strTime(forecast->sunset));

    for (int ii=0 ; ii<8; ii++) {
      msg = "[Meteo] - Prévis. de " +strTime(forecast->dt[ii]) 
        +" : Temp: " +String(forecast->temp[ii]) 
        +"°C, Hum: " +String(forecast->humidity[ii])
        +"%, Vent: "  +String(forecast->wind_speed[ii]) 
        +", Dir: "    +String(forecast->wind_deg[ii])
        +"°, Pres: " +String(forecast->pressure[ii], 0)
        +"hPa, Main: " +forecast->main[ii] 
        +", Desc: "  +forecast->description[ii]
        +", Icone: "  +forecast->icon[ii] 
        +" - ID: "    +String(forecast->id[ii]);
      log_println(msg);
    }

    myMeteo->lever = strTime(forecast->sunrise);
    myMeteo->coucher = strTime(forecast->sunset);

    myMeteo->temp       = forecast->temp[0];
    myMeteo->feels_like = 0;
    myMeteo->humidity   = forecast->humidity[0];
    myMeteo->pressure   = forecast->pressure[0];

    myMeteo->clouds     = forecast->clouds_all[0];
    myMeteo->visibility = forecast->visibility[0];
    myMeteo->rain       = 0;

    myMeteo->wind_deg   = forecast->wind_deg[0];
    myMeteo->wind_gust  = forecast->wind_gust[0];
    myMeteo->wind_speed = forecast->wind_speed[0];

    myMeteo->icone      = forecast->icon[0];
    myMeteo->ID         = forecast->id[0];
    myMeteo->main       = forecast->main[0];
    myMeteo->description = forecast->description[0];

    myMeteo->dt         = strTime(forecast->dt[0]);
    myMeteo->name       = forecast->city_name;

    msg = "[Meteo] - dt= " +myMeteo->dt 
      +", lever= " +myMeteo->lever 
      +", coucher= " +myMeteo->coucher
      +", temp= " +String(myMeteo->temp, 1) +"°"
      +", fells_like= " +String(myMeteo->feels_like, 1) +"°"
      +", hum= " +String(myMeteo->humidity) +"%"
      +", press= " +String(myMeteo->pressure, 0) +"hPa"
      +", clouds= " +String(myMeteo->clouds)
      +", visi= " +String(myMeteo->visibility)
      +", rain= " +String(myMeteo->rain, 1)
      +", vent= " +String(myMeteo->wind_speed, 1)
      +", rafale= " +String(myMeteo->wind_gust, 1)
      +", dir= " +String(myMeteo->wind_deg) +"°"
      +", icone= " +myMeteo->icone
      +", id= " +myMeteo->ID
      +"; main= " +myMeteo->main
      +", desc= " +myMeteo->description
      +", name= " +myMeteo->name;

    log_println(msg);

    lever = strTime(forecast->sunrise);
    coucher = strTime(forecast->sunset);
    tempExt = forecast->temp[0];
    icone = (forecast->icon[0]);
    ID = (forecast->id[0]);
  }
  else {
    log_println("[Meteo] - Failed to get data points");
  }

  esp_task_wdt_reset();

}

/***************************************************************************************
**                         Requete OpenWeather
**
***************************************************************************************/
void requestMeteo2() {
  const String urlWeather = "https://api.openweathermap.org/data/2.5/weather?lat=" + ow_latitude + "&lon=" + ow_longitude +"&units=" + ow_units + "&lang=" + ow_language + "&appid=" + ow_api_key;

  log_println("[Meteo2] - GET " +urlWeather);

  HTTPClient http;
  http.begin(urlWeather); //HTTP
  http.useHTTP10(true);
  http.setConnectTimeout(3000);
  http.setTimeout(3000);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
    // Parse response
    JsonDocument doc;
    auto err = deserializeJson(doc, http.getStream());
    if (err) {
      log_println("deserializeJson failed");
      http.end();
      esp_task_wdt_reset();
      return;
    }

    log_println("[Meteo2] - deseralized");

    // Read values
    myMeteo->lever      = strTime(doc["sys"]["sunrise"].as<uint32_t>());
    myMeteo->coucher    = strTime(doc["sys"]["sunset"].as<uint32_t>());

    myMeteo->temp       = doc["main"]["temp"].as<float>();
    myMeteo->feels_like = doc["main"]["feels_like"].as<float>();
    myMeteo->humidity   = doc["main"]["humidity"].as<uint8_t>();
    myMeteo->pressure   = doc["main"]["pressure"].as<float>();

    myMeteo->clouds     = doc["clouds"]["all"].as<uint8_t>();
    myMeteo->visibility = doc["visibility"].as<uint32_t>();
    myMeteo->rain       = doc["rain"]["1h"].as<float>();

    myMeteo->wind_deg   = doc["wind"]["deg"].as<uint16_t>();
    myMeteo->wind_gust  = doc["wind"]["gust"].as<float>();
    myMeteo->wind_speed = doc["wind"]["speed"].as<float>();

    myMeteo->icone      = doc["weather"][0]["icon"].as<String>();
    myMeteo->ID         = doc["weather"][0]["id"].as<String>();
    myMeteo->main       = doc["weather"][0]["main"].as<String>();
    myMeteo->description = doc["weather"][0]["description"].as<String>();

    myMeteo->dt         = strTime(doc["dt"].as<uint32_t>());
    myMeteo->name       = doc["name"].as<String>();

    lever   = myMeteo->lever;
    coucher = myMeteo->coucher;
    tempExt = myMeteo->temp;
    icone   = myMeteo->icone;
    ID      = myMeteo->ID;

    String msg = "[Meteo2] - dt= " +myMeteo->dt 
      +", lever= " +myMeteo->lever 
      +", coucher= " +myMeteo->coucher
      +", temp= " +String(myMeteo->temp, 1) +"°"
      +", fells_like= " +String(myMeteo->feels_like, 1) +"°"
      +", hum= " +String(myMeteo->humidity) +"%"
      +", press= " +String(myMeteo->pressure, 0) +"hPa"
      +", clouds= " +String(myMeteo->clouds)
      +", visi= " +String(myMeteo->visibility)
      +", rain= " +String(myMeteo->rain, 1)
      +", vent= " +String(myMeteo->wind_speed, 1)
      +", rafale= " +String(myMeteo->wind_gust, 1)
      +", dir= " +String(myMeteo->wind_deg) +"°"
      +", icone= " +myMeteo->icone
      +", id= " +myMeteo->ID
      +"; main= " +myMeteo->main
      +", desc= " +myMeteo->description
      +", name= " +myMeteo->name;

    log_println(msg);
  }
  else {
      log_println("[Meteo2] - Erreur code= " +httpCode);
  }

  http.end();
  esp_task_wdt_reset();

  return;
}

/***************************************************************************************
**                         Requete OpenWeather
**
***************************************************************************************/
void requestMeteo3() {
  String urlWeather = "https://api.openweathermap.org/data/2.5/forecast?cnt=1&lat=" + ow_latitude + "&lon=" + ow_longitude +"&units=" + ow_units + "&lang=" + ow_language + "&appid=" + ow_api_key;

  log_println("[Meteo3] - GET " +urlWeather);

  HTTPClient http;
  http.begin(urlWeather); //HTTP
  http.useHTTP10(true);
  http.setConnectTimeout(3000);
  http.setTimeout(3000);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
    // Parse response
    JsonDocument doc;
    auto err = deserializeJson(doc, http.getStream());
    if (err) {
      log_println("deserializeJson failed");
      http.end();
      esp_task_wdt_reset();
      return;
    }

    log_println("[Meteo3] - deseralized");

    // Read values
    myMeteo->lever      = strTime(doc["city"]["sunrise"].as<uint32_t>());
    myMeteo->coucher    = strTime(doc["city"]["sunset"].as<uint32_t>());

    myMeteo->temp       = doc["list"][0]["main"]["temp"].as<float>();
    myMeteo->feels_like = doc["list"][0]["main"]["feels_like"].as<float>();
    myMeteo->humidity   = doc["list"][0]["main"]["humidity"].as<uint8_t>();
    myMeteo->pressure   = doc["list"][0]["main"]["pressure"].as<float>();

    myMeteo->clouds     = doc["list"][0]["clouds"]["all"].as<int>();
    myMeteo->visibility = doc["list"][0]["visibility"].as<uint32_t>();
    myMeteo->rain       = doc["list"][0]["rain"]["1h"].as<float>();

    myMeteo->wind_deg   = doc["list"][0]["wind"]["deg"].as<uint16_t>();
    myMeteo->wind_gust  = doc["list"][0]["wind"]["gust"].as<float>();
    myMeteo->wind_speed = doc["list"][0]["wind"]["speed"].as<float>();

    myMeteo->icone      = doc["list"][0]["weather"][0]["icon"].as<String>();
    myMeteo->ID         = doc["list"][0]["weather"][0]["id"].as<String>();
    myMeteo->main       = doc["list"][0]["weather"][0]["main"].as<String>();
    myMeteo->description = doc["list"][0]["weather"][0]["description"].as<String>();

    myMeteo->dt         = strTime(doc["list"][0]["dt"].as<uint32_t>());
    myMeteo->name       = doc["city"]["name"].as<String>();

    lever   = myMeteo->lever;
    coucher = myMeteo->coucher;
    tempExt = myMeteo->temp;
    icone   = myMeteo->icone;
    ID      = myMeteo->ID;
  
    String msg = "[Meteo3] - dt= " +myMeteo->dt 
      +", lever= " +myMeteo->lever 
      +", coucher= " +myMeteo->coucher
      +", temp= " +String(myMeteo->temp, 1) +"°"
      +", fells_like= " +String(myMeteo->feels_like, 1) +"°"
      +", hum= " +String(myMeteo->humidity) +"%"
      +", press= " +String(myMeteo->pressure, 0) +"hPa"
      +", clouds= " +String(myMeteo->clouds)
      +", visi= " +String(myMeteo->visibility)
      +", rain= " +String(myMeteo->rain, 1)
      +", vent= " +String(myMeteo->wind_speed, 1)
      +", rafale= " +String(myMeteo->wind_gust, 1)
      +", dir= " +String(myMeteo->wind_deg) +"°"
      +", icone= " +myMeteo->icone
      +", id= " +myMeteo->ID
      +"; main= " +myMeteo->main
      +", desc= " +myMeteo->description
      +", name= " +myMeteo->name;

    log_println(msg);
  }
  else {
      log_println("[Meteo3] - Erreur code= " +httpCode);
  }

  http.end();
  esp_task_wdt_reset();

  return;
}

/***************************************************************************************
**                         Requete Forecast.solar
**
***************************************************************************************/
void reqMeteoSolaire (ulong ms) {
  static ulong nextSolar = 0UL;

  if ((ms < nextSolar) || (WiFi.status() != WL_CONNECTED))
    return; 

  nextSolar = ms + UPDATE_METEO_INTERVAL;

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

  log_println("[MeteoSolaire] - GET " +url);

  forecast_size = 0;

  HTTPClient http;
  http.begin(url); //HTTP
  http.useHTTP10(true);
  http.setConnectTimeout(3000);
  http.setTimeout(3000);
  http.addHeader("accept", "application/json");
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
    // Parse response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getStream());
    //ReadLoggingStream loggingStream(http.getStream(), Serial);
    //DeserializationError error = deserializeJson(doc, loggingStream);
    if (error) {
      log_println("[MeteoSolaire] - deserializeJson() failed: " + String(error.c_str()));
      http.end();  // Ferme la connexion
      return;
    }

    log_println("[MeteoSolaire] - deseralized");

    JsonObject watts = doc["result"]["watts"];
    forecast_size = watts.size();
    int ii = 0;

    for (JsonPair item : watts) {
      //log_println ("Item: key= " + String(item.key().c_str()) +", value= " + String(item.value().as<int>()));
      struct tm tm = {0};
      strptime(item.key().c_str(), "%Y-%m-%d %H:%M:%S", &tm);
      forecast_times[ii] = mktime(&tm);
      forecast_watts[ii] = item.value();
      ii++;
    }

    JsonObject wattshours = doc["result"]["watt_hours_period"];
    ii = 0;
    for (JsonPair item : wattshours) {
      forecast_wattshours[ii] = item.value();
      ii++;
    }

    JsonObject whday = doc["result"]["watt_hours_day"];
    ii = 0;
    for (JsonPair item : whday) {
      forecast_whday[ii] = item.value();
      ii++;
    }

    ratelimit_remaining = doc["message"]["ratelimit"]["remaining"];
    log_println("[MeteoSolaire] - ratelimit_remaining= " +String(ratelimit_remaining));
  } 

  http.end();  // Ferme la connexion
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
  str = parseSubString(dataStr, "<inAns>", "</inAns>");
  log_println("[Decode] - <inAns> : " +str);

  split(vals, 16, str, ';');
  powreso = vals[0].toInt();                // Consommation (si positif), ou Injection (si négatif)
  powpv = vals[1].toInt();                  // Panneaux PV
  powpv = (powpv > residuel) ? powpv : 0;
  outbal = vals[2].toInt() / 4;             // Dimmer Cumulus (0 à 400%) ==> 0-100%
  outrad = vals[3].toInt() / 4;             // Dimmer Radiateur (0 à 400%) ==> 0-100%
  voltres = vals[4].toInt();                // Tension réseau (V)
  tbal = vals[5].toFloat();                 // Sonde température Cumulus (en degrés, avec une decimale)
  tsdb = vals[6].toFloat();                 // Sonde température Salle de bain (en degrés, avec une decimale)
  tamb = vals[7].toFloat();                 // Sonde température Ambiente (en degrés, avec une decimale)

  powbal = (outbal * pCaractBallon) /100;
  powbal = (powbal > residuel) ? powbal : 0;

  log_println("[Decode] - <inAns> ==> powreso= " +String(powreso) + "w"
      + ", powpv= " +String(powpv) + "w"
      + ", outbal= " +String(outbal) + "%"
      + ", outrad= " +String(outrad) + "%"
      + ", voltres= " +String(voltres) + "V"
      + ", tbal= " +String(tbal) + "°C"
      + ", tsdb= " +String(tsdb) + "°C"
      + ", tamb= " +String(tamb) + "°C" );

  /*
  //+++++++++++++++++++++++++++++
  str = parseSubString(dataStr, "<survMm>", "</survMm>");
  log_println("[Decode] - <survMm> : " +str); 
  split(vals, 16, str, ';');

  //+++++++++++++++++++++++++++++
  str = parseSubString(dataStr, "<cmdPos>", "</cmdPos>");
  log_println("[Decode] - <cmdPos> : " +str); 
  split(vals,  8, str, ';');
  
  //+++++++++++++++++++++++++++++
  str = parseSubString(dataStr, "<outStat>", "</outStat>");
  log_println("[Decode] - <outStat> : " +str); 
  split(vals, 16, str, ';');
  */

  //+++++++++++++++++++++++++++++
  str = parseSubString(dataStr, "<cptVals>", "</cptVals>");
  log_println("[Decode] - <cptVals> : " +str); 
  split(vals,  8, str, ';');
  cumulConso = hexa2float(vals[0]) /10000.;       // Cumul Consomation en kwh
  cumulInj   = hexa2float(vals[1]) /10000.;       // Cumul Injection en kwh
  cumulPV_J  = hexa2float(vals[2]) /10000.;       // Cumul Panneaux
  cumulPV_P  = hexa2float(vals[3]) /10000.;       // Cumul Ballon cumulus
  
  log_println("[Decode] - <cptVals> ==> cumulConso= " +String(cumulConso, 3) + "kwh"
      + ", cumulInj= " +String(cumulInj, 3) + "kwh"
      + ", cumulPV_J= " +String(cumulPV_J, 3) + "kwh"
      + ", cumulPV_P= " +String(cumulPV_P, 3) + "kwh"); 

  /*
  //+++++++++++++++++++++++++++++
  str = parseSubString(dataStr, "<chOutVal>", "</chOutVal>");
  log_println("[Decode] - <cmdPos> : " +str); 
  split(vals, 4, str, ';');
  */

  return true;
}

/***************************************************************************************
**                         Réception des données MSunPV
**
***************************************************************************************/
bool requestMSunPV(ulong ms) {
  static ulong nextMSunPVUpdate = 0UL;
  bool isOk = false;

  if (WiFi.status() != WL_CONNECTED) {
    // En cas de perte de WIFI > 10mn
    if (ms > nextMSunPVUpdate + 600000)
      ESP.restart();

    return isOk;
  }

  if (ms < nextMSunPVUpdate)
    return isOk;

  nextMSunPVUpdate = ms + UPDATE_MSUNPV_INTERVAL;

  // Affichage indicateur de rafraichissement des données en haut à gauche : démarrage
  if ((page == PAGE_DEFAULT) && (!firstAff)) {
    sprite.drawSmoothCircle(8, 8, 4, TFT_GREENYELLOW, TFT_TRANSPARENT); 
    sprite.pushSprite(0, 0);
  }

  HTTPClient http;
  String url = "http://" + msunpv_server + "/status.xml"; 
  log_println("[MSunPV] - GET " +url);
  http.setConnectTimeout(3000);
  http.setTimeout(3000);
  http.begin(url); //HTTP

  int httpCode = http.GET();
  if(httpCode != HTTP_CODE_OK) {
    log_println("[MSunPV] - Failed to GET "+url +" - error: " +http.errorToString(httpCode));
    http.end();
  }
  else {
    String data = http.getString();
    //log_println("[MSunPV] - GET [Ok], data : ["  + data +"]");
    http.end();

    if(decodeXML(data)) {
      isOk = true;
      updateAff = true;
      
      // Indicateur de rafraichissement des données => statut terminé (il sera effacé lors du rafraichissement de l'écran)
      if ((page == PAGE_DEFAULT) && (!firstAff)) {
        sprite.fillSmoothCircle(8, 8, 4, TFT_GREENYELLOW);
        sprite.pushSprite(0, 0);
      }
    }
  } 

  esp_task_wdt_reset();
  return isOk;
}

/***************************************************************************************
**                         Réception des données Tempo
**
***************************************************************************************/
void requestTempo(ulong ms) {
  static ulong nextTempo = 0UL;

  if ((ms < nextTempo) || (WiFi.status() != WL_CONNECTED)) 
    return;

  nextTempo = ms + UPDATE_METEO_INTERVAL;

  HTTPClient http;
  const String urlTempoJour   = "https://www.api-couleur-tempo.fr/api/jourTempo/today";
  const String urlTempoDemain = "https://www.api-couleur-tempo.fr/api/jourTempo/tomorrow";
  JsonDocument doc;

  codeTempoJ = codeTempoJ1 = 0;

  http.setConnectTimeout(3000);
  http.setTimeout(3000);
  http.addHeader("accept", "application/json");

  log_println("[Tempo] - GET " +urlTempoJour);
  if (http.begin(urlTempoJour)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
      DeserializationError err = deserializeJson(doc, http.getString());
      if (err) {
        log_println("[Tempo] - deserializeJson() failed: " + String(err.c_str()));
        http.end();
        return;
      }

      codeTempoJ = doc["codeJour"];
      if ((codeTempoJ < 0) || codeTempoJ > 3) 
        codeTempoJ = 0;
    }

    http.end();
  } 

  log_println("[Tempo] - GET " +urlTempoDemain);
  if (http.begin(urlTempoDemain)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
      DeserializationError err = deserializeJson(doc, http.getString());
      if (err) {
        log_println("[Tempo] - deserializeJson() failed: " + String(err.c_str()));
        http.end();
        return;
      }

      codeTempoJ1 = doc["codeJour"];
      if ((codeTempoJ1 < 0) || codeTempoJ1 > 3) 
        codeTempoJ1 = 0;
    } 

    http.end();
  }

  esp_task_wdt_reset();
}

/***************************************************************************************
**         Lecture de la tension batterie
**
***************************************************************************************/
void readVbatt() {

  //vBatt = (float)(analogRead(4)) * 2. * 3300. / 4096.;         // Vbatt = 3.3V, 12bits
  vBatt = (float)(analogRead(4)) * 7.26 / 4096.;         // Vbatt = 3.3V, (12bits, Vref= 1,1V, analogInputDivider= 2) ==>  Val = (Read) * 1,1 * 2 /4096 
  log_println("[vBat] - vBat = " +String(vBatt) +"V");
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
String strTime(time_t unixTime) {
  time_t local_time = TIMEZONE.toLocal(unixTime, &tz1_Code);

  String str = ((hour(local_time) < 10) ? ("0" +String(hour(local_time))) : (String(hour(local_time)))) +":";
  str += ((minute(local_time) < 10) ? ("0" +String(minute(local_time))) : (String(minute(local_time))));
  return str;
}

/***************************************************************************************
** Transforme les W en kW (par Bellule, encore lui!)
**
***************************************************************************************/
float wh_to_kwh(float wh) {
  return wh / 1000.0;
}

/***************************************************************************************
**     Routine de test pour afficher toutes les icones sur l'écran
**               Décommenter la ligne 446 pour l'activer
***************************************************************************************/
void test() {
  powpv = 0;
  powreso = 4500;
  powbal = 90;
  tbal = 55.0;
  tempExt = 3.0;
  withTBal = true;
  withRad = true;
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
  // Log message on serial port if available
  if (Serial)
    Serial.print(msg);
}

void log_println(String msg) {
  log_print(msg + "\n");
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
      log_println("\nWiFi lost. Call connectMultiWiFi in loop");
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

  log_println("apCallback  <<<<<<<<<<<<<<");

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
  log_println("Should save config");
  shouldSaveConfig = true;
}

/***************************************************************************************
**                              Initialisaton du Wifi
** 
***************************************************************************************/
void init_wifi() {

  log_println("[Wifi] - Connect WIFI...");

  btStop();  // Stop Bluetooth
  WiFi.mode(WIFI_MODE_STA);

  AsyncWebServer webServer(80);
  AsyncDNSServer dnsServer;
  ESPAsync_WiFiManager ESPAsync_wifiManager(&webServer, &dnsServer, "Companion-NG");

  //reset settings - for testing
  //ESPAsync_wifiManager.resetSettings();
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
  
//  ESPAsync_WMParameter p_withTBal("withTBal", "Temp. Ballon",  (withTBal) ? "T" : "F", 1, (withTBal ? htmlCB_on : htmlCB), WFM_LABEL_AFTER);
//  ESPAsync_WMParameter p_withVeille("withVeille", "Mode Veille", (withVeille) ? "T" : "F", 1, (withVeille ? htmlCB_on : htmlCB), WFM_LABEL_AFTER);
//  ESPAsync_WMParameter p_withBatt("withBatt", "Alim par Batt", (withBatt) ? "T" : "F", 1, (withBatt ? htmlCB_on : htmlCB), WFM_LABEL_AFTER);
  ESPAsync_WMParameter p_withTBal("withTBal", "Temp. Ballon",  "T", 2, (withTBal ? htmlCB_on : htmlCB), WFM_LABEL_AFTER);
  ESPAsync_WMParameter p_withVeille("withVeille", "Mode Veille", "T", 2, (withVeille ? htmlCB_on : htmlCB), WFM_LABEL_AFTER);
  ESPAsync_WMParameter p_withBatt("withBatt", "Alim par Batt", "T", 2, (withBatt ? htmlCB_on : htmlCB), WFM_LABEL_AFTER);

  ESPAsync_WMParameter p_sepMeteo("<br/><p>M&eacute;teo OpenWeatherMap");
  ESPAsync_WMParameter p_ow_latitude("ow_latitude", "Latitude", ow_latitude.c_str(), 9);
  ESPAsync_WMParameter p_ow_longitude("ow_longitude", "Longitude", ow_longitude.c_str(), 9);
  ESPAsync_WMParameter p_ow_apikey("ow_api_key", "Cl&eacute; API", ow_api_key.c_str(), 33);


  ESPAsync_WMParameter p_sepMqtt("<br/><p>HA MQTT Int&eacute;gration");
//  ESPAsync_WMParameter p_withMqtt("withMqtt", "MQTT", (withMqtt) ? "T" : "F", 1, (withMqtt ? htmlCB_on : htmlCB), WFM_LABEL_AFTER);
  ESPAsync_WMParameter p_withMqtt("withMqtt", "MQTT", "T", 2, (withMqtt ? htmlCB_on : htmlCB), WFM_LABEL_AFTER);
  ESPAsync_WMParameter p_mqtt_server("mqtt_server", "MQTT (adrs. IP)", mqtt_server.c_str(), 16);
  ESPAsync_WMParameter p_mqtt_user("mqtt_user", "MQTT user", mqtt_user.c_str(), 16);
  ESPAsync_WMParameter p_mqtt_pwd("mqtt_pwd", "MQTT password", mqtt_pwd.c_str(), 16);


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

  
  String stored_SSID = ESPAsync_wifiManager.WiFi_SSID();
  String stored_Pass = ESPAsync_wifiManager.WiFi_Pass();
  log_println("ESP Self-Stored: SSID = " + stored_SSID + ", Pass = " + stored_Pass);
  
  if ((stored_SSID == "") || (msunpv_server == ""))
  {
    log_println("No previous AP credentials and/or config");
    initialConfig = true;
  }
/*  
  else {
    log_println(" ==> Add this SSID/PW on wifiMulti");
    wifiMulti.addAP(stored_SSID.c_str(), stored_Pass.c_str());
    ESPAsync_wifiManager.setConfigPortalTimeout(120);
  }
*/

  if (initialConfig) { 
    log_println("Start ConfigPortal AP mode");
    // Start AP mode       
    if (!ESPAsync_wifiManager.startConfigPortal("Companion-NG", NULL)) {
      log_println("Not connected to WiFi but continuing anyway.");
    }

    if (WiFi.status() == WL_CONNECTED) {
      log_println("WiFi connected... after ConfigPortal AP mode");
    }
    else {
      log_println("ERREUR - WiFi not connected after ConfigPortal AP mode");
    }
  }
  else {
    // Auto connect with multiWifi
    //log_println("ConnectMultiWiFi in setup");
    //connectMultiWiFi();

    log_println("Start autoconnect mode");
    ESPAsync_wifiManager.autoConnect("Companion-NG");   // en cas d'echec, autoconnect lance egalement le ConfigPortal (mode AP)

    if (WiFi.status() == WL_CONNECTED) {
      log_println("WiFi connected... after autoconnect");
    }
    else {
      log_println("ERREUR - WiFi not connected after autoconnect");
    }
  }

  // Save the custom parameters to FS
  if (shouldSaveConfig) {
    shouldSaveConfig = false;

    // Read updated parameters
    msunpv_server = p_msunpv_server.getValue();
    pCaractPV = String(p_puisPV.getValue()).toInt();
    pCaractBallon = String(p_puisBal.getValue()).toInt();

    log_println("p_withTBal value= " +String(p_withTBal.getValue()));
    log_println("p_withVeille value= " +String(p_withVeille.getValue()));
    log_println("p_withBatt value= " +String(p_withBatt.getValue()));
    withTBal = (String(p_withTBal.getValue()) == "T");
    withVeille = (String(p_withVeille.getValue()) == "T");
    withBatt = (String(p_withBatt.getValue()) == "T");
    
    ow_latitude = String(p_ow_latitude.getValue());
    ow_longitude = String(p_ow_longitude.getValue());
    ow_api_key = String(p_ow_apikey.getValue());

    log_println("p_withMqtt value= " +String(p_withMqtt.getValue()));
    withMqtt = (String(p_withMqtt.getValue()) == "T");
    mqtt_server = p_mqtt_server.getValue();
    mqtt_user = p_mqtt_user.getValue();
    mqtt_pwd = p_mqtt_pwd.getValue();

    log_println("Sauvegarde de la config : server= " + String(msunpv_server) 
      +", pCaractPV= " +String(pCaractPV)
      +", pCaractBallon= " +String(pCaractBallon)

      +", withTBal= " +String(withTBal)
      +", withVeille= " +String(withVeille)
      +", withBatt= " +String(withBatt)

      +", ow_latitude= " +ow_latitude
      +", ow_longitude= " +ow_longitude
      +", ow_api_key= " +ow_api_key 
      
      +", withMqtt= " +String(withMqtt) 
      +", mqtt_server= " +mqtt_server 
      +", mqtt_user= " +mqtt_user 
      +", mqtt_pwd= " +mqtt_pwd 
      );

    saveFileFSConfigFile();
  }

  WiFi.mode(WIFI_MODE_STA);

  log_println("WiFi connected...yeey 2");
}

/***************************************************************************************
** Liste le contenu de LittleFS
** 
***************************************************************************************/
void listDir(const char * dirname, uint8_t levels){
    log_println("Listing directory: " + String(dirname));

    File root = LittleFS.open(dirname);

    if(!root){
        log_println("- failed to open directory");
        return;
    }

    if(!root.isDirectory()){
        log_println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            log_println("  DIR : " +String(file.name()));
            if(levels){
                listDir(file.name(), levels -1);
            }
        } else {
            log_println("  FILE: " +String(file.name()) +",  SIZE: " +String(file.size()));
        }
        file = root.openNextFile();
    }
}

/***************************************************************************************
** Read File from LittleFS 
** 
***************************************************************************************/
String readFile(const char * path){
  log_printf("Reading file: %s\r\n", path);

  File file = LittleFS.open(path);
  if(!file || file.isDirectory()){
    log_println("- failed to open file for reading");
    return String();
  }
  
  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;     
  }
  return fileContent;
}

/***************************************************************************************
** Write file to LittleFS 
** 
***************************************************************************************/
void writeFile(const char * path, const char * message){
  log_printf("Writing file: %s\r\n", path);

  File file = LittleFS.open(path, FILE_WRITE);
  if(!file){
    log_println("- failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    log_println("- file written");
  } else {
    log_println("- write failed");
  }
}

/***************************************************************************************
** Load config file
** 
***************************************************************************************/
bool loadFileFSConfigFile() {
  log_println("Reading config file - " + String(configFileName));

  if (!LittleFS.begin()) {
    log_println("ERROR - failed to mount FS");
    return false;
  }    

  File fs = LittleFS.open(configFileName, "r");
  if (!fs) {
    log_println("ERROR - Failed to open config file for reading");
    return false;
  }

  JsonDocument doc;
  auto err = deserializeJson(doc, fs.readString());
  if (err) {
    log_println("deserializeJson failed");
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

  fs.close();

  log_println("Lecture config OK : msunpv_server= " + msunpv_server 
    +", pCaractPV= " +String(pCaractPV)
    +", pCaractBallon= " +String(pCaractBallon)

    +", withTBal= " +String(withTBal)
    +", withVeille= " +String(withVeille)
    +", withBatt= " +String(withBatt)

    +", ow_latitude= " +String(ow_latitude)
    +", ow_longitude= " +String(ow_longitude)
    +", ow_api_key= " +String(ow_api_key) 

    +", withMqtt= " +String(withMqtt) 
    +", mqtt_server= " +mqtt_server 
    +", mqtt_user= " +mqtt_user 
    +", mqtt_pwd= " +mqtt_pwd 
    );

  return true;
}

/***************************************************************************************
** Save config file
** 
***************************************************************************************/
bool saveFileFSConfigFile() {
  log_println("Saving config to file : " + String(configFileName));

  File fs = LittleFS.open(configFileName, "w");
  if (!fs) {
    log_println("ERROR - Failed to open config file for writing");
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

  log_println("saveFileFSConfigFile - serialyze");

  serializeJsonPretty(json, Serial);    // debug

  // Write data to file and close it
  serializeJson(json, fs);

  fs.close();
  log_println("saveFileFSConfigFile - serialyze OK");
  
  return true;
}

const char* lumiFileName = "/lumi.json";

void loadLumiParam() {

  if (!LittleFS.begin()) {
    log_println("ERROR - failed to mount FS");
    return;
  }

  File fs = LittleFS.open(lumiFileName, "r");
  if (!fs) {
    log_println("ERROR - Failed to open lumi.json file for reading");
    return;
  }

  JsonDocument doc;
  auto err = deserializeJson(doc, fs.readString());
  if (err) {
    log_println("deserializeJson failed");
  }
  else {
    if (doc["lumi"]) backligthLevel = doc["lumi"].as<int>();
  }

  fs.close();
}

void saveLumiParam() {
  File fs = LittleFS.open(lumiFileName, "w");
  if (!fs) {
    log_println("ERROR - Failed to open lumi.json file for writing");
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
