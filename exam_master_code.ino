/*
 * EXAM POLLING SYSTEM - MASTER DEVICE
 * 
 * Hardware Requirements:
 * - Arduino Uno/Nano
 * - 433MHz RF Transmitter (TX pin to Arduino pin 12)
 * - 433MHz RF Receiver (DATA pin to Arduino pin 11)
 * - Serial Monitor for displaying results
 * 
 * Library Required: RadioHead (Install via Arduino Library Manager)
 * Install: Sketch -> Include Library -> Manage Libraries -> Search "RadioHead"
 */

#include <RH_ASK.h>
#include <SPI.h>

// RF Communication setup
RH_ASK rf_driver(2000, 11, 12, 0); // Speed, RX pin, TX pin, PTT pin

// System configuration
#define MAX_DEVICES 50
#define RESPONSE_TIMEOUT 500    // Time to wait for device response (ms)
#define POLLING_INTERVAL 100    // Time between polling attempts (ms)
#define MAX_QUESTIONS 100

// Protocol message types
#define MSG_POLL 'P'           // Master polls for ready devices
#define MSG_REQUEST 'R'        // Slave requests to submit answer
#define MSG_GRANT 'G'          // Master grants permission to send
#define MSG_ANSWER 'A'         // Slave sends answer
#define MSG_ACK 'K'            // Master acknowledges receipt

// Data structures
struct Answer {
  uint8_t questionNum;
  uint8_t deviceId;
  char answer;
  unsigned long timestamp;
};

Answer answerLog[MAX_DEVICES * MAX_QUESTIONS];
uint16_t answerCount = 0;
uint8_t currentQuestion = 1;
uint8_t totalQuestions = 10; // Modify as needed

// State management
unsigned long lastPollTime = 0;
bool waitingForAnswers = true;
uint8_t devicesAnswered[MAX_DEVICES];
uint8_t deviceAnswerCount = 0;

void setup() {
  Serial.begin(9600);
  
  if (!rf_driver.init()) {
    Serial.println("RF init failed!");
    while (1);
  }
  
  Serial.println("=================================");
  Serial.println("EXAM POLLING SYSTEM - MASTER");
  Serial.println("=================================");
  Serial.println();
  Serial.print("Total Questions: ");
  Serial.println(totalQuestions);
  Serial.println();
  Serial.println("Starting Question 1...");
  Serial.println("---------------------------------");
  
  memset(devicesAnswered, 0, sizeof(devicesAnswered));
}

void loop() {
  // Check for incoming messages
  checkIncomingMessages();
  
  // Periodically poll for ready devices
  if (waitingForAnswers && (millis() - lastPollTime >= POLLING_INTERVAL)) {
    pollForReadyDevices();
    lastPollTime = millis();
  }
  
  // Check for manual question advancement (optional)
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'n' || cmd == 'N') {
      advanceToNextQuestion();
    } else if (cmd == 'r' || cmd == 'R') {
      printResults();
    }
  }
}

void pollForReadyDevices() {
  // Broadcast poll message: "P,questionNum"
  char pollMsg[10];
  snprintf(pollMsg, sizeof(pollMsg), "%c,%d", MSG_POLL, currentQuestion);
  rf_driver.send((uint8_t*)pollMsg, strlen(pollMsg));
  rf_driver.waitPacketSent();
}

void checkIncomingMessages() {
  uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
  uint8_t buflen = sizeof(buf);
  
  if (rf_driver.recv(buf, &buflen)) {
    buf[buflen] = '\0'; // Null terminate
    processMessage((char*)buf, buflen);
  }
}

void processMessage(char* msg, uint8_t len) {
  if (len < 3) return; // Invalid message
  
  char msgType = msg[0];
  
  if (msgType == MSG_REQUEST) {
    // Format: "R,deviceId"
    uint8_t deviceId = atoi(msg + 2);
    
    // Check if device already answered this question
    if (hasDeviceAnswered(deviceId)) {
      return; // Ignore duplicate request
    }
    
    // Grant permission to this device
    grantPermission(deviceId);
    
  } else if (msgType == MSG_ANSWER) {
    // Format: "A,deviceId,answer"
    uint8_t deviceId = 0;
    char answer = '?';
    
    // Parse message
    char* token = strtok(msg + 2, ",");
    if (token) deviceId = atoi(token);
    token = strtok(NULL, ",");
    if (token) answer = token[0];
    
    // Record answer
    recordAnswer(deviceId, answer);
    
    // Send acknowledgment
    sendAcknowledgment(deviceId);
  }
}

void grantPermission(uint8_t deviceId) {
  // Send grant message: "G,deviceId"
  char grantMsg[10];
  snprintf(grantMsg, sizeof(grantMsg), "%c,%d", MSG_GRANT, deviceId);
  rf_driver.send((uint8_t*)grantMsg, strlen(grantMsg));
  rf_driver.waitPacketSent();
}

void recordAnswer(uint8_t deviceId, char answer) {
  if (answerCount >= (MAX_DEVICES * MAX_QUESTIONS)) {
    Serial.println("ERROR: Answer log full!");
    return;
  }
  
  // Store answer
  answerLog[answerCount].questionNum = currentQuestion;
  answerLog[answerCount].deviceId = deviceId;
  answerLog[answerCount].answer = answer;
  answerLog[answerCount].timestamp = millis();
  answerCount++;
  
  // Mark device as answered
  if (deviceAnswerCount < MAX_DEVICES) {
    devicesAnswered[deviceAnswerCount++] = deviceId;
  }
  
  // Display on serial
  Serial.print("Q");
  Serial.print(currentQuestion);
  Serial.print(" | Device ");
  Serial.print(deviceId);
  Serial.print(" | Answer: ");
  Serial.println(answer);
}

void sendAcknowledgment(uint8_t deviceId) {
  // Send ACK message: "K,deviceId,questionNum"
  char ackMsg[15];
  snprintf(ackMsg, sizeof(ackMsg), "%c,%d,%d", MSG_ACK, deviceId, currentQuestion);
  rf_driver.send((uint8_t*)ackMsg, strlen(ackMsg));
  rf_driver.waitPacketSent();
}

bool hasDeviceAnswered(uint8_t deviceId) {
  for (uint8_t i = 0; i < deviceAnswerCount; i++) {
    if (devicesAnswered[i] == deviceId) {
      return true;
    }
  }
  return false;
}

void advanceToNextQuestion() {
  if (currentQuestion >= totalQuestions) {
    Serial.println();
    Serial.println("=================================");
    Serial.println("EXAM COMPLETED!");
    Serial.println("=================================");
    printResults();
    waitingForAnswers = false;
    return;
  }
  
  currentQuestion++;
  deviceAnswerCount = 0;
  memset(devicesAnswered, 0, sizeof(devicesAnswered));
  
  Serial.println("---------------------------------");
  Serial.print("Starting Question ");
  Serial.println(currentQuestion);
  Serial.println("---------------------------------");
}

void printResults() {
  Serial.println();
  Serial.println("=================================");
  Serial.println("ANSWER LOG");
  Serial.println("=================================");
  Serial.println("Q# | Device | Answer | Time(ms)");
  Serial.println("---------------------------------");
  
  for (uint16_t i = 0; i < answerCount; i++) {
    Serial.print(answerLog[i].questionNum);
    Serial.print("  | ");
    Serial.print(answerLog[i].deviceId);
    Serial.print("      | ");
    Serial.print(answerLog[i].answer);
    Serial.print("      | ");
    Serial.println(answerLog[i].timestamp);
  }
  
  Serial.println("=================================");
  Serial.print("Total Answers Recorded: ");
  Serial.println(answerCount);
  Serial.println("=================================");
}