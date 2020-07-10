/*    HOW TO USE:
  Shutter works on pin 2 and 5V.

  COMMANDS (without spaces):
    s - starts / stops the shutter
    o number - sets number as a time in seconds for how long the shutter will be open
    c number - sets number as a time in seconds for how long the shutter will be closed
    p number - opens shutter for number seconds
    n - opens shutter
    m - closes shutter

*/
#define INIT_PIN 2
#define BAUD_RATE 9600
#define CLOSED_ANGLE 180
#define OPENED_ANGLE 160
#define REFRESH_INTERVAL 100
#define VALUE_PRECISION 1000
#define DEBOUNCE_DELAY 50
#define UPDATE_DELAY 100
#define DECIMAL_POINT_POSITION 1

#include <Servo.h>
#include "SevSeg.h" // You need to install SevSeg library

Servo board;
SevSeg sevseg;

bool shutterState = 0;
bool onOffCycle = 0;
bool polymerization = 0;

volatile long timeSinceLastCommand = 0;
int openTime = 40;
int closedTime = 60;
int polymerizationTime = 30;
int polymerizationCountdown = 10;

void setup() {
  board.attach(INIT_PIN);
  Serial.print("cmds: s - starts/stops, \noX - sets the shutter to be open for X seconds, \ncX - sets the shutter to be closed for X seconds\npX - polymerizes for X seconds,\nn - opens shutter,\nmcloses shutter");
  Serial.begin(BAUD_RATE);

  byte numDigits = 3;
  byte digitPins[] = {5, 8, 9};
  //byte segmentPins[] = {6, 7, 10, A0, A1, A2, A3, A4};
  byte segmentPins[] = {6, 10, A1, A3, A4, 7, A0, A2};
  
  bool resistorsOnSegments = true; // 'false' means resistors are on digit pins
  byte hardwareConfig = COMMON_ANODE; // See README.md for options
  bool updateWithDelays = true; // Default 'false' is Recommended
  bool leadingZeros = false; // Use 'true' if you'd like to keep the leading zeros
  bool disableDecPoint = false; // Use 'true' if your decimal point doesn't exist or isn't connected. Then, you only need to specify 7 segmentPins[]

  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments,
  updateWithDelays, leadingZeros, disableDecPoint);

  pinMode(11, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP);
  pinMode(A5, INPUT_PULLUP);

  cli();
  TCCR3A = 0;
  TCCR3B = 0;
  TCNT3  = 0;
  OCR3A = 6249;
  TCCR3B |= (1 << WGM32);
  TCCR3B |= (1 << CS32);
  TIMSK3 |= (1 << OCIE3A);
  sei();
}


void loop() {
  if (onOffCycle) {
    changeShutterState();
  }

  if (polymerization) {
    polymerize();
    sevseg.setNumber(polymerizationCountdown, 1);
  }
  else if (polymerizationTime){
    sevseg.setNumber(polymerizationTime, 1);
  }

  changePolymerizationTime(11, +1);
  changePolymerizationTime(12, -1);
  changeState(A5);

  if (Serial.available() > 0) {
    char cmd = '0';
    int cmdNumber = 0;
    readCmd(cmd, cmdNumber);
    execCmd(cmd, cmdNumber);
    timeSinceLastCommand = 0;
  }
  sevseg.refreshDisplay();
}


void changeShutterState() {
  if (shutterState && timeSinceLastCommand == openTime) {
    shutterState = 0;
    board.write(CLOSED_ANGLE);
    timeSinceLastCommand = 0;
  }
  else if (!shutterState && timeSinceLastCommand == closedTime) {
    shutterState = 1;
    board.write(OPENED_ANGLE);
    timeSinceLastCommand = 0;
  }
}

void polymerize() {
  if (timeSinceLastCommand == polymerizationTime) {
    shutterState = 0;
    board.write(CLOSED_ANGLE);
    timeSinceLastCommand = 0;
    polymerization = 0;
    Serial.print("Polimerization finished.\n");
  }
}

void readCmd(char& cmd, int& cmdNumber) {
  cmd = Serial.read();
  cmdNumber = Serial.parseInt();
}

void execCmd(const char& cmd, const int& cmdNumber) {
  switch (cmd) {
    case 'o':
      openTime = cmdNumber;
      Serial.print("Shutter open time set to ");
      Serial.print(openTime * REFRESH_INTERVAL);
      Serial.print(" milliseconds.\n");
      break;
    case 'c':
      closedTime = cmdNumber;
      Serial.print("Shutter closed time set to ");
      Serial.print(closedTime * REFRESH_INTERVAL);
      Serial.print(" milliseconds.\n");
      break;
    case 'p':
      polymerizationTime = cmdNumber;
      polymerizationCountdown = cmdNumber;
      Serial.print("Polimerization time set to ");
      Serial.print(polymerizationTime * REFRESH_INTERVAL);
      Serial.print(" milliseconds.\n");
      onOffCycle = 0;
      board.write(OPENED_ANGLE);
      polymerization = 1;
      break;
    case 's':
      onOffCycle = !onOffCycle;
      if (onOffCycle) {
        Serial.print("Shutter started.\n");
      } else {
        Serial.print("Shutter stopped.\n");
      }
      board.write(CLOSED_ANGLE);
      break;
    case 'n':
      onOffCycle = 0;
      board.write(OPENED_ANGLE);
      Serial.print("Shutter opened.\n");
      break;
    case 'm':
      onOffCycle = 0;
      board.write(CLOSED_ANGLE);
      Serial.print("Shutter closed.\n");
      break;
  }
}

void changePolymerizationTime(byte pin, int value) {
  if (!digitalRead(pin)) {
    delay(DEBOUNCE_DELAY);
    while (!digitalRead(pin)) {
      polymerizationTime = (polymerizationTime + VALUE_PRECISION + value) % VALUE_PRECISION;
      sevseg.setNumber(polymerizationTime, DECIMAL_POINT_POSITION); 
      sevseg.refreshDisplay();
      delay(UPDATE_DELAY);
    }
  }
}

void changeState(byte pin) {
   if (!digitalRead(pin) && !polymerization) {
    delay(250);
    if (!digitalRead(pin)) {
      if (polymerizationTime) {
        Serial.print("Polimerization started manually.");
        onOffCycle = 0;
        board.write(OPENED_ANGLE);
        polymerization = 1;
        polymerizationCountdown = polymerizationTime;
        timeSinceLastCommand = 0;
      }
      else {
        shutterState = !shutterState;
        if (shutterState) {
          board.write(CLOSED_ANGLE);
          sevseg.setChars("cls");
        }
        else {
          board.write(OPENED_ANGLE);
          sevseg.setChars("OPN");
        }
      }
    }
  }
}

ISR(TIMER3_COMPA_vect) {
  timeSinceLastCommand++;
  if (polymerization) {
    polymerizationCountdown--;
  }
}
