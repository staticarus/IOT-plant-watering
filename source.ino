//------------------[NODEMCU CODE ONLINE COMPOSANTS]-------------------------
#define CAYENNE_DEBUG             // Logs
#define CAYENNE_PRINT Serial      // Cayenne Serial communication
#include <CayenneMQTTESP8266.h>   // Cayenne MQTT implementation
#include <ESP8266WiFi.h>          // ESP8266 Wi-Fi connectivity
#include <WiFiClientSecure.h>     // HTTPS
#include <ESP8266HTTPClient.h>    // Simplifie les requêtes HTTP
#include <pt.h>                   // Protothreading
//-------------------[DONNÉES PERSONNELLES INSÉRÉES ICI POUR LES COMMUNICATIONS INTERNET]------------------
char ssid[] = "RESEAU_WIFI";
char wifiPassword[] = "MOT_DE_PASSE_WIFI";
char username[] = "COMPTE_CAYENNE";
char password[] = "COMPTE_CAYENNE";
char clientID[] = "COMPTE_CAYENNE";
// Temp-Humi
String url_webhook1 = "WEBHOOK_URL";
const char fingerprint1[] = "WEBHOOK_FINGERPRINT";
// Plante
String url_webhook2 = "WEBHOOK_URL";
const char fingerprint2[] = "WEBHOOK_FINGERPRINT";

//-----------------[NODEMCU CODE OFFLINE COMPOSANTS]-------------------------
#include <LiquidCrystal_I2C.h>        // LCD 16x2 I2C library
#include <dht.h>                      // DHT library
#define DHT11_PIN 12                  // DHT pin (D6 NodeMCU)
#define watersensor A0                // Water level sensor pin
#define IN1 14                        // Motor pin (D5 NodeMCU - IN1 L298N)
LiquidCrystal_I2C lcd(0x27, 16, 2);   // Création d'un objet LCD
dht DHT;                              // Création d'un objet DHT
//---------------------------------------------------------------------------
unsigned long lastMillis = 0;         //
int t;                                // temp
int h;                                // humi
int wa;                               // water level
int wb;                               // water percent
static struct pt pt1;                 // création des protothreads
static struct pt pt2;                 //
static struct pt pt3;                 //
static struct pt pt4;                 //
//---------------------------------------------------------------------------[PROTOTHREADS]
static int protothreadCayenneLoop(struct pt *pt)  // Code pour le MQTT
{
  static unsigned long lastTimeBlink = 0;
  PT_BEGIN(pt);
  while(1) {
    lastTimeBlink = millis();
    PT_WAIT_UNTIL(pt, millis() - lastTimeBlink > 50);
    Cayenne.loop();
  }
  PT_END(pt);
}
static int protothreadLED(struct pt *pt)          // Code pour allumer et éteindre la LED intégrée (inutile, permet juste de vérifier le bon fonctionnement du protothreading)
{
  static unsigned long lastTimeBlink = 0;
  PT_BEGIN(pt);
  while(1) {
    lastTimeBlink = millis();
    PT_WAIT_UNTIL(pt, millis() - lastTimeBlink > 1000);
    digitalWrite(2,LOW);
    lastTimeBlink = millis();
    PT_WAIT_UNTIL(pt, millis() - lastTimeBlink > 1000);
    digitalWrite(2,HIGH);
  }
  PT_END(pt);
}
static int protothreadCayenne(struct pt *pt)      // Code pour l'envoi périodique de données de température et humidité sur channel MQTT de Cayenne et sur écran LCD
{
  static unsigned long lastTimeMQTT = 0;
  PT_BEGIN(pt);
  while(1) {
    lastTimeMQTT = millis();
    PT_WAIT_UNTIL(pt, millis() - lastTimeMQTT > 1000);  
    t = DHT.temperature;
    h = DHT.humidity;
    Serial.println(DHT.temperature);
    Serial.println(DHT.humidity);
    Cayenne.celsiusWrite(2, t);                 // La fonction celsiusWrite intègre directement la syntaxe de t°
    Cayenne.virtualWrite(3, h, "rel_hum", "p"); // J'écris à la main la syntaxe d'humidité relative
    lcd.setCursor(13,1);
    lcd.print('.');
    lastTimeMQTT = millis();                    // Petite animation LCD avant actualisation des résultats
    PT_WAIT_UNTIL(pt, millis() - lastTimeMQTT > 1000);
    lcd.print('.');
    lastTimeMQTT = millis();
    PT_WAIT_UNTIL(pt, millis() - lastTimeMQTT > 1000);
    lcd.print('.');
    lastTimeMQTT = millis();
    PT_WAIT_UNTIL(pt, millis() - lastTimeMQTT > 1000);
    lcd.setCursor(6,0);
    lcd.print("   ");
    lcd.setCursor(6,0);
    lcd.print(DHT.temperature);
    lcd.setCursor(6,1);
    lcd.print("   ");
    lcd.setCursor(6,1);
    lcd.print(DHT.humidity);
    lcd.setCursor(13,1);
    lcd.print("   ");
  }
  PT_END(pt);
}
static int protothreadDiscord(struct pt *pt)      // Code pour l'envoi périodique des données t° + humi % sur discord
{
  static unsigned long lastTimeDiscord = 0;
  PT_BEGIN(pt);
  while(1) {
    lastTimeDiscord = millis();
    PT_WAIT_UNTIL(pt, millis() - lastTimeDiscord > 30000);  
    float DHTdata = DHT.read11(DHT11_PIN);
    t = DHT.temperature;
    h = DHT.humidity;
    WiFiClientSecure client_https;
    client_https.setFingerprint(fingerprint1);
    HTTPClient requete;
    requete.begin(client_https, url_webhook1);
    requete.addHeader("Content-Type", "application/json");  
    String payload = "";
    payload = "{\"content\": \"";
    payload += "Température = " + String(t) + " °C";
    payload += ", ";
    payload += "Humidité = " + String(h) + "%";
    payload += "\"}";
  
  int httpCode = requete.POST(payload);
  if(httpCode > 0)
  {
    Serial.print("Le code est : ");
    Serial.println(httpCode);

    if (httpCode == HTTP_CODE_OK) 
    {
        const String reponse = requete.getString();
        Serial.println("reponse:");
        Serial.println(reponse);
    }
  }
  else
  {
     Serial.println("[ERREUR] Échec de la tentative, aucune réponse HTTP reçue.");
  }
  }
  PT_END(pt);
}

//-----------------------------[SETUP du Wifi + de la connexion Cayenne + de l'écran LCD]-------------------------

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println(" ");
  Serial.println("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, wifiPassword);
  lcd.begin(16,2);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Connexion au web");
  lcd.setCursor(0,1);
  int i = 0;
  while (WiFi.status() != WL_CONNECTED)   // Boucle pour la tentative de connexion
    {
      if (i==16) // Remise à zéro de l'animation lorsqu'elle atteint le bout de l'écran
      {
        lcd.setCursor(0,1);
        lcd.print("                ");
        lcd.setCursor(0,1);
      }
      lcd.print(".");
      Serial.print(".");
      delay(500);
      i += 1;
    }
  lcd.setCursor(0,0);
  lcd.print("  Connecte ! :) ");
  lcd.setCursor(0,1);
  lcd.print("                ");
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());         // IP de l'ESP8266
//---------------------------------------------------------------------------
  Cayenne.begin(username, password, clientID, ssid, wifiPassword);
  pinMode(2,OUTPUT);              // Built in LED
  pinMode(IN1,OUTPUT);            // Moteur
  pinMode(watersensor,INPUT);     // Senseur de niveau d'eau
  pinMode(DHT11_PIN, INPUT);      // DHT
  digitalWrite(IN1,LOW);          // Moteur OFF
  digitalWrite(watersensor,LOW);  // Water Level OFF
  PT_INIT(&pt1);
  PT_INIT(&pt2);
  PT_INIT(&pt3);
  PT_INIT(&pt4);
//---------------------------------------------------------------------------
  delay(2000);    // Permet de laisser le message "Connecte ! :)" durant deux secondes à l'écran avant de le rafraîchir pour les mesures
  lcd.setCursor(0,0);
  lcd.print("Temp:           ");
  lcd.setCursor(0,1);
  lcd.print("Humi:           ");
}

void loop() {
  // Lance les protothreads l'un à la suite de l'autre
  // lorsque la condition de l'un d'entre eux n'est pas remplie
  // le code passe automatiquement au suivant et ainsi de suite
  // De cette manière, je simule du multi-thread
  protothreadCayenneLoop(&pt4);
  protothreadLED(&pt1);                     // Led ON ou OFF toutes les 1 secondes
  float DHTdata = DHT.read11(DHT11_PIN);
  protothreadCayenne(&pt2);                 // Envoi MQTT 
  protothreadDiscord(&pt3);
  
  // ---------------------------------------------------------

}

CAYENNE_IN(0) // Channel 0 = données de la LED intégrée à l'ESP
{
  // Permet d'allumer ou éteindre la LED à distance, par ordre (= appui sur le bouton du dashboard web)
  // Je reçois la valeur envoyée par le bouton ON/OFF de Cayenne, 
  // et j'inverse l'état car les 2 built-in LED du NodeMCU fonctionnement en état inversé (LOW pour allumer, HIGH pour éteindre)
  digitalWrite(2,!getValue.asInt());
}
CAYENNE_IN(1) // Channel 1 = données de l'état du moteur
{
  // Allume le moteur 5 secondes, allume le capteur d'eau pour récupérer sa valeur
  // Puis les éteint tous les deux : par souci de batterie pour le moteur
  // Et par souci d'usure pour le capteur d'eau (l'alimenter continuellement lorsqu'il est immergé favorise l'usure)
    digitalWrite(IN1,HIGH);
    digitalWrite(watersensor,HIGH);
    delay(5000);
    digitalWrite(IN1,LOW);
    wa = analogRead(watersensor);   // reçoit une valeur qui oscille entre 0 (pas d'eau) et environ 650 (pistes totalement immergées)
    wb = (wa/65)*10;                // transforme cette valeur en un pourcentage approximatif
    Cayenne.virtualWrite(4, wb, "t_lum", "p");
    discordArrosage();
    digitalWrite(watersensor,LOW);
}

void tempLCD() {
  // Petite animation sur l'écran LCD pour l'actualiation des résultats t° et humidité
  lcd.setCursor(13,1);
  lcd.print('.');
  delay(1000);
  lcd.print('.');
  delay(1000);
  lcd.print('.');
  delay(1000);
  lcd.setCursor(6,0);
  lcd.print("   ");
  lcd.setCursor(6,0);
  lcd.print(DHT.temperature);
  lcd.setCursor(6,1);
  lcd.print("   ");
  lcd.setCursor(6,1);
  lcd.print(DHT.humidity);
  lcd.setCursor(13,1);
  lcd.print("   ");
}

void discordArrosage()  // Envoie une requête sur Discord pour informer qu'un arrosage a eu lieu
{
  WiFiClientSecure client_https;
  client_https.setFingerprint(fingerprint2);
  HTTPClient requete;
  requete.begin(client_https, url_webhook2);
  requete.addHeader("Content-Type", "application/json");  
  String payload = "";
  payload = "{\"content\": \"";
  payload += "USER_ID_DISCORD";   // ID réel de mon compte utilisateur obtenu via la commande \@user. Cela permet de me @mentionner (notification rouge)
  payload += " , ";
  payload += "un arrosage vient d'avoir lieu ! Le niveau d'eau restant est : " + (String)wb + "%.  :tulip: :droplet:"; // emojis car c'est joli !
  payload += "\"}";
  
  int httpCode = requete.POST(payload);
  if(httpCode > 0)
  {
    Serial.print("Le code est : ");
    Serial.println(httpCode);

    if (httpCode == HTTP_CODE_OK) 
    {
        const String reponse = requete.getString();
        Serial.println("reponse:");
        Serial.println(reponse);
    }
  }
  else
  {
     Serial.println("[ERREUR] Échec de la tentative, aucune réponse HTTP reçue.");
  }
}

//-------------------------------------------------
