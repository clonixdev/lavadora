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
int sttone = 0; //TONO INICIAL

int presostato = 3;
int val1 =  8;    // VALVULA DE ENTRADA DE AGUA
int giro = 5;    // GIRO DEL MOTOR
int vel1 = 6;    // VELOCIDAD DE MOTOR
int vel2 = 7;    // VELOCIDAD DE MOTOR
int motor = 4;    // ENCENDER MOTOR
int bomba = 9;    // BOMBA DE AGUA
int bloqueo = 10;  // BLOQUEO DE PUERTA
int alarma = 2;   // ALARMA BUZZER PARA FIN DE LAVADO 
int acelerado = 0;



int tamborVacio = 0;
int tiempoTotal = 0;
int tiempoStart = 0;
int tiempoEnd = 0;
int faseActual = 0;
int llenadoError = 0;
struct FaseLavado {
  String funcion;
  int tiempo;
};

//FASES DE LAVADO, FUNCION - TIEMPO en minutos
FaseLavado fases[] = {
  {"llenadosolo",5},
  {"llenado",5},
  {"lavado", 15},
  {"vaciado", 1},
  {"llenadosolo",5},
  {"llenado", 5},
  {"lavado", 8},
  {"vaciado", 1},
  {"centrifugar", 5},
  {"llenadosolo",5},
  {"llenado", 5},
  {"lavado", 8},
  {"vaciado", 1},
  {"centrifugar", 10},
  
};

// si la direccion 0x27 no funciona, prueba con 0x28 ó con 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2);

//CONFIGURACION DE PINES
void setup() {

  Serial.begin(9600);
  //CONFIGURAMOS LOS PINES DE SALIDA NECESARIOS PARA NUESTRA LAVADORA
  pinMode(presostato, INPUT);
  pinMode(val1, OUTPUT); 
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
  digitalWrite(bloqueo, LOW); //BLOQUEO DE PUERTA
  
  //INICIALIZACION DE LA PANTALLA LCD 16X2
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("LAVADORA");
  lcd.setCursor(3, 1);
  lcd.print("CLONIX");
  delay(5000);

  // PRESENTAMOS NUESTRA PANTALLA INICIAL QUE MOSTRARA EL TIEMPO Y EL ESTADO SE NUESTRA LAVADORA
  PantallaLavado();

  
  for (int i = 0; i < sizeof(fases) / sizeof(FaseLavado); i++) {
    tiempoTotal += fases[i].tiempo;
  }
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
  //lcd.setCursor(14, 1);
  //lcd.print(paso);
}

//FUNCION DE LLENADO
void llenado() {
  ciclo = ciclos[0];         // ACTUALIZAMOS EL ESTADO EN PANTALLA "LLENANDO"

  if(tamborVacio == 1){
      digitalWrite(bomba, HIGH); // APAGAMOS LA BOMBA DE DESAGOTE
  digitalWrite(val1, LOW);   // ENCENDEMOS LA VALVULA PARA QUE PUEDA ENTRAR AGUA
  }else{

    digitalWrite(val1, HIGH);   // ENCENDEMOS LA VALVULA PARA QUE PUEDA ENTRAR AGUA
  digitalWrite(bomba, HIGH);
  }

  
}
void apagarLlenado() {
  digitalWrite(val1, HIGH);   // ENCENDEMOS LA VALVULA PARA QUE PUEDA ENTRAR AGUA
  digitalWrite(bomba, HIGH);
}

//FUNCION DE LAVADO
void lavado() {
  ciclo = ciclos[1];         // SE ACTUALIZA EL ESTADO EN PANTALLA "LAVANDO"
  if (paso == 0) { 
    digitalWrite(vel1, HIGH);
    digitalWrite(vel2, HIGH);
    digitalWrite(motor, HIGH); 
    digitalWrite(bomba, HIGH);
    
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

  digitalWrite(val1, HIGH);
  digitalWrite(giro, HIGH); //AH
    ciclo = ciclos[4];
    
  if(acelerado == 0){
    digitalWrite(vel1, HIGH); 
    digitalWrite(vel2, HIGH); 
    delay(1000);
    digitalWrite(motor, LOW);
    delay(3000);   
    digitalWrite(motor, HIGH);
    delay(10);
    digitalWrite(vel1, LOW); 
    digitalWrite(vel2, LOW); 
    digitalWrite(giro, LOW); //AH
    delay(300);
    digitalWrite(motor, LOW);
    acelerado = 1;
  }
  
  ciclo = ciclos[4];
//SENTIDO DE GIRO EN CENTRIFUGADO , EL CENTRIFUGADO FUNCIONA BIEN CON 1 SENTIDO NO FUNCIONA DE LA MISMA MANERA EN LOS DOS
  // ACTIVAMOS LOS 2 RELES DE CAMBIOI DE VELOCDIAD
  digitalWrite(vel1, LOW); 
  digitalWrite(vel2, LOW); 
  digitalWrite(bomba, LOW); //ACTIVAMOS LA BOMBA DE DESAGOTE
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

//PLAY PIRATAS DEL CARIBE
void buzzerEnd(){
    tone(alarma, NOTE_A5, 100);
    delay(600);
    noTone(alarma);    
        tone(alarma, NOTE_A5, 100);
    delay(600);
    noTone(alarma);    
        tone(alarma, NOTE_A5, 100);
    delay(600);
    noTone(alarma);    
        tone(alarma, NOTE_A5, 100);
    delay(600);
    noTone(alarma);    
        tone(alarma, NOTE_A5, 100);
    delay(5000);
     tone(alarma, NOTE_A5, 100);
    delay(600);
    noTone(alarma);    
        tone(alarma, NOTE_A5, 100);
    delay(600);
    noTone(alarma);    
        tone(alarma, NOTE_A5, 100);
    delay(600);
    noTone(alarma);    
        tone(alarma, NOTE_A5, 100);
    delay(600);
    noTone(alarma);    
        tone(alarma, NOTE_A5, 100);
    delay(5000);
    noTone(alarma);    
}

//TONO INICIO
void startTone(){
    tone(alarma, NOTE_A5, 200);
    delay(1000);
    noTone(alarma);    
}

void errorTone(){
    tone(alarma, NOTE_C5, 500);
    delay(1000);
    tone(alarma, NOTE_C5, 500);
    delay(1000);
    noTone(alarma);    
}

void loop() {

  tamborVacio = digitalRead(presostato);
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
      Serial.println("START");
  }

  if(llenadoError){
     Serial.println("ERROR DE LLENADO");
     apagar();
    return;
  }
 
  tiempoEnd = tiempoStart + fases[faseActual].tiempo;

    if (minuto >= tiempoStart && minuto < tiempoEnd) {
      // Ejecutar la función correspondiente
      if (fases[faseActual].funcion == "llenado") {
        lavado();
        llenado();
      } else if (fases[faseActual].funcion == "llenadosolo") {
         llenado();
         if(tamborVacio == 0){
            minuto = minuto + 1;
            segundos = 0;
            Serial.println("AVANCE TAMBOR LLENO");
         }
      } else if (fases[faseActual].funcion == "lavado") {

          apagarLlenado();
          lavado();
        /*if(tamborVacio == 1){
           apagarLlenado();
          errorTone(); 
           apagar();
           llenadoError = 1;
        }else {

        
        }*/
        
      } else if (fases[faseActual].funcion == "vaciado") {
        vaciado();
      } else if (fases[faseActual].funcion == "centrifugar") {
        centrifugar();
      }
 }

  if(segundos % 10 == 0){
      Serial.println("PRESOSTATO");
       Serial.println(tamborVacio);
    Serial.println("FASE");
       Serial.println(fases[faseActual].funcion);
       Serial.println("MINUTO");
        Serial.println(minuto);
  }
  
   if (minuto >= tiempoEnd) {
         faseActual = faseActual + 1;
         acelerado = 0;
       tiempoStart = tiempoEnd;
              Serial.println("================================================ ");

       Serial.println("PRESOSTATO ");
       Serial.println(tamborVacio);
       Serial.println("FASE");
       Serial.println(fases[faseActual].funcion);
       Serial.println("MINUTO");
        Serial.println(minuto);
  }
    if (minuto >= tiempoTotal + 2) {  // Espera 3 minutos adicionales antes de apagar todo y activar la alarma
    apagar();
     Serial.println("FIN");
    buzzerEnd();
  }
}
