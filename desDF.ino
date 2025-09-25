#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include "Servo.h"
#include <EEPROM.h>

// ---------------- Hardware pins ----------------
const int prevButton  = 2;
const int nextButton  = 3;
const int trueButton  = 4;
const int falseButton = 5;

// 7-segment pins
const int segA = 6;
const int segB = 7;
const int segC = 8;
const int segD = 9;
const int segE = 12;
const int segF = 13;
const int segG = A0;

// Servo pin
const int servoPin = A1;

// ---------------- Objects ----------------
SoftwareSerial mySoftwareSerial(10, 11);  // RX, TX for DFPlayer
DFRobotDFPlayerMini myDFPlayer;
Servo dispenserServo;

// EEPROM address for last servo position
const int servoEEPROMAddr = 0;

// ---------------- Playback / questions ----------------
int currentQuestion = 1;
const int totalQuestions = 10;
bool questionCompleted[totalQuestions + 1]; 

// ---------------- Secret code ----------------
const int secretButton = falseButton;
const int secretCountRequired = 4;
int secretCount = 0;
unsigned long lastSecretPressTime = 0;
const unsigned long maxGap = 700; // ms

bool secretPlaying = false;
unsigned long secretStart = 0;
int secretTrack = 1;
const int totalSecretTracks = 5;
bool secretPaused = false;

// ---------------- Button polling / debounce ----------------
const int NUM_BUTTONS = 4;
const int BUTTONS[NUM_BUTTONS] = { prevButton, nextButton, trueButton, falseButton };
int prevState[NUM_BUTTONS];
unsigned long lastDebounceTime[NUM_BUTTONS];
const unsigned long debounceMs = 50;

// ---------------- Countdown ----------------
int countdown = 5;          
unsigned long lastCountMillis = 0;
const unsigned long countInterval = 1000;
bool waitingForAnswer = false;
bool answerInProgress = false;

// ---------------- States ----------------
enum State { IDLE, QUESTION, DISPENSER };
State currentState = IDLE;

// ---------------- Function prototypes ----------------
void pollButtons(bool secretOnly);
void onButtonPress(int pin);
void registerSecretPress(int pin);
void handleSecretButtons(int pin);
bool waitWithSecretCheck(unsigned long durationMs);
void playQuestion(int qNum);
void playAnswer(int qNum, bool isCorrect);
void moveToNextQuestion();
void displayNumber(int num);
void displayTF(char letter);
bool anyButtonPressed();
void initServo();
void moveServoSmooth(int startAngle, int endAngle, int stepDelay = 10);
void dispenseGum();

// ---------------- Setup ----------------
void setup() {
  for (int i = 0; i < NUM_BUTTONS; ++i) {
    pinMode(BUTTONS[i], INPUT_PULLUP);
    prevState[i] = digitalRead(BUTTONS[i]);
    lastDebounceTime[i] = 0;
  }

  pinMode(segA, OUTPUT); pinMode(segB, OUTPUT); pinMode(segC, OUTPUT);
  pinMode(segD, OUTPUT); pinMode(segE, OUTPUT); pinMode(segF, OUTPUT);
  pinMode(segG, OUTPUT);

  mySoftwareSerial.begin(9600);
  Serial.begin(115200);

  Serial.println(F("Initializing DFPlayer Mini..."));
  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println(F("Not initialized! Check DFPlayer connections and SD card."));
    while (true);
  }
  Serial.println(F("DFPlayer Mini ready."));
  myDFPlayer.volume(28);

  displayNumber(0);

  for (int i = 1; i <= totalQuestions; i++) {
    questionCompleted[i] = false;
  }

  initServo();
}

// ---------------- Servo initialization ----------------
void initServo() {
  dispenserServo.attach(servoPin);
  int lastPos = EEPROM.read(servoEEPROMAddr);
  if (lastPos < 0 || lastPos > 180) lastPos = 0;
  dispenserServo.write(lastPos);
}

// ---------------- Smooth servo movement ----------------
void moveServoSmooth(int startAngle, int endAngle, int stepDelay) {
  if (startAngle < endAngle) {
    for (int pos = startAngle; pos <= endAngle; pos++) {
      dispenserServo.write(pos);
      delay(stepDelay);
    }
  } else {
    for (int pos = startAngle; pos >= endAngle; pos--) {
      dispenserServo.write(pos);
      delay(stepDelay);
    }
  }
}

// ---------------- Dispense gum ----------------
void dispenseGum() {
  int currentPos = dispenserServo.read();
  int targetPos = (currentPos == 0) ? 180 : 0; 
  moveServoSmooth(currentPos, targetPos, 10);
  delay(2000);
  EEPROM.update(servoEEPROMAddr, targetPos); 
}

// ---------------- Main loop ----------------
void loop() {
  pollButtons(false);

  switch(currentState) {
    case IDLE:
      displayNumber(0);
      if (anyButtonPressed()) {
        currentState = QUESTION;
        playQuestion(currentQuestion);
      }
      break;

    case QUESTION:
      if (!secretPlaying && waitingForAnswer && !answerInProgress) {
        if (millis() - lastCountMillis >= countInterval) {
          lastCountMillis = millis();
          displayNumber(countdown);
          countdown--;
          if (countdown < 0) {
            waitingForAnswer = false;
            myDFPlayer.playFolder(3, 5);
            waitWithSecretCheck(2500);    
            displayNumber(0);
            currentState = IDLE;
          }
        }
      }
      break;

    case DISPENSER:
      Serial.println(F("DISPENSER STATE: Activating servo"));
      dispenseGum();
      displayNumber(0);
      moveToNextQuestion(); 
      currentState = IDLE;
      break;
  }
}

// ---------------- Move to next question ----------------
void moveToNextQuestion() {
  int next = currentQuestion + 1;
  if (next > totalQuestions) next = 1;

  bool allCompleted = true;
  for (int i = 1; i <= totalQuestions; i++) {
    if (!questionCompleted[i]) {
      allCompleted = false;
      break;
    }
  }

  if (allCompleted) {
    for (int i = 1; i <= totalQuestions; i++) {
      questionCompleted[i] = false;
    }
  }

  while (questionCompleted[next]) {
    next++;
    if (next > totalQuestions) next = 1;
    if (next == currentQuestion) break;
  }
  currentQuestion = next;
}

// ---------------- Poll buttons ----------------
void pollButtons(bool secretOnly) {
  for (int i = 0; i < NUM_BUTTONS; ++i) {
    int pin = BUTTONS[i];
    int reading = digitalRead(pin);

    if (reading != prevState[i]) {
      if (millis() - lastDebounceTime[i] > debounceMs) {
        if (reading == LOW) {
          if (secretOnly) {
            registerSecretPress(pin);
          } else {
            onButtonPress(pin);
          }
        }
        prevState[i] = reading;
        lastDebounceTime[i] = millis();
      }
    }
  }
}

// ---------------- When a button is pressed ----------------
void onButtonPress(int pin) {
  registerSecretPress(pin);

  if (secretPlaying) {
    handleSecretButtons(pin);
    return;
  }

  if (currentState == IDLE) {
    currentState = QUESTION;
    playQuestion(currentQuestion);
    return;
  }

  if (waitingForAnswer) waitingForAnswer = false;

  if (pin == nextButton) {
    currentQuestion++;
    if (currentQuestion > totalQuestions) currentQuestion = 1;
    playQuestion(currentQuestion);
  }
  else if (pin == prevButton) {
    currentQuestion--;
    if (currentQuestion < 1) currentQuestion = totalQuestions;
    playQuestion(currentQuestion);
  }
  else if (pin == trueButton) {
    displayTF('T');
    playAnswer(currentQuestion, false);
    displayNumber(0);
    currentState = IDLE;
  }
  else if (pin == falseButton) {
    displayTF('F');
    playAnswer(currentQuestion, true);
    questionCompleted[currentQuestion] = true;
    currentState = DISPENSER; 
  }
}

// ---------------- Secret press registration ----------------
void registerSecretPress(int pin) {
  unsigned long now = millis();

  if (pin != secretButton) {
    secretCount = 0;
    lastSecretPressTime = 0;
    return;
  }

  if (lastSecretPressTime == 0 || (now - lastSecretPressTime) > maxGap) secretCount = 0;
  lastSecretPressTime = now;
  secretCount++;

  if (secretCount >= secretCountRequired) {
    Serial.println(F("Secret code matched! Entering secret mode..."));
    myDFPlayer.stop();
    delay(50);
    secretTrack = 1;
    myDFPlayer.playFolder(4, secretTrack);
    secretPlaying = true;
    secretPaused = false;
    secretCount = 0;
  }
}

// ---------------- Handle buttons during secret mode ----------------
void handleSecretButtons(int pin) {
  if (pin == prevButton) {
    if (secretPaused) {
      myDFPlayer.start();
      secretPaused = false;
    } else {
      myDFPlayer.pause();
      secretPaused = true;
    }
  }
  else if (pin == nextButton) {
    secretTrack++;
    if (secretTrack > totalSecretTracks) secretTrack = 1;
    myDFPlayer.playFolder(4, secretTrack);
    secretPaused = false;
  }
  else if (pin == trueButton) {
    myDFPlayer.stop();
    secretPlaying = false;
    secretPaused = false;
    currentState = QUESTION;
    playQuestion(currentQuestion);
  }
}

// ---------------- Wait helper ----------------
bool waitWithSecretCheck(unsigned long durationMs) {
  unsigned long start = millis();
  while (millis() - start < durationMs) {
    pollButtons(true);
    if (secretPlaying) return false;
    delay(20);
  }
  return true;
}

// ---------------- Play question/answer ----------------
void playQuestion(int qNum) {
  Serial.print(F("Playing Question "));
  Serial.println(qNum);

  myDFPlayer.playFolder(3, 1);
  waitWithSecretCheck(2000);

  myDFPlayer.playFolder(1, qNum);

  countdown = 5;
  lastCountMillis = millis();
  waitingForAnswer = true;
  displayNumber(countdown);
}

void playAnswer(int qNum, bool isCorrect) {
  answerInProgress = true;

  if (isCorrect) {
    myDFPlayer.playFolder(3, 2);
    waitWithSecretCheck(2500);
  } else {
    myDFPlayer.playFolder(3, 3);
    waitWithSecretCheck(4500);
  }

  myDFPlayer.playFolder(2, qNum);
  waitWithSecretCheck(9500);

  myDFPlayer.playFolder(3, 4);
  waitWithSecretCheck(2500);

  answerInProgress = false;
}

// ---------------- 7-segment helpers ----------------
void displayNumber(int num) {
  bool a,b,c,d,e,f,g;
  a=b=c=d=e=f=g=false;

  switch(num) {
    case 5: a=true;b=false;c=true;d=true;e=false;f=true;g=true; break;
    case 4: a=false;b=true;c=true;d=false;e=false;f=true;g=true; break;
    case 3: a=true;b=true;c=true;d=true;e=false;f=false;g=true; break;
    case 2: a=true;b=true;c=false;d=true;e=true;f=false;g=true; break;
    case 1: a=false;b=true;c=true;d=false;e=false;f=false;g=false; break;
    case 0: a=true;b=true;c=true;d=true;e=true;f=true;g=false; break;
  }

  digitalWrite(segA,a); digitalWrite(segB,b); digitalWrite(segC,c);
  digitalWrite(segD,d); digitalWrite(segE,e); digitalWrite(segF,f); digitalWrite(segG,g);
}

void displayTF(char letter) {
  bool a=false,b=false,c=false,d=false,e=false,f=false,g=false;

  switch(letter) {
    case 'T': f=true;e=true;g=true; break;
    case 'F': a=true;f=true;e=true;g=true; break;
  }

  digitalWrite(segA,a); digitalWrite(segB,b); digitalWrite(segC,c);
  digitalWrite(segD,d); digitalWrite(segE,e); digitalWrite(segF,f); digitalWrite(segG,g);
}

// ---------------- Any button pressed helper ----------------
bool anyButtonPressed() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (digitalRead(BUTTONS[i]) == LOW) return true;
  }
  return false;
}




