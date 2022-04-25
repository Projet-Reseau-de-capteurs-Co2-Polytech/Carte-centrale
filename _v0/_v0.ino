#define INPUT_SIZE 31
#define ID_BATIMENT 22001

char receivedData[INPUT_SIZE+1];
int idCapteur;
float tauxCO2;


void setup() {
  Serial.begin(9600);
  // TODO
}


void envoiServeur() {
  // TODO
}


char* getServerTime() {
  // TODO
}


void loop() {

  // Si un message est reçu :
  if(Serial.available() > 0) {
    // Récupération du message
    //  Format:  "idCapteur;tauxCO2"
    receivedData = Serial.readString().toCharArray();

    // Premier champs : idCapteur (int)
    char* data = strtok(receivedData,";");
    idCapteur = parseInt(data);

    // Deuxième champs : tauxCO2 (float)
    data = strtok(receivedData,";");
    tauxCO2 = parseFloat(data);

    envoiServeur();
  }

}
