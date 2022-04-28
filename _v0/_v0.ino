/**
 * Code de la carte centrale
 * 
 * Fonctionnalités :
 *  - Récupérer les données de différents capteurs d'un même réseau Zigbee
 *  - Calculer ou recevoir les données complémentaires
 *  - Envoyer les données complètes au serveur via API
 */


// ----------------------------------------
//      Importations & Déclaration :
// ----------------------------------------
#include <Arduino.h>
#include <Wire.h>
#include <Ethernet.h>

#define INPUT_SIZE 31
#define ID_BATIMENT 22001

// Variables locales
char receivedData[INPUT_SIZE+1];
int idCapteur;
float tauxCO2;
char* heure;


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
    heure = h;
  } else {
    // Garder la valeur précédente ou faire ...
  }
  
  
  // TODO
}

/** 
 *  Récupère le temps "hh:mm:ss" à partir du serveur
 */
char* getServerTime() {
  
  // TODO
  
  return heure;
}


/**
 * Récuprère les données
 */
void parseData() {
  // Récupération du message
  //  Format:  "idCapteur;tauxCO2"
  String str = Serial.readString();
  str.toCharArray(receivedData, str.length()+1);
  
  // Premier champs : idCapteur (int)
  char* data = strtok(receivedData,";");
  idCapteur = atoi(data);
  
  // Deuxième champs : tauxCO2 (float)
  data = strtok(receivedData,";");
  tauxCO2 = atof(data);
}

/**
 * Récuprère les données
 */
void parseData_v2() {
  //  Format:  "idCapteur;tauxCO2"
  
  // Premier champs : idCapteur (int)
  idCapteur = Serial.parseInt();
  
  // Deuxième champs : tauxCO2 (float)
  tauxCO2 = Serial.parseFloat();
}



// ----------------------------------------
//            setup & loop :
// ----------------------------------------

void setup() {
  Serial.begin(9600);
  // TODO
}

void loop() {
  // Si un message est reçu :
  if(Serial.available() > 0) {
    parseData_v2();
    envoiServeur();
  }

}
