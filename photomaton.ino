#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#ifndef PSTR
 #define PSTR // Make Arduino Due happy
#endif

#define PIN_LED_CMD 6      /* Pin de commande du panneau de LEDs */
#define PIN_BOUTON_PIED A0 /* Pin pour récupérer l'état du bouton de déclenchement au pied */
#define PIN_POTAR_G     A5 /* Non utilisé dans le projet */
#define PIN_POTAR_M     A4 /* Pin pour récupérer l'intensité lumineuse à afficher */
#define PIN_POTAR_D     A3 /* Pin pour récupérer la couleur à afficher */
#define PIN_BOUTON      A1 /* Pin pour récupérer l'état du bouton de déclenchement façade */
#define PIN_JACK        A2 /* Pin de commande du port jack du téléphone */

/* Seuils de couleurs définis en fonction de la position du potar_D (couleur) */
#define POTAR_D_SEUIL_0 0
#define POTAR_D_SEUIL_1 12
#define POTAR_D_SEUIL_2 25
#define POTAR_D_SEUIL_3 40
#define POTAR_D_SEUIL_4 55
#define POTAR_D_SEUIL_5 85
#define POTAR_D_SEUIL_6 125
#define POTAR_D_SEUIL_7 190

/* Configuration du panneau de LEDs */
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(5, 8, PIN_LED_CMD,
  NEO_MATRIX_TOP     + NEO_MATRIX_RIGHT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_PROGRESSIVE,
  NEO_GRB            + NEO_KHZ800);

unsigned  long temps_yeux; /* mesure du temps pour gérer l'animation des yeux dans le flash */
int bouton_old; /* mémorisation du dernier état du bouton façade */
int lumiere_save; /* valeur n-1 de l'intensité afin de la comparer à la valeur N pour coder un hystérésis pour palier aux clignotements liés à la mesure ADC */
int couleur_save; /* valeur n-1 de la couleur afin de la comparer à la valeur N pour coder un hystérésis pour palier aux clignotements liés à la mesure ADC */
int lumiere_save2; /* Dernière valeur valide de lumière, permet de restaurer cette luminosité lorsque le flash sort de sa veille */
int couleur_save2; /* Dernière valeur valide de couleur, permet de restaurer cette couleur lorsque le flash sort de sa veille */
unsigned  long temps; /* Mesure du temps pour éviter au téléphone de se mettre en veille */
unsigned  long veille; /* Mesure du temps afin de gérer la mise en veille du flash */
int lumiere; /* valeur de la lumière après lecture du potentiomètre */
int lumiere2; /* valeur à afficher au flash en prenant en compte la veille du flash */
int couleur_index; /* index de la couleur à prendre dans le tableau couleur*/
int couleur_potar; /* valeur de la couleur après lecture du potentiomètre */

/* Plage de couleurs en fonction des seuils POTAR_D_SEUIL_x */
int couleur[8][3]={
{0,0,0},  
{0,0,255},
{0,255,0},
{0,255,255},
{255,0,0},
{255,0,255},
{255,255,0},
{255,255,255}
};

/* Plage de couleurs en fonction des seuils POTAR_D_SEUIL_x pour dessiner des yeux dans le flash*/
int couleur2[8][3]={
{255,255,255},  
{255,255,255},  
{255,255,255},  
{255,255,255},  
{255,255,255},  
{255,255,255},  
{255,255,255},  
{0,0,0},  
};

/* Envoi de la couleur RGB définie au panneau de LEDs */
void flash(int R,int G,int B){
  for(uint16_t row=0; row < 8; row++) {
    for(uint16_t column=0; column < 5; column++) {
      matrix.drawPixel(column, row, matrix.Color(R,G, B));     
    }
  }
}

/* Permet de dessiner des yeux mouvants dans le flash en fonction du temps écoulé */
void flash2(int R,int G,int B){
  unsigned  long delta;
  delta=millis()-temps_yeux;
  matrix.drawPixel(3, 2, matrix.Color(R,G, B));
  matrix.drawPixel(3, 5, matrix.Color(R,G, B));
  
  if(delta>=0 && delta<=10000){
    matrix.drawPixel(3, 1, matrix.Color(R,G, B));
    matrix.drawPixel(3, 6, matrix.Color(R,G, B));
  }
  if(delta>10000 && delta<=11000){
    matrix.drawPixel(3, 1, matrix.Color(R,G, B));
    matrix.drawPixel(3, 6, matrix.Color(R,G, B));
    matrix.drawPixel(2, 2, matrix.Color(R,G, B));
    matrix.drawPixel(2, 5, matrix.Color(R,G, B));    
  }
  if(delta>11000 && delta<=21000){
    matrix.drawPixel(3, 1, matrix.Color(R,G, B));
    matrix.drawPixel(3, 6, matrix.Color(R,G, B));
  }
  if(delta>21000 && delta<=22000){
    matrix.drawPixel(4, 1, matrix.Color(R,G, B));
    matrix.drawPixel(4, 6, matrix.Color(R,G, B));
    matrix.drawPixel(3, 1, matrix.Color(R,G, B));
    matrix.drawPixel(3, 6, matrix.Color(R,G, B));
  }
  if(delta>22000 || delta<0){
    temps_yeux=millis();
  }
}

/* Scrute les appuis bouton pied et les changements d'états du bouton façade, renvoi  1 si appui détecté, sinon renvoi 0 */
/* Le bouton façade reste enfoncé lorsqu'il est appuyé, il faut détecter ses transitions et non son état */
int read_bouton(void){
  int bouton_ana;
  int bouton_ana_pied;
  int retour;
  int bouton;
  bouton_ana=analogRead(PIN_BOUTON);
  bouton_ana_pied=analogRead(PIN_BOUTON_PIED);
  if(bouton_ana<512){
    bouton=0;
  }else{
    bouton=1;
  }
  if(bouton!=bouton_old){
    retour=1;
    bouton_old=bouton;
  }else{
    bouton_old=bouton;
    retour=0;
  }
  if(bouton_ana_pied>512){
    retour=1;
  }
  return retour;
}

/* Mémorise l'état actuel du bouton façade */
void reset_bouton(void){
  int bouton;
  int bouton_ana=analogRead(PIN_BOUTON);
  if(bouton_ana<512){
    bouton=0;
  }else{
    bouton=1;
  }
  bouton_old=bouton;
}

/* Lecture de la couleur*/
/* Comparaison avec la valeur précédente pour éviter les clignotements intempestifs liés à la précision ADC (codage d'un hystérésis simple) */
int read_potar_droit(void){
  int32_t potar_D;
  potar_D=analogRead(PIN_POTAR_D);
  potar_D=(830-potar_D);
  potar_D=potar_D*255/(830-370);
  if(potar_D<0){potar_D=0;}
  if(potar_D>255){potar_D=255;}
  if(potar_D<=(couleur_save+4) && potar_D>=(couleur_save-4)){
    potar_D=couleur_save;
  }else{
    couleur_save=potar_D;
  }
  return potar_D;
}

/* non utilisé*/
int read_potar_gauche(void){
  int32_t potar_G;
  potar_G=analogRead(PIN_POTAR_G);
  potar_G=(820-potar_G);
  potar_G=potar_G*255/(820-320);
  if(potar_G<0){potar_G=0;}
  if(potar_G>255){potar_G=255;}
  return potar_G;
  
}

/* Lecture de l'intensité*/
/* Comparaison avec la valeur précédente pour éviter les clignotements intempestifs liés à la précision ADC (codage d'un hystérésis simple) */
int read_potar_milieu(void){
  int32_t potar_M;
  potar_M=analogRead(PIN_POTAR_M);
  potar_M=(830-potar_M);
  potar_M=potar_M*255/(830-330);
  if(potar_M<0){potar_M=0;}
  if(potar_M>255){potar_M=255;}
  if(potar_M<=(lumiere_save+4) && potar_M>=(lumiere_save-4)){
    potar_M=lumiere_save;
  }else{
    lumiere_save=potar_M;
  }
    return potar_M;
}

void setup() {
  Serial.begin(250000); /* non utilisé - Debug */
  matrix.begin();
  matrix.setBrightness(255);
  matrix.setTextColor( matrix.Color(255, 255, 255) );
  matrix.setTextWrap(false);
  temps_yeux=millis();
  flash(0,0,0);
  flash2(0,0,0);
  matrix.show();
  reset_bouton();
  lumiere_save=0;
  couleur_save=0;
  pinMode(PIN_JACK, OUTPUT);
  digitalWrite(PIN_JACK,HIGH);
  delay(300);
  digitalWrite(PIN_JACK,LOW);
  temps=millis();
  veille=millis();
  lumiere=0;
  couleur_index=0;
  couleur_potar=0;
}

void loop() {

/* Déclenchement automatique d'un cliché pour éviter le mode veille du téléphone */
if((millis()-temps>570000) || (millis()-temps<0)){
  digitalWrite(PIN_JACK,HIGH);
  delay(300);
  digitalWrite(PIN_JACK,LOW);
  temps=millis();
}

/* Gestion de la veille du flash */
lumiere_save2=lumiere;
couleur_save2=couleur_potar;
couleur_potar=read_potar_droit();
lumiere=read_potar_milieu();
if((couleur_save2==couleur_potar) && (lumiere_save2==lumiere)){
}else{
  veille=millis();
}
Serial.println(millis()-veille);
if(millis()-veille>=60000){
  lumiere2=10;
}else{
  lumiere2=lumiere; 
}

/* Choix de la couleur en fonction des seuils de couleurs */
couleur_index=0;
if(couleur_potar>POTAR_D_SEUIL_0 && couleur_potar<=POTAR_D_SEUIL_1){
  couleur_index=0;
}
if(couleur_potar>POTAR_D_SEUIL_1 && couleur_potar<=POTAR_D_SEUIL_2){
  couleur_index=1;
}
if(couleur_potar>POTAR_D_SEUIL_2 && couleur_potar<=POTAR_D_SEUIL_3){
  couleur_index=2;
}
if(couleur_potar>POTAR_D_SEUIL_3 && couleur_potar<=POTAR_D_SEUIL_4){
  couleur_index=3;
}
if(couleur_potar>POTAR_D_SEUIL_4 && couleur_potar<=POTAR_D_SEUIL_5){
  couleur_index=4;
}
if(couleur_potar>POTAR_D_SEUIL_5 && couleur_potar<=POTAR_D_SEUIL_6){
  couleur_index=5;
}
if(couleur_potar>POTAR_D_SEUIL_6 && couleur_potar<=POTAR_D_SEUIL_7){
  couleur_index=6;
}
if(couleur_potar>POTAR_D_SEUIL_7){
  couleur_index=7;
}

matrix.setBrightness(lumiere2);
flash(couleur[couleur_index][0],couleur[couleur_index][1],couleur[couleur_index][2]);
flash2(couleur2[couleur_index][0],couleur2[couleur_index][1],couleur2[couleur_index][2]);
matrix.show();

/* Déclenchement d'une photo si un appui bouton est détecté */
if(read_bouton()){
  matrix.setBrightness(lumiere);
  flash(couleur[couleur_index][0],couleur[couleur_index][1],couleur[couleur_index][2]);
  flash2(couleur2[couleur_index][0],couleur2[couleur_index][1],couleur2[couleur_index][2]);
  matrix.show();
  digitalWrite(PIN_JACK,HIGH);
  delay(300);
  digitalWrite(PIN_JACK,LOW);
  temps=millis();
  delay(3000);
  matrix.setBrightness(0);
  flash(0,0,0);
  matrix.show();
  delay(50);
  matrix.setBrightness(lumiere);
  flash(couleur[couleur_index][0],couleur[couleur_index][1],couleur[couleur_index][2]);
  flash2(couleur2[couleur_index][0],couleur2[couleur_index][1],couleur2[couleur_index][2]);
  matrix.show();
  reset_bouton();
  veille=millis();
}
}
