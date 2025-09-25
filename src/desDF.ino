#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"

// ---------------- Hardware pins ----------------
const int prevButton  = 2;
const int nextButton  = 3;
const int trueButton  = 4;
const int falseButton = 5;

SoftwareSerial mySoftwareSerial(10, 11);  // RX, TX for DFPlayer
DFRobotDFPlayerMini myDFPlayer;

// ---------------- Playback / questions ----------------
int currentQuestion = 1;
const int totalQuestions = 10;

// ---------------- Secret code settings ----------------
// Secret = 4 consecutive presses of the FALSE button
const int secretButton = falseButton;
const int secretCountRequired = 4;
int secretCount = 0;
unsigned long lastSecretPressTime = 0;
const unsigned long maxGap = 700; // ms allowed between the consecutive presses

// secret playback tracking
bool secretPlaying = false;
unsigned long secretStart = 0;
int secretTrack = 1;                 // track index within folder 04
const int totalSecretTracks = 5;     // adjust this to number of files in folder 04

// pause state inside secret
bool secretPaused = false;

// ---------------- Button polling / debounce ----------------
const int NUM_BUTTONS = 4;
const int BUTTONS[NUM_BUTTONS] = { prevButton, nextButton, trueButton, falseButton };
int prevState[NUM_BUTTONS];
unsigned long lastDebounceTime[NUM_BUTTONS];
const unsigned long debounceMs = 50; // debounce window ms

// ---------------- Setup ----------------
void setup() {
  for (int i = 0; i < NUM_BUTTONS; ++i) {
    pinMode(BUTTONS[i], INPUT_PULLUP);
    prevState[i] = digitalRead(BUTTONS[i]);
    lastDebounceTime[i] = 0;
  }

  mySoftwareSerial.begin(9600);
  Serial.begin(115200);

  Serial.println(F("Initializing DFPlayer Mini..."));
  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println(F("Not initialized! Check DFPlayer connections and SD card."));
    while (true);
  }
  Serial.println(F("DFPlayer Mini ready."));
  myDFPlayer.volume(28);

  // start with first question
  playQuestion(currentQuestion);
}

// ---------------- Main loop ----------------
void loop() {
  pollButtons(false);
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
  // always check secret activation first
  registerSecretPress(pin);

  if (secretPlaying) {
    handleSecretButtons(pin);
    return;
  }

  // Normal behavior
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
    playAnswer(currentQuestion, false);  // incorrect
  }
  else if (pin == falseButton) {
    playAnswer(currentQuestion, true);   // correct
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

  if (lastSecretPressTime == 0 || (now - lastSecretPressTime) > maxGap) {
    secretCount = 0;
  }
  lastSecretPressTime = now;
  secretCount++;

  Serial.print(F("Secret false-press count: "));
  Serial.println(secretCount);

  if (secretCount >= secretCountRequired) {
    Serial.println(F("Secret code matched! Entering secret mode..."));
    myDFPlayer.stop();
    delay(500);
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
    // Pause/resume
    if (secretPaused) {
      Serial.println(F("Secret resume"));
      myDFPlayer.start();
      secretPaused = false;
    } else {
      Serial.println(F("Secret pause"));
      myDFPlayer.pause();
      secretPaused = true;
    }
  }
  else if (pin == nextButton) {
    // Next track with wrap
    secretTrack++;
    if (secretTrack > totalSecretTracks) secretTrack = 1;
    Serial.print(F("Secret next track: "));
    Serial.println(secretTrack);
    myDFPlayer.playFolder(4, secretTrack);
    secretPaused = false;
  }
  else if (pin == trueButton) {
    // Exit secret mode
    Serial.println(F("Exiting secret mode..."));
    myDFPlayer.stop();
    secretPlaying = false;
    secretPaused = false;
    playQuestion(currentQuestion); // resume normal mode
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
  if (!waitWithSecretCheck(2000)) return;

  myDFPlayer.playFolder(1, qNum);
}

void playAnswer(int qNum, bool isCorrect) {
  Serial.print(F("Playing Answer for Q"));
  Serial.println(qNum);

  if (isCorrect) {
    myDFPlayer.playFolder(3, 2);
    if (!waitWithSecretCheck(2500)) return;
  } else {
    myDFPlayer.playFolder(3, 3);
    if (!waitWithSecretCheck(4500)) return;
  }

  myDFPlayer.playFolder(2, qNum);
  if (!waitWithSecretCheck(9500)) return;
}


