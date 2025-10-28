/*
 * EXAM POLLING SYSTEM - SLAVE DEVICE (ANSWERING DEVICE)
 * 
 * Hardware Requirements:
 * - Arduino Uno/Nano
 * - 433MHz RF Transmitter (TX pin to Arduino pin 12)
 * - 433MHz RF Receiver (DATA pin to Arduino pin 11)
 * - 2x2 Membrane Keypad connected to pins 2, 3, 4, 5
 * - Orange LED + 220Ω resistor (pin 8)
 * - Green LED + 220Ω resistor (pin 9)
 * 
 * Library Required: RadioHead (Install via Arduino Library Manager)
 * 
 * IMPORTANT: Set unique DEVICE_ID for each slave device!
 */

#include <RH_ASK.h>
#include <SPI.h>
#include <Keypad.h>

// ============================================
// CONFIGURATION - CHANGE FOR EACH DEVICE!
// ============================================
#define DEVICE_ID 1  // SET UNIQUE ID: 1, 2, 3, 4, etc.
// ============================================

// RF Communication setup
RH_ASK rf_driver(2000, 11, 12, 0); // Speed, RX pin, TX pin, PTT pin

// LED pins
#define LED_ORANGE 8  // Waiting for acknowledgment
#define LED_GREEN 9   // Answer submitted successfully

// Keypad setup (2x2 configuration)
const byte ROWS = 2;
const byte COLS = 2;
char keys[ROWS][COLS] = {
  {'A', 'B'},
  {'C', 'D'}
};
byte rowPins[ROWS] = {2, 3};    // Connect to row pins of keypad
byte colPins[COLS] = {4, 5};    // Connect to col pins of keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Protocol message types
#define MSG_POLL 'P'           // Master polls for ready devices
#define MSG_REQUEST 'R'        // Slave requests to submit answer
#define MSG_GRANT 'G'          // Master grants permission to send
#define MSG_ANSWER 'A'         // Slave sends answer
#define MSG_ACK 'K'            // Master acknowledges receipt

// State management
enum State {
  IDLE,                    // Waiting for poll
  WAITING_FOR_INPUT,      // Waiting for candidate to press key
  INPUT_RECEIVED,         // Key pressed, waiting to request
  REQUESTING,             // Requesting permission from master
  WAITING_GRANT,          // Waiting for grant from master
  SENDING_ANSWER,         // Sending answer to master
  WAITING_ACK,            // Waiting for acknowledgment
  CONFIRMED               // Answer confirmed, waiting for next question
};

State currentState = IDLE;
uint8_t currentQuestion = 0;
char selectedAnswer = '?';
unsigned long stateTimer = 0;
unsigned long requestTimer = 0;

#define REQUEST_TIMEOUT 2000    // Timeout for grant response
#define REQUEST_RETRY_DELAY 300 // Delay between request retries
#define ACK_TIMEOUT 2000        // Timeout for acknowledgment

void setup() {
  Serial.begin(9600);
  
  // Initialize RF
  if (!rf_driver.init()) {
    Serial.println("RF init failed!");
    while (1);
  }
  
  // Initialize LEDs
  pinMode(LED_ORANGE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_ORANGE, LOW);
  digitalWrite(LED_GREEN, LOW);
  
  // Startup indication
  blinkStartup();
  
  Serial.println("=================================");
  Serial.print("ANSWERING DEVICE #");
  Serial.println(DEVICE_ID);
  Serial.println("=================================");
  Serial.println("Waiting for questions...");
  Serial.println();
}

void loop() {
  // Check for incoming RF messages
  checkIncomingMessages();
  
  // Handle keypad input
  if (currentState == WAITING_FOR_INPUT) {
    char key = keypad.getKey();
    if (key) {
      handleKeyPress(key);
    }
  }
  
  // State machine logic
  switch (currentState) {
    case INPUT_RECEIVED:
      // Wait a moment then request permission
      if (millis() - stateTimer > random(50, 200)) {
        requestPermission();
      }
      break;
      
    case REQUESTING:
    case WAITING_GRANT:
      // Retry request if no grant received
      if (millis() - requestTimer > REQUEST_RETRY_DELAY) {
        if (millis() - stateTimer > REQUEST_TIMEOUT) {
          Serial.println("Timeout: No grant received. Retrying...");
          stateTimer = millis();
        }
        requestPermission();
      }
      break;
      
    case WAITING_ACK:
      // Timeout waiting for acknowledgment
      if (millis() - stateTimer > ACK_TIMEOUT) {
        Serial.println("Timeout: No ACK received. Retrying...");
        currentState = INPUT_RECEIVED;
        stateTimer = millis();
      }
      break;
      
    case CONFIRMED:
      // Just wait for next question poll
      break;
  }
}

void checkIncomingMessages() {
  uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
  uint8_t buflen = sizeof(buf);
  
  if (rf_driver.recv(buf, &buflen)) {
    buf[buflen] = '\0';
    processMessage((char*)buf, buflen);
  }
}

void processMessage(char* msg, uint8_t len) {
  if (len < 3) return;
  
  char msgType = msg[0];
  
  if (msgType == MSG_POLL) {
    // Format: "P,questionNum"
    uint8_t questionNum = atoi(msg + 2);
    handlePoll(questionNum);
    
  } else if (msgType == MSG_GRANT) {
    // Format: "G,deviceId"
    uint8_t deviceId = atoi(msg + 2);
    if (deviceId == DEVICE_ID) {
      handleGrant();
    }
    
  } else if (msgType == MSG_ACK) {
    // Format: "K,deviceId,questionNum"
    uint8_t deviceId = 0;
    uint8_t questionNum = 0;
    
    char* token = strtok(msg + 2, ",");
    if (token) deviceId = atoi(token);
    token = strtok(NULL, ",");
    if (token) questionNum = atoi(token);
    
    if (deviceId == DEVICE_ID && questionNum == currentQuestion) {
      handleAcknowledgment();
    }
  }
}

void handlePoll(uint8_t questionNum) {
  // New question received
  if (questionNum != currentQuestion) {
    currentQuestion = questionNum;
    selectedAnswer = '?';
    currentState = WAITING_FOR_INPUT;
    
    digitalWrite(LED_ORANGE, LOW);
    digitalWrite(LED_GREEN, LOW);
    
    Serial.println("---------------------------------");
    Serial.print("Question ");
    Serial.print(currentQuestion);
    Serial.println(" - Select answer (A/B/C/D)");
  }
}

void handleKeyPress(char key) {
  selectedAnswer = key;
  currentState = INPUT_RECEIVED;
  stateTimer = millis();
  
  Serial.print("Answer selected: ");
  Serial.println(selectedAnswer);
  Serial.println("Requesting to submit...");
  
  digitalWrite(LED_ORANGE, HIGH); // Turn on orange LED
}

void requestPermission() {
  // Send request: "R,deviceId"
  char requestMsg[10];
  snprintf(requestMsg, sizeof(requestMsg), "%c,%d", MSG_REQUEST, DEVICE_ID);
  rf_driver.send((uint8_t*)requestMsg, strlen(requestMsg));
  rf_driver.waitPacketSent();
  
  currentState = WAITING_GRANT;
  requestTimer = millis();
}

void handleGrant() {
  Serial.println("Permission granted! Sending answer...");
  currentState = SENDING_ANSWER;
  
  // Send answer: "A,deviceId,answer"
  char answerMsg[15];
  snprintf(answerMsg, sizeof(answerMsg), "%c,%d,%c", MSG_ANSWER, DEVICE_ID, selectedAnswer);
  rf_driver.send((uint8_t*)answerMsg, strlen(answerMsg));
  rf_driver.waitPacketSent();
  
  currentState = WAITING_ACK;
  stateTimer = millis();
}

void handleAcknowledgment() {
  Serial.println("Answer confirmed!");
  Serial.println();
  
  digitalWrite(LED_ORANGE, LOW);
  digitalWrite(LED_GREEN, HIGH);  // Turn on green LED
  
  currentState = CONFIRMED;
  
  // Keep green LED on for 2 seconds
  delay(2000);
  digitalWrite(LED_GREEN, LOW);
  
  currentState = IDLE;
}

void blinkStartup() {
  // Blink both LEDs to indicate device is ready
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_ORANGE, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    delay(200);
    digitalWrite(LED_ORANGE, LOW);
    digitalWrite(LED_GREEN, LOW);
    delay(200);
  }
}