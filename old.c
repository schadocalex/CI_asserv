#include <SimpleTimer.h>

SimpleTimer timer; // Timer pour échantillonnage

// Moteur V = Moteur asservi en vitesse
int vitMoteur = 0;
unsigned long time_old_tick = 0, time_new_tick = 0, time_old2_tick = 0;

const int frequence_echantillonnage = 200;  // 20 Fréquence d'exécution de l'asservissement
const float rapport_reducteur = 9.68; //9.7         // Rapport entre le nombre de tours de l'arbre moteur et de la roue
const int tick_par_tour_codeuse = 48;  //48 tick sur deux capteurs hall, ici on a pris un seul capteur


//definition des entrées
int Motcansens1 = 52; // Commande de sens moteur, Input 1
int Motcansens2 = 53; // Commande de sens moteur, Input 2
int MoteurcanPWM =  5; // Commande de vitesse moteur, Output Enabled1

//consigne en tour/s
float consigne_Motcan= 2;  // 3; 0,17  //  Consigne nombre de tours de roue par seconde

// init calculs asservissement PID
float erreur_precedente = consigne_Motcan; // (en tour/s)
float somme_erreur = 0;

//Definition des constantes du correcteur PID
float kp = 200;           // Coefficient proportionnel    choisis par tatonnement sur le moniteur. Ce sont les valeurs qui donnaient les meilleures performances
float ki = 0;      // Coefficient intégrateur
float kd = 0.005;          // Coefficient dérivateur


/* Routine d'initialisation */
void setup() {
  Serial.begin(57600);    //115200 ou 9600    // Initialisation port COM
  pinMode(MoteurcanPWM, OUTPUT);   // Sorties commande moteur
  pinMode(Motcansens1, OUTPUT );
  pinMode(Motcansens2, OUTPUT );
  
  digitalWrite( Motcansens1, HIGH );
  digitalWrite( Motcansens2, LOW );
  
  analogWrite(MoteurcanPWM, 0);  // Initialisation sortie moteur à 0 
  delay(300);                // Pause de 0,3 sec pour laisser le temps au moteur de s'arréter si celui-ci est en marche

  attachInterrupt(0, compteur, CHANGE);    // Interruption sur tick de la codeuse  (interruption 0 = pin2 arduino)
  timer.setInterval(1000/frequence_echantillonnage, asservissement);  // Interruption pour calcul du PID et asservissement; toutes les 10ms, on recommence la routine
}


/* Fonction principale */
void loop(){
  loopCommunication();
  timer.run();  //on fait tourner l'horloge
  //delay(10);   
  
}

/* Interruption sur tick de la codeuse */
void compteur(){
  // tick_codeuse++; 
  // On incrémente le nombre de tick de la codeuse.   un seul sens

  //2ème méthode
  time_old2_tick = time_old_tick;
  time_old_tick = time_new_tick;
  time_new_tick = micros();
}


/* Interruption pour calcul du P */
void asservissement()
{
  static unsigned long old_t = 0;
  // Calcul de l'erreur
  float tr_sec = 1000000/((float)(time_new_tick - time_old2_tick)/2*(float)tick_par_tour_codeuse*(float)rapport_reducteur);
  if(tr_sec > 100) tr_sec = 0;
  
  float erreur = (float)consigne_Motcan - tr_sec;//vit_roue_tour_sec; // pour le proportionnel
  somme_erreur += erreur; // pour l'intégrateur
  float delta_erreur = erreur-erreur_precedente;  // pour le dérivateur
  erreur_precedente = erreur;
        
  // Réinitialisation du nombre de tick de la codeuse
  //tick_codeuse=0;

  // P : calcul de la commande
  vitMoteur = kp*erreur + ki*somme_erreur + kd*delta_erreur;  //somme des tois erreurs

 
  if(vitMoteur < 0) {
  vitMoteur=0;
  }
  else if(vitMoteur > 255){
    vitMoteur = 255;
  }
  
  analogWrite(MoteurcanPWM, vitMoteur);
  if(millis() - old_t > 50) {
    old_t = millis();
    Serial.print('U');
    Serial.print(tr_sec,4);
    Serial.print('\n');
  }

}

////////////////////
// Communication
////////////////////

char buffer[100] = "";
short i_buffer = 0;
char c_in;

void parseCommand() {
  switch(buffer[0]) {
    case 'P': // PID
      {
        long f1, f2, f3;
        sscanf(buffer, "P;%ld;%ld;%ld", &f1, &f2, &f3);
        kp = (float)f1/1000;
        kd = (float)f2/1000;
        ki = (float)f3/1000;
        Serial.print('D');
        Serial.print("change PID: ");
        Serial.print(kp,3); Serial.print(' ');
        Serial.print(kd,3); Serial.print(' ');
        Serial.print(ki,3); Serial.print(' ');
        Serial.print('\n');
      }
    break;
    case 'C': // PID
      {
        long f1;
        sscanf(buffer, "C;%ld", &f1);
        consigne_Motcan = (float)f1/1000;
        Serial.print('D');
        Serial.print("change consigne: ");
        Serial.print(consigne_Motcan,3); Serial.print(" tr/s");
        Serial.print('\n');
      }
    break;
    default:
    break;
  }
}

void loopCommunication() {
  if(Serial.available() > 0) {
    c_in = Serial.read();
    if(c_in == '\n') {
      buffer[i_buffer] = '\0';
      if(i_buffer > 0) {
        parseCommand();
      }
      i_buffer = 0;
    } else {
      buffer[i_buffer] = c_in;
      i_buffer++;
    }
  }
}