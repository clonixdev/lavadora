// version 1.1 de la lavadora con arduino
// la valvula de entrada de agua o val1 estara conectada al pin3
// el giro del motor 1 o giro1 en el sentido contrario a la manecillas el reloj, estara conectado al pin 4
// el giro del motor 2 o giro2 en el sentido de las manecillas el reloj, estara conectado al pin 5
// la bomba de agua estara conectada junto del el bloqueo del tanque ya que solo dispongo de 4 reles, en el pin 6
// el modulo de reles se activa con os pines en bajo, o LOW, o 0 logico.
// este codigo fue creado por Santiago Eusse Toro.

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "pitches.h"

// ESTE ARREGLO DETERMINA EN QUE ESTADO SE ENCUENTRA NUESTRA LAVADORA
String ciclos[6 ] = {"LLENANDO",
                     "LAVANDO",
                     "VACIANDO",
                     "ACELERAR",
                     "CENTRIFUGANDO",
                     "ESPERA"
                    };

bool led = true;
unsigned long hora = 0;
const int intervalo = 1000;

int contador = 0;
int segundos = 0;
int minuto = 0;

String ciclo;    // VARIABLE PARA LA ACTUALIZACION DEL CICLO ENLA PANTALL
int paso = 0;    // REGISTRO DE PASO PARA EL LAVADO Y EL CICLO DE ACELERACION DEL TANQUE

int sttone = 0;
int val1 =  8;    // VALVULA DE ENTRADA DE AGUA
int giro = 5;    // GIRO DEL MOTOR
int vel1 = 6;    // VELOCIDAD DE LAVADO
int vel2 = 7;    // VELOCIDAD DE CENTRIFUGADO
int motor = 4;    // ENCENDER MOTOR
int bomba = 9;    // BOMBA DE AGUA
int bloqueo = 10;  // BLOQUEO DE PUERTA
int alarma = 11;   // ALARMA BUZZER PARA FIN DE LAVADO 


// si la direccion 0x27 no funciona, prueba con 0x28 ó con 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2);

int melody[] = {
  NOTE_E4, NOTE_G4, NOTE_A4, NOTE_A4, REST,
  NOTE_A4, NOTE_B4, NOTE_C5, NOTE_C5, REST,
  NOTE_C5, NOTE_D5, NOTE_B4, NOTE_B4, REST,
  NOTE_A4, NOTE_G4, NOTE_A4, REST,
  
  NOTE_E4, NOTE_G4, NOTE_A4, NOTE_A4, REST,
  NOTE_A4, NOTE_B4, NOTE_C5, NOTE_C5, REST,
  NOTE_C5, NOTE_D5, NOTE_B4, NOTE_B4, REST,
  NOTE_A4, NOTE_G4, NOTE_A4, REST,
  
  NOTE_E4, NOTE_G4, NOTE_A4, NOTE_A4, REST,
  NOTE_A4, NOTE_C5, NOTE_D5, NOTE_D5, REST,
  NOTE_D5, NOTE_E5, NOTE_F5, NOTE_F5, REST,
  NOTE_E5, NOTE_D5, NOTE_E5, NOTE_A4, REST,
  
  NOTE_A4, NOTE_B4, NOTE_C5, NOTE_C5, REST,
  NOTE_D5, NOTE_E5, NOTE_A4, REST,
  NOTE_A4, NOTE_C5, NOTE_B4, NOTE_B4, REST,
  NOTE_C5, NOTE_A4, NOTE_B4, REST,
  
  NOTE_A4, NOTE_A4,
  //Repeat of first part
  NOTE_A4, NOTE_B4, NOTE_C5, NOTE_C5, REST,
  NOTE_C5, NOTE_D5, NOTE_B4, NOTE_B4, REST,
  NOTE_A4, NOTE_G4, NOTE_A4, REST,
  
  NOTE_E4, NOTE_G4, NOTE_A4, NOTE_A4, REST,
  NOTE_A4, NOTE_B4, NOTE_C5, NOTE_C5, REST,
  NOTE_C5, NOTE_D5, NOTE_B4, NOTE_B4, REST,
  NOTE_A4, NOTE_G4, NOTE_A4, REST,
  
  NOTE_E4, NOTE_G4, NOTE_A4, NOTE_A4, REST,
  NOTE_A4, NOTE_C5, NOTE_D5, NOTE_D5, REST,
  NOTE_D5, NOTE_E5, NOTE_F5, NOTE_F5, REST,
  NOTE_E5, NOTE_D5, NOTE_E5, NOTE_A4, REST,
  
  NOTE_A4, NOTE_B4, NOTE_C5, NOTE_C5, REST,
  NOTE_D5, NOTE_E5, NOTE_A4, REST,
  NOTE_A4, NOTE_C5, NOTE_B4, NOTE_B4, REST,
  NOTE_C5, NOTE_A4, NOTE_B4, REST,
  //End of Repeat
  
  NOTE_E5, REST, REST, NOTE_F5, REST, REST,
  NOTE_E5, NOTE_E5, REST, NOTE_G5, REST, NOTE_E5, NOTE_D5, REST, REST,
  NOTE_D5, REST, REST, NOTE_C5, REST, REST,
  NOTE_B4, NOTE_C5, REST, NOTE_B4, REST, NOTE_A4,
  
  NOTE_E5, REST, REST, NOTE_F5, REST, REST,
  NOTE_E5, NOTE_E5, REST, NOTE_G5, REST, NOTE_E5, NOTE_D5, REST, REST,
  NOTE_D5, REST, REST, NOTE_C5, REST, REST,
  NOTE_B4, NOTE_C5, REST, NOTE_B4, REST, NOTE_A4
};

int durations[] = {
  8, 8, 4, 8, 8,
  8, 8, 4, 8, 8,
  8, 8, 4, 8, 8,
  8, 8, 4, 8,
  
  8, 8, 4, 8, 8,
  8, 8, 4, 8, 8,
  8, 8, 4, 8, 8,
  8, 8, 4, 8,
  
  8, 8, 4, 8, 8,
  8, 8, 4, 8, 8,
  8, 8, 4, 8, 8,
  8, 8, 8, 4, 8,
  
  8, 8, 4, 8, 8,
  4, 8, 4, 8,
  8, 8, 4, 8, 8,
  8, 8, 4, 4,
  
  4, 8,
  //Repeat of First Part
  8, 8, 4, 8, 8,
  8, 8, 4, 8, 8,
  8, 8, 4, 8,
  
  8, 8, 4, 8, 8,
  8, 8, 4, 8, 8,
  8, 8, 4, 8, 8,
  8, 8, 4, 8,
  
  8, 8, 4, 8, 8,
  8, 8, 4, 8, 8,
  8, 8, 4, 8, 8,
  8, 8, 8, 4, 8,
  
  8, 8, 4, 8, 8,
  4, 8, 4, 8,
  8, 8, 4, 8, 8,
  8, 8, 4, 4,
  //End of Repeat
  
  4, 8, 4, 4, 8, 4,
  8, 8, 8, 8, 8, 8, 8, 8, 4,
  4, 8, 4, 4, 8, 4,
  8, 8, 8, 8, 8, 2,
  
  4, 8, 4, 4, 8, 4,
  8, 8, 8, 8, 8, 8, 8, 8, 4,
  4, 8, 4, 4, 8, 4,
  8, 8, 8, 8, 8, 2
};

//CONFIGURACION DE PINES
void setup() {

  //CONFIGURAMOS LOS PINES DE SALIDA NECESARIOS PARA NUESTRA LAVADORA
  pinMode(val1, OUTPUT);   // VALVULA UNO O VAL1
  pinMode(giro, OUTPUT);
  pinMode(vel1, OUTPUT);
  pinMode(vel2, OUTPUT);
  pinMode(motor, OUTPUT);
  pinMode(bomba, OUTPUT);
  pinMode(bloqueo, OUTPUT);
  pinMode(alarma, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);   // LED INDICATIVO DE TRANCURSO DEL TIEMPO

  // CONFIGURACION INICIAL DE LOS PINES EN ALTO, YA QUE LOS RELES ENCIENDEN CUANDO PONEMOS EN BAJO EL PIN
  // CONFIGURAMOIS EN ALTO LOS PINES PARA QUE LOS RELES ESTEN APAGADOS AL INICIO DEL LOOP
  digitalWrite(val1, HIGH);
  digitalWrite(giro, HIGH);
  digitalWrite(vel1, HIGH);
  digitalWrite(vel2, HIGH);
  digitalWrite(motor, HIGH);
  digitalWrite(bomba, HIGH);
  digitalWrite(bloqueo, LOW);
  
  //INICIALIZACION DE LA PANTALLA LCD 16X2
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("REMOVEDOR DE");
  lcd.setCursor(3, 1);
  lcd.print("OLOR A PATA");
  delay(5000);

  // PRESENTAMOS NUESTRA PANTALLA INICIAL QUE MOSTRARA EL TIEMPO Y EL ESTADO SE NUESTRA LAVADORA
  PantallaLavado();
   
}

//FUNCIONES PARA OPTIMIZAR UN POCO EL CODIGO

//FUNCION PARA MOSTRAR EN PANTALLA EL TIEMPO Y EL ESTADO DE LA LAVADORA
void PantallaLavado() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(ciclo);
  lcd.setCursor(0, 1);
  lcd.print("TIEMPO");
  lcd.setCursor(8, 1);
  lcd.print(minuto);
  lcd.setCursor(11, 1);
  lcd.print(segundos);

  lcd.setCursor(14, 1);
  lcd.print(paso);
}

//FUNCION DE LLENADO
void llenado() {
  ciclo = ciclos[0];         // ACTUALIZAMOS EL ESTADO EN PANTALLA "LLENANDO"
  digitalWrite(val1, LOW);   // ENCENDEMOS LA VALVULA PARA QUE PUEDA ENTRAR AGUA
  
}
void apagarLlenado() {
  digitalWrite(val1, HIGH);   // ENCENDEMOS LA VALVULA PARA QUE PUEDA ENTRAR AGUA
}

//FUNCION DE LAVADO
void lavado() {
  ciclo = ciclos[1];         // SE ACTUALIZA EL ESTADO EN PANTALLA "LAVANDO"
  if (paso == 0) { 
    digitalWrite(vel1, HIGH);
    digitalWrite(vel2, HIGH);
    digitalWrite(motor, HIGH); 
    
  } else if (paso == 1) {               //PASO DE LAVADO 1  CICLO DE MOTOR APAGADO
     digitalWrite(motor, HIGH); 
     digitalWrite(giro, HIGH);
  }
  else if (paso == 2) {        //PASO DE LAVADO 2  CICLO DE GIRO EN EL SENTIDO CONTRARIO A LAS MANECILLAS DEL RELOJ
         digitalWrite(giro, HIGH);
     digitalWrite(motor, LOW); 
  }
  else if (paso == 3) {        //PASO DE LAVADO 3  CICLO DE MOTOR APAGADO
    digitalWrite(motor, HIGH); 
    digitalWrite(giro, LOW);
  }
  else if (paso == 4) {        //PASO DE LAVADO 4  CICLO DE GIRO EN EL SENTIDO DE LAS MANECILLAS DEL RELOJ
      digitalWrite(giro, LOW);
     digitalWrite(motor, LOW); 
  }
  if (paso > 4) {              // RESETEAR LOS PASOS PARA REPETIR EL CICLO DE LAVADO DURANTE EL TIEMPO ESTIMADO
    paso = 0;
  }
}

//FUNCION DE VACIADO DE TANQUE
void vaciado() {
  ciclo = ciclos[2];         //ACTUALIZAMOS EL ESTADO EN PANTALLA A "VACIANDO"
  digitalWrite(val1, HIGH);   // APAGAMOS FUNCIONES QUE NO NECESITAMOS
  digitalWrite(giro, HIGH);
  digitalWrite(vel1, HIGH);
  digitalWrite(vel2, HIGH);
  digitalWrite(motor, HIGH);
  digitalWrite(bomba, LOW);     // ENCENDIDO DE LA BOMBA PARA VACIAR EL TANQUE
}

void centrifugar() {       //FUNCION DE CENTRIFUGADO
  ciclo = ciclos[4];
  digitalWrite(val1, HIGH);
  digitalWrite(giro, HIGH); //DEFINIR
  digitalWrite(vel1, LOW);
  digitalWrite(bomba, LOW);
  digitalWrite(vel2, LOW);
  digitalWrite(motor, LOW);
}

void apagar() {
  ciclo = ciclos[5];
  digitalWrite(val1, HIGH);
  digitalWrite(giro, HIGH);
  digitalWrite(vel1, HIGH);
  digitalWrite(vel2, HIGH);
  digitalWrite(bomba, HIGH);
  digitalWrite(motor, HIGH);
  digitalWrite(bloqueo, HIGH);
}


void buzzerEnd(){
    int size = sizeof(durations) / sizeof(int);

  for (int note = 0; note < size; note++) {
    //to calculate the note duration, take one second divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int duration = 1000 / durations[note];
    tone(alarma, melody[note], duration);

    //to distinguish the notes, set a minimum time between them.
    //the note's duration + 30% seems to work well:
    int pauseBetweenNotes = duration * 1.30;
    delay(pauseBetweenNotes);

    //stop the tone playing:
    noTone(alarma);
  }
}

void startTone(){
    tone(alarma, NOTE_A5, 200);
   
    
     delay(1000);
       noTone(alarma);
      
}

void loop() {

  /////////////////////////////////////////// control tiempos
  if (millis() - hora >= intervalo) {
    hora = millis();
    digitalWrite(LED_BUILTIN, led);
    led = !led;
    contador = contador + 1;
    segundos = segundos + 1;

    if (segundos == 60) {
      minuto = minuto + 1;
      segundos = 0;
    }
    if (contador == 3) {
      contador = 0;
      paso = paso + 1;
    }
    PantallaLavado();
  }


    
  if(sttone == 0){
     startTone();
     sttone = 1;
  }

   ///////////////////////////////////////// LLENADO INICIAL DEL TANQUE
  if ( minuto < 3 ) {
     llenado();
     lavado();
  }
  ///////////////////////////////////////////// PRIMER CICLO DE LAVADO

  if (minuto >= 3 && minuto < 23 ) {
   digitalWrite(val1, HIGH); 
    lavado();
  }
  //////////////////////////////////////////// CILO DE VACIADO

  if (minuto >= 23 && minuto < 25 ) {
    vaciado();
  }
  ///////////////////////////////////////// LLENADO PARA EL CICLO DE ENGUAGUE

  if (minuto >= 25 && minuto < 28 ) {
    llenado();
    lavado();
  }
  ///////////////////////////////////////////// SEGUNDO CICLO DE LAVADO, ENGUAGUE DE ROPA

  if (minuto >= 28 && minuto < 38 ) {
     digitalWrite(val1, HIGH); 
    lavado();
  }
  //////////////////////////////////////////// CILO DE VACIADO

  if (minuto >= 38 && minuto < 40 ) {
    vaciado();
  }
  ////////////////////////////////////////////// CENTRIFUGADO

  if (minuto >= 40 && minuto < 47 ) {
    centrifugar();
  }
  //////////////////////////////////////////////////TERCER CILO DE LLENADO

  if (minuto >= 47 && minuto < 50 ) {
    llenado();
    lavado();
  }
  ///////////////////////////////////////////////////////////////// TERCER CICLO DE LAVADO

  if (minuto >= 50 && minuto < 58 ) {
     digitalWrite(val1, HIGH); 
    lavado();
  }
  ////////////////////////////////////////////////////////////////// CICLO DE VACIADO PARA SECADO FINAL

  if (minuto >= 58 && minuto < 60 ) {
    vaciado();
  }
  ////////////////////////////////////////////////////////// SEGUNDO CICLO DE CENTRIFUGADO

  if (minuto >= 60 && minuto < 70) {
    centrifugar();
  }
  ////////////////////////////////////////////////////////// CICLO DE ESPERA AQUI SE APAGN TODAS LAS FUNCIONES
  if (minuto >= 70 && minuto < 73) {
    apagar();
    buzzerEnd();
  }

}
