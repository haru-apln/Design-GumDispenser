#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"

// DFPlayer Serial
SoftwareSerial mySoftwareSerial(10, 11);  // RX, TX
DFRobotDFPlayerMini myDFPlayer;

// Button pins
const int prevButton  = 2;
const int nextButton  = 3;
const int trueButton  = 4;
const int falseButton = 5;

// Question counter
int currentQuestion = 1;   // Start from Question 1
int totalQuestions  = 10;  // Total number of questions

void setup() {
  // Button setup
  pinMode(prevButton, INPUT_PULLUP);
  pinMode(nextButton, INPUT_PULLUP);
  pinMode(trueButton, INPUT_PULLUP);
  pinMode(falseButton, INPUT_PULLUP);

  // DFPlayer serial
  mySoftwareSerial.begin(9600);
  Serial.begin(115200);

  Serial.println(F("Initializing DFPlayer Mini..."));
  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println(F("Not initialized! Check connections and SD card."));
    while (true);
  }

  Serial.println(F("DFPlayer Mini ready."));
  myDFPlayer.volume(28);  // Set volume (0–30)

  playQuestion(currentQuestion); // Start with Q1
}

void loop() {
  // Next button → Next question (with loop around)
  if (digitalRead(nextButton) == LOW) {
    currentQuestion++;
    if (currentQuestion > totalQuestions) currentQuestion = 1; // loop back to 1
    playQuestion(currentQuestion);
    delay(300); // debounce
  }

  // Prev button → Previous question (with loop around)
  if (digitalRead(prevButton) == LOW) {
    currentQuestion--;
    if (currentQuestion < 1) currentQuestion = totalQuestions; // loop back to last
    playQuestion(currentQuestion);
    delay(300);
  }

  // True button → always Incorrect first, then play answer, then "Thank you"
  if (digitalRead(trueButton) == LOW) {
    playFalseAnswer(currentQuestion, false); // false because True button is always incorrect
    delay(300);
  }

  // False button → always Correct first, then play answer, then "Thank you"
  if (digitalRead(falseButton) == LOW) {
    playFalseAnswer(currentQuestion, true); // true because False button is always correct
    delay(300);
  }
}

// ------------------ FUNCTIONS ------------------ //

void playQuestion(int qNum) {
  Serial.print("Playing Question "); Serial.println(qNum);
  myDFPlayer.playFolder(3, 1);   // Folder 03 → file 001.mp3 ("Here’s your question")
  delay(2000); 
  myDFPlayer.playFolder(1, qNum); // Folder 01 → Question files
}

void playFalseAnswer(int qNum, bool isCorrect) {
  Serial.print("Playing Answer for Q"); Serial.println(qNum);

  if (isCorrect) {
    myDFPlayer.playFolder(3, 2);  // Folder 03 → "That's right"
     delay(2500); 
  } else {
    myDFPlayer.playFolder(3, 3);  // Folder 03 → "Incorrect"
     delay(4500); 
  }

  myDFPlayer.playFolder(2, qNum); // Folder 02 → Answer file
  delay(9500);  // Adjust to match answer length
}


