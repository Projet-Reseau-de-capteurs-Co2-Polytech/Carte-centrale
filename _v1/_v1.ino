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
 *    - heure
 */


// ----------------------------------------
//      Importations & Déclaration :
// ----------------------------------------
#include <Arduino.h>
#include <Wire.h>
#include <Ethernet.h>
#include <avr/sleep.h>
#include <Array.h>

/**
 * Mémoire Arduino Uno = 2k bytes (SRAM)
 * Array idCapteurs    = x * 4 bytes
 * Array tauxCO2       = x * 4 bytes
 * Array heure         = x * 20 bytes
 *  => Stockage max de environ 70 tuples de données
 *  => nb tuples choisi = 60 pour éviter une collision heap/stack
 */

#define ID_BATIMENT 22001
#define INPUT_SIZE 31
#define DATE_TIME_SIZE 20
#define NB_MAX_TUPLES 60



struct datetime {
  // date-time format : "yyyy-mm-dd hh:mm:ss"
  char value[DATE_TIME_SIZE];
};

// Variables locales
char receivedData[INPUT_SIZE+1];  // INPUT_SIZE+1 bytes
Array<int,      NB_MAX_TUPLES> idCapteur;
Array<float,    NB_MAX_TUPLES> tauxCO2;
Array<datetime, NB_MAX_TUPLES> heure;

int nbPaquetsNonEnvoyes;


// ----------------------------------------
//                Fonctions
// ----------------------------------------
/** Envoie les données au serveur
 * "idBatiment" -> ID_BATIMENT (Carte)
 * "idCapteur"  -> idCapteur (Capteur)
 * "tauxCO2"    -> tauxCO2 (Capteur)
 * "heure"      -> heure (Serveur)
 */
void envoiServeur() {
  // Si l'heure a bien été reçue
  char* h = getServerTime();
  if (strcmp(h, "") != 0) {
    datetime dt;
    strcpy(dt.value, h);
    heure.push_back(dt);
  } else {
    // calculer le temps écoulé depuis le dernier temps reçu
    unsigned long timer = millis();
    datetime dt;
    itoa(timer, dt.value, 10); // <- à changer
    heure.push_back(dt);
  }

  /**
   * Si connexion serveur :
   */
  {
    String str = "";
    str = str + ID_BATIMENT + ";" + idCapteur.front() + ";" + tauxCO2.front() + ";" + heure.front().value + ";";
    // TODO : Envoyer str au serveur
    idCapteur.remove(0);
    tauxCO2.remove(0);
    heure.remove(0);
    nbPaquetsNonEnvoyes--;
  }
}


/** 
 *  Récupère le temps "yyyy-mm-dd hh:mm:ss" à partir du serveur
 */
char* getServerTime() {
  char* t = "";
  // TODO
  
  return t;
}

/**
 * Récuprère les données
 * Format:  "idCapteur;tauxCO2"
 */
void parseData() {
  // Premier champs : idCapteur (int)
  idCapteur.push_back(Serial.parseInt());
  
  // Deuxième champs : tauxCO2 (float)
  tauxCO2.push_back(Serial.parseFloat());
}



// ----------------------------------------
//            setup & loop :
// ----------------------------------------

void setup() {
  Serial.begin(9600);
  //set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  
  nbPaquetsNonEnvoyes = 0;
}


void loop() {
  // Si un message est reçu :
  if(Serial.available() > 0) {
    parseData();    // Parse puis ajoute les données dans les Arrays correspondants
    nbPaquetsNonEnvoyes++;
    envoiServeur(); // Envoie les données au serveur
  }

  //sleep_mode();
}
