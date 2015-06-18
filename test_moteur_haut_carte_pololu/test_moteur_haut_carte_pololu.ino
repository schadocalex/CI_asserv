#include <SimpleTimer.h>
SimpleTimer timer; // Timer pour échantillonnage

#define DEBUG
#define DEBUG_MOTORV
//#define DEBUG_MOTORP
#define borne(X, min, max) ((X < min) ? min : ((X > max) ? max : X))

////////////////////
// Variables
////////////////////

// Moteur V = Moteur asservi en vitesse
#define MOTORV_PWM 6
#define MOTORV_EN 50
float consigne_motorV = 3; // en tr/s
unsigned long time_old_tick = 0, time_new_tick = 0, time_old2_tick = 0; // Pour mesurer la vitesse du moteur
const float coeff_motorV = 465/2; // counts per revolution 48*9.68; // nombre de ticks par tour * reducteur
long tick_motorV = coeff_motorV; // Initialisation à 1 tr, il ne fera qu'un tour au démarrage au lieu de 2
float kp_motorV = 220;
float ki_motorV = 0;
float kd_motorV = 0;

// Moteur P = Moteur asservi en position
#define MOTORP_PWM 5
#define MOTORP_SENS_CW 52
#define MOTORP_SENS_CCW 53
#define MOTORP_PIN_A 2
#define MOTORP_PIN_B 3
float consigne_motorP = 3.2; // en mm
const float nb_ticks_rev_motorP = 1216;
const float rayon_motorP = 11; // en mm
const float coeff_motorP = rayon_motorP * 2*PI/nb_ticks_rev_motorP;
long tick_motorP = (int) ((float)consigne_motorP/(float)coeff_motorP) + 1;
float kp_motorP = 28.5;
float ki_motorP = 0.35;
float kd_motorP = 0;

// Asservissement
const int frequence_echantillonnage = 200;

////////////////////
// setup() & loop()
////////////////////

void setup() {
  // Motor V
  pinMode(MOTORV_PWM, OUTPUT);
  pinMode(MOTORV_EN, OUTPUT);
  analogWrite(MOTORV_PWM, 20);
  digitalWrite(MOTORV_EN, HIGH);
  
  // Motor P
  pinMode(MOTORP_PWM, OUTPUT);
  pinMode(MOTORP_SENS_CW, OUTPUT);
  pinMode(MOTORP_SENS_CCW, OUTPUT);
  analogWrite(MOTORP_PWM, 0);
  digitalWrite(MOTORP_SENS_CW, HIGH);
  digitalWrite(MOTORP_SENS_CCW, LOW);

  // Interruptions
    // Moteur V
  attachInterrupt(2, interruption_motorV, CHANGE); // Interruption sur tick de la codeuse (interruption 2 = pin 21 Arduino Mega)
    // Moteur P
  pinMode(MOTORP_PIN_A, INPUT);
  pinMode(MOTORP_PIN_B, INPUT);
  attachInterrupt(0, interruption_motorP_A, CHANGE); // Interruption sur tick de la codeuse (interruption 0 = pin 2 Arduino Mega)
  attachInterrupt(1, interruption_motorP_B, CHANGE); // Interruption sur tick de la codeuse (interruption 1 = pin 3 Arduino Mega)
    // Asservissement
  timer.setInterval(1000/frequence_echantillonnage, asservissement);
  
  Serial.begin(57600);
  delay(300); // Pause de 0,3 sec pour laisser le temps au moteur de s'arréter si celui-ci est en marche
}

void loop()
{
  loopCommunication();
  timer.run();
}

////////////////////
// Odométrie
////////////////////

void interruption_motorV()
{
  time_old2_tick = time_old_tick;
  time_old_tick = time_new_tick;
  time_new_tick = micros();
  tick_motorV++;
}

void interruption_motorP_A()
{
   // look for a low-to-high on channel A
   if (digitalRead(MOTORP_PIN_A) == HIGH) { 
    // check channel B to see which way encoder is turning
    if (digitalRead(MOTORP_PIN_B) == LOW) {  
      tick_motorP++;
    } 
    else {
      tick_motorP--;         // CCW
    }
  }
  else   // must be a high-to-low edge on channel A                                       
  { 
    // check channel B to see which way encoder is turning  
    if (digitalRead(MOTORP_PIN_B) == HIGH) {   
      tick_motorP++;          // CW
    } 
    else {
      tick_motorP--;          // CCW
    }
  }
}
void interruption_motorP_B()
{
  // look for a low-to-high on channel B
  if (digitalRead(MOTORP_PIN_B) == HIGH) {   
   // check channel A to see which way encoder is turning
    if (digitalRead(MOTORP_PIN_A) == HIGH) {  
      tick_motorP++;         // CW
    } 
    else {
      tick_motorP--;         // CCW
    }
  }
  // Look for a high-to-low on channel B
  else { 
    // check channel B to see which way encoder is turning  
    if (digitalRead(MOTORP_PIN_A) == LOW) {   
      tick_motorP++;          // CW
    } 
    else {
      tick_motorP--;          // CCW
    }
  }
}

////////////////////
// Asservissement
////////////////////

int update_data = 0;
void asservissement()
{
  static unsigned long old_t = 0;
  
  if(millis() - old_t > 50) {
    old_t = millis();
    update_data = 1;
  }
    
  #ifdef DEBUG
    if(update_data) {
      Serial.print('U');
    }
  #endif
  
  asservissement_motorV();
  
  #ifdef DEBUG
    if(update_data) {
      Serial.print(';');
    }
  #endif
  
  asservissement_motorP();
  
  #ifdef DEBUG
    if(update_data) {
      Serial.print('\n');
    }
  #endif
  
  update_data = 0;
}

void asservissement_motorV()
{
  static float previous_error = consigne_motorV;
  static float sum_error = 0;
  static int pwm = 0;

  // Calcul de l'erreur
  float speed = 1000000/((float)(time_new_tick-time_old2_tick)/2 * coeff_motorV);
  if(speed > 10000) speed = 0; // Infini (au démarrage) -> 0

  float error = consigne_motorV - speed; // Proportionnel
  sum_error += error; // Intégrale
  sum_error = borne(sum_error, 0, 100);
  float delta_error = error - previous_error; // Dérivée
  previous_error = error;

  pwm = borne(kp_motorV*error + ki_motorV*sum_error + kd_motorV*delta_error, 0, 255);
  analogWrite(MOTORV_PWM, pwm);

  #ifdef DEBUG
    if(update_data) {
      Serial.print(speed, 4);
    }
  #endif
}

void asservissement_motorP()
{
  static float previous_error = consigne_motorV;
  static float sum_error = 0;
  static int pwm = 0;
  
  // On modifie la consigne si on doit bouger le moteur
  if(tick_motorV >= 2*coeff_motorV)
  {
    tick_motorV -= 2*coeff_motorV;
    tick_motorP -= (int) ((float)consigne_motorP/(float)coeff_motorP) + 1;
    sum_error = 0;
    previous_error = consigne_motorV; 
  }
  
  // Calcul de l'erreur
  float pos = tick_motorP * coeff_motorP;
  
  float error = consigne_motorP - pos; // Proportionnel
  sum_error += error; // Intégrale
  sum_error = borne(sum_error, -100, 100);
  float delta_error = error - previous_error; // Dérivée
  previous_error = error;

  pwm = borne(kp_motorP*error + ki_motorP*sum_error + kd_motorP*delta_error, -255, 255);
  if(pwm >= 0) {
    digitalWrite(MOTORP_SENS_CW, HIGH);
    digitalWrite(MOTORP_SENS_CCW, LOW);
    analogWrite(MOTORP_PWM, pwm);
  } else {
    digitalWrite(MOTORP_SENS_CW, LOW);
    digitalWrite(MOTORP_SENS_CCW, HIGH);
    analogWrite(MOTORP_PWM, -pwm);
  }

  #ifdef DEBUG
    if(update_data) {
      Serial.print(pos, 4);
    }
  #endif
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
        kp_motorV = (float)f1/1000;
        ki_motorV = (float)f2/1000;
        kd_motorV = (float)f3/1000;
        #ifdef DEBUG 
          Serial.print('D');
          Serial.print("change PID moteur V : ");
          Serial.print(kp_motorV,4); Serial.print(' ');
          Serial.print(ki_motorV,4); Serial.print(' ');
          Serial.print(kd_motorV,4); Serial.print(' ');
          Serial.print('\n');
        #endif
      }
    break;
    case 'Q': // PID
      {
        long f1, f2, f3;
        sscanf(buffer, "Q;%ld;%ld;%ld", &f1, &f2, &f3);
        kp_motorP = (float)f1/1000;
        ki_motorP = (float)f2/1000;
        kd_motorP = (float)f3/1000;
        #ifdef DEBUG 
          Serial.print('D');
          Serial.print("change PID moteur P : ");
          Serial.print(kp_motorP,4); Serial.print(' ');
          Serial.print(ki_motorP,4); Serial.print(' ');
          Serial.print(kd_motorP,4); Serial.print(' ');
          Serial.print('\n');
        #endif
      }
    break;
    case 'C': // Consigne
      {
        long f1;
        sscanf(buffer, "C;%ld", &f1);
        consigne_motorV = (float)f1/1000;
        #ifdef DEBUG 
          Serial.print('D');
          Serial.print("change consigne moteur V : ");
          Serial.print(consigne_motorV,3); Serial.print(" tr/s");
          Serial.print('\n');
        #endif
      }
    break;
    case 'D': // Consigne
      {
        long f1;
        sscanf(buffer, "D;%ld", &f1);
        consigne_motorP = (float)f1/1000;
        #ifdef DEBUG 
          Serial.print('D');
          Serial.print("change consigne moteur P : ");
          Serial.print(consigne_motorP); Serial.print(" mm");
          Serial.print('\n');
        #endif
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
