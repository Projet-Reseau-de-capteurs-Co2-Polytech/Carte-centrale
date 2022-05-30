/**
 * Code de la carte centrale
 * 
 * Fonctionnalités :
 *  - Récupérer les données de différents capteurs d'un même réseau Zigbee
 *  - Calculer ou recevoir les données complémentaires
 *  - Envoyer les données complètes au serveur via API
 *  
 *  Données reçues :
 *    - idCapteur
 *    - tauxCO2
 *    
 *  Données envoyées :
 *    - idBatiment
 *    - idCapteur
 *    - tauxCO2
 *    - date
 */


// ----------------------------------------
//      Importations & Déclaration :
// ----------------------------------------
#include <Arduino.h>
#include <Wire.h>
#include <Ethernet.h>
#include <avr/sleep.h>
#include <Array.h>
#include <SPI.h>
#include<SoftwareSerial.h>

/**
 * Mémoire Arduino Uno = 2k bytes (SRAM)
 * Array idCapteurs    = x * 4 bytes
 * Array tauxCO2       = x * 2 bytes
 * Array date          = x * 8 bytes
 *  => Stockage max de environ 140 tuples de données
 *  => nb tuples choisi = 60 pour éviter une collision heap/stack
 */
#define ID_BATIMENT 22001
#define INPUT_SIZE 31
#define DATE_TIME_SIZE 20
#define NB_MAX_TUPLES 80

SoftwareSerial zigbeeSerial(7, 6);
unsigned long previousMillis = 0;


// Variables locales
char receivedData[INPUT_SIZE+1];  // INPUT_SIZE+1 bytes
Array<int,       NB_MAX_TUPLES> idCapteur;
Array<short,     NB_MAX_TUPLES> tauxCO2;
Array<unsigned long long, NB_MAX_TUPLES> date;
int nbPaquetsNonEnvoyes;
int lastDataSentTime = 0;


//Variable locales pour la connexion ethernet : 
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
int    HTTP_PORT   = 80;
String POST="POST"; 
String GET="GET"; 
char   HOST_NAME[] = "sirgodfroy.alwaysdata.net";
String PATH_NAME_POST= "/api/api.php";
String PATH_NAME_GET="/api/date.php"; 
//String queryString = "?idBatiment=1&idCapteur=2&tauxCO2=400&heure=11:00";
EthernetClient client;


struct datetime { // 12 bytes
  // date-time format : "yyyy-mm-dd hh:mm:ss" (20 bytes)
  //char value[DATE_TIME_SIZE];
  short year;     //  2 bytes
  short month;    //  2 bytes
  short day;      //  2 bytes
  short hours;    //  2 bytes
  short minutes;  //  2 bytes
  short seconds;  //  2 bytes
};



// ----------------------------------------
//                Fonctions
// ----------------------------------------
/**
 * Réalise la requete http POST via l'API. 
 */
  void HTTP_Request_POST(String queryString){
     if(client.connect(HOST_NAME, HTTP_PORT)) {
    // if connected:
    Serial.println("Connected to server");
    // make a HTTP request:
    // send HTTP header
    client.println(POST + " " + PATH_NAME_POST + " HTTP/1.1");
    client.println("Host: " + String(HOST_NAME));
    client.println("Connection: close");
    client.println(); // end HTTP header
    client.println(queryString);
    }
  }

/**
 * Réalise la requête http GET via l'API
 */
void HTTP_Request_GET(){
  if(client.connect(HOST_NAME, HTTP_PORT)) {
    // if connected:
    Serial.println("Connected to server");
    // make a HTTP request:
    // send HTTP header
    client.println(GET + " " + PATH_NAME_GET + " HTTP/1.1");
    client.println("Host: " + String(HOST_NAME));
    client.println("Connection: close");
    client.println(); // end HTTP header 
  }
}


/**
 * Parse une chaîne de caractères de format "yyyy-mm-dd hh:mm:ss"
 */
char** stringToTokens(char* src) {
    char** res = malloc( sizeof(char*) * 6);
    int nbtokens = 0, ite = 0;

    res[0] = malloc( sizeof(char) *5);
    for (int p=0; p<4; p++) {
        res[0][p] = src[p];
    }
    res[0][4] = '\0';

    for (int i=1; i<6; i++) {
        res[i] = malloc( sizeof(char) *3);
        for (int p=0; p<2; p++) {
            res[i][p] = src[i*3 + 2 + p];
        }
        res[i][2] = '\0';
    }
    return res;
}


/**
 * Convertit une variable datetime en ms
 */
datetime parseDateTimeString(char* timeStr) {
  datetime dt;

  char** tokens = stringToTokens(timeStr);
    
  dt.year = atoi(tokens[0]);
  dt.month = atoi(tokens[1]);
  dt.day = atoi(tokens[2]);
  dt.hours = atoi(tokens[3]);
  dt.minutes = atoi(tokens[4]);
  dt.seconds = atoi(tokens[5]);

  return dt;
}

/**
 * convertit un datetime en un temps en ms
 */
unsigned long long datetimeToMs(struct datetime t) {
  unsigned long long timeRef = 1000;
  unsigned long long l = t.seconds * timeRef;
  timeRef *= 60;  l += t.minutes * timeRef;
  timeRef *= 60;  l += t.hours * timeRef;
  timeRef *= 24;  l += t.day * timeRef;
  l += t.month * timeRef * 30.436875;
  l += t.year * timeRef * 365.2425;
  return l;
}

/**
 * convertit un temps en ms au format "yyyy-mm-dd hh:mm:ss"
 */
char* msToDatetime(long long timeMs) {
  unsigned long long timeref = 1000*60*60*24;
  short ye = timeMs/timeref/365.2425; timeMs = timeMs - ye * 365.2425*timeref;
  short mo = timeMs/timeref/30.436875; timeMs = timeMs - mo * 30.436875*timeref;
  short da = timeMs/timeref; timeMs = timeMs - da * timeref;   timeref /= 24;
  short ho = timeMs/timeref; timeMs = timeMs - ho * timeref; timeref /= 60;
  short mi = timeMs/timeref; timeMs = timeMs - mi * timeref; timeref /= 60;
  short se = timeMs/timeref;
  
  char* dt = malloc(sizeof(char) * 20);
  sprintf(dt, "%04hi-%02hi-%02hi %02hi:%02hi:%02hi", ye, mo, da, ho, mi, se);
  return dt;
}

/**
 * Enlève la mesure la plus ancienne
 */
void removeLastMeasure() {
  idCapteur.remove(0);
  tauxCO2.remove(0);
  date.remove(0);
  nbPaquetsNonEnvoyes--;
}

/** 
 *  Récupère le temps "yyyy-mm-dd hh:mm:ss" à partir du serveur
 */
char* getServerTime() {
  char* t = "";
  HTTP_Request_GET(); 
  t=getRequestInformation(); 
  return t;
}

/**
* Récupère les informations d'une requête http 1.1
*/
char* getRequestInformation(){
  char* information=""; 
  while(client.connected()) {
    if(client.available()){
      // read an incoming byte from the server and print it to serial monitor:
      char c = client.read();
      information=information+c; 
    }
  }
  return information; 
}


/** Envoie les données au serveur
 * "idBatiment" -> ID_BATIMENT (Carte)
 * "idCapteur"  -> idCapteur (Capteur)
 * "tauxCO2"    -> tauxCO2 (Capteur)
 * "date"       -> date (Serveur ou carte)
 */
void envoiServeur() {
  // Si l'date a bien été reçue
  char* h = getServerTime();
  if (strcmp(h, "") != 0) {
    datetime dt = parseDateTimeString(h);
    date.push_back(datetimeToMs(dt));
  } else {
    unsigned long currentMillis = millis();
    unsigned long lastMillisB = 4294947295;
    if (currentMillis < previousMillis) currentMillis += lastMillisB - previousMillis;
    previousMillis = currentMillis % lastMillisB;
    date.push_back(date.back() + currentMillis);
  }
  String str = "?";
  char heure[16];
  sprintf(heure, "%lld", date.front());
  str = str +"idBatiment="+ ID_BATIMENT + "&" +"idCapteurs"+ idCapteur.front() + "&"+"tauxCO2" + tauxCO2.front() + "&"+"heure" + heure;
  HTTP_Request_POST(str);
  removeLastMeasure();
}


/**
 * Récuprère les données
 * Format:  "idCapteur;tauxCO2"
 */
void parseData() {
  // Premier champs : idCapteur (int)
  idCapteur.push_back(zigbeeSerial.parseInt());
  
  // Deuxième champs : tauxCO2 (short)
  tauxCO2.push_back(zigbeeSerial.parseInt());
}



// ----------------------------------------
//            setup & loop :
// ----------------------------------------

void setup() {
  
  Serial.begin(9600);
  zigbeeSerial.begin(115200);
  //set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  
  nbPaquetsNonEnvoyes = 0;
  
   if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to obtaining an IP address using DHCP");
    //while(true);
  }
  
}


void loop() {
  // Si un message est reçu :
  if(zigbeeSerial.available() > 0) {
    parseData();    // Parse puis ajoute les données dans les Arrays correspondants
    nbPaquetsNonEnvoyes++;
    envoiServeur(); // Envoie les données au serveur
  }

  //sleep_mode();
}