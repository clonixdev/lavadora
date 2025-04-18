void buzzerPWM(int pin, int freq, int duration) {
  int period = 1000000 / freq; // periodo en microsegundos
  int halfPeriod = period / 2;
  long cycles = ((long)duration * 1000L) / period;

  for (long i = 0; i < cycles; i++) {
    digitalWrite(pin, HIGH);
    delayMicroseconds(halfPeriod);
    digitalWrite(pin, LOW);
    delayMicroseconds(halfPeriod);
  }
}

void nobuzzerPWM(int pin) {
  digitalWrite(pin, LOW); // Asegura que el pin esté en bajo
}
void buzzerEnd()
{
  buzzerPWM(alarma, 880, 100);
  delay(600);
  nobuzzerPWM(alarma);
  buzzerPWM(alarma, 880, 100);
  delay(600);
  nobuzzerPWM(alarma);
  buzzerPWM(alarma, 880, 100);
  delay(600);
  nobuzzerPWM(alarma);
  buzzerPWM(alarma, 880, 100);
  delay(600);
  nobuzzerPWM(alarma);
  buzzerPWM(alarma, 880, 100);
  nobuzzerPWM(alarma);
}

// TONO INICIO
void startbuzzerPWM()
{
  buzzerPWM(alarma, 880, 200);
  delay(500);
  buzzerPWM(alarma, 1000, 200);
  delay(500);
  nobuzzerPWM(alarma);
}

void powerOnbuzzerPWM()
{
  buzzerPWM(alarma, 880, 200);
  /*delay(1000);
  nobuzzerPWM(alarma);*/
}

void errorbuzzerPWM()
{
  buzzerPWM(alarma, 440, 500);
  delay(1000);
  buzzerPWM(alarma, 440, 500);
  delay(1000);
  nobuzzerPWM(alarma);
}