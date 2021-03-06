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
#define NB_MAX_TUPLES 20
#define INPUT_SIZE 31
#define DATE_TIME_SIZE 20
#define TAILLE_BUFFER 26
#define CAPTEUR_ID_SIZE 13


SoftwareSerial zigbeeSerial(7, 6);
unsigned long previousMillis = 0;

struct nomcapteur {
  char* value;
};

// Variables locales
char receivedData[INPUT_SIZE+1];  // INPUT_SIZE+1 bytes
Array<char*,              NB_MAX_TUPLES> idCapteur;
Array<short,              NB_MAX_TUPLES> tauxCO2;
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
    client.println(POST + " " + PATH_NAME_POST+queryString + " HTTP/1.1");
    client.println("Host: " + String(HOST_NAME));
    client.println("Connection: close");
    client.println(); // end HTTP header
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

    res[0] = malloc( sizeof(char) * 5);
    for (int p=0; p<4; p++) {
        res[0][p] = src[p];
    }
    res[0][4] = '\0';

    for (int i=1; i<6; i++) {
        res[i] = malloc( sizeof(char) * 3);
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
  short ye = timeMs/timeref/365.2425;  timeMs = timeMs - ye * 365.2425*timeref;
  short mo = timeMs/timeref/30.436875; timeMs = timeMs - mo * 30.436875*timeref;
  short da = timeMs/timeref; timeMs = timeMs - da * timeref; timeref /= 24;
  short ho = timeMs/timeref; timeMs = timeMs - ho * timeref; timeref /= 60;
  short mi = timeMs/timeref; timeMs = timeMs - mi * timeref;
  short se = timeMs;
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
  HTTP_Request_GET(); 
  return getRequestInformation();
}

/**
* Récupère les informations d'une requête http 1.1
*/
char* getRequestInformation(){
  Serial.println("getRequestInformation");
  delay(100);
  int length = 0;
  int compteur=0; 
  char* information = malloc(TAILLE_BUFFER * sizeof(char)); 
  while(client.connected()) {
    if(client.available()){
       // read an incoming byte from the server:
       char c = client.read();
       if(c!='{' && c!='}'){
        if(compteur==1){
         information[length] = c;
         length++;
        }
       }
       else{
        compteur++; 
       } 
    }
  }
  information[length] = '\0';

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
  h = "2022-06-02 08:08:00";
  if (strcmp(h, "") != 0) {
    Serial.write("Réponse serveur\n");
    datetime dt = parseDateTimeString(h);
    date.push_back(datetimeToMs(dt));
  } else {
    Serial.write("Aucune réponse serveur\n");
    unsigned long currentMillis = millis();
    unsigned long lastMillisB = 4294947295;
    if (currentMillis < previousMillis) currentMillis += lastMillisB - previousMillis;
    previousMillis = currentMillis % lastMillisB;
    //date.push_back(date.back() + currentMillis);
  }
  Serial.write("Préparation envoi données\n");
  char* heure =  msToDatetime(date.front());
  String str = "?";
  str = str +"idBatiment="+ ID_BATIMENT + "&" +"idCapteurs="+ idCapteur.front() + "&"+"tauxCO2=" + tauxCO2.front() + "&"+"heure=" + heure;
  short str_len = str.length()+1;
  char buff[str_len];
  str.toCharArray(buff, str_len);
  Serial.print(buff);
  HTTP_Request_POST(str);
  removeLastMeasure();
}


char* parseInput() {
  char* data = malloc(CAPTEUR_ID_SIZE * sizeof(char));
  short ite = 0;
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == ';' || c == -1 || ite>CAPTEUR_ID_SIZE) break;
    data[ite] = c;
    ite++;
    
  }
  data[ite] = '\0';
  return data;
}


/**
 * Récuprère les données
 * Format:  "idCapteur;tauxCO2"
 */
void parseData() {
  // Premier champs : idCapteur (int)
  idCapteur.push_back(parseInput());
  // Deuxième champs : tauxCO2 (short)
  //tauxCO2.push_back(zigbeeSerial.parseInt());
  tauxCO2.push_back(Serial.parseInt());
  Serial.flush();
}



// ----------------------------------------
//            setup & loop :
// ----------------------------------------

void setup() {
  
  Serial.begin(9600);
  Serial.write("Initialisation...\n");
  zigbeeSerial.begin(115200);
  //set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  
  nbPaquetsNonEnvoyes = 0;
  
  if (Ethernet.begin(mac, "192.168.246.22") == 0) {
    Serial.println("Failed to obtaining an IP address using DHCP");
    //while(true);
  }
  Serial.write("Fin d'initialisation, début programme...\n");
}


void loop() {
  // Si un message est reçu :
  if(zigbeeSerial.available() > 0 || Serial.available() > 1) {
    delay(100);
    Serial.write("Données reçues...\n");
    parseData();    // Parse puis ajoute les données dans les Arrays correspondants
    nbPaquetsNonEnvoyes++;
    envoiServeur(); // Envoie les données au serveur
  }

  //sleep_mode();
}
