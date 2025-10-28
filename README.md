# Arduino_MCQ
Arduino project for your exam polling system using 433MHz RF modules with a collision-avoidance protocol.
I've created a complete Arduino-based exam polling system with collision avoidance! Here's what I've implemented:
System Overview
Communication Protocol (Prevents Signal Jamming):

Master broadcasts poll messages periodically
Slaves listen and only respond when they have an answer ready
Request-Grant mechanism: Slaves request permission before sending
Random delays: Built-in randomization to avoid simultaneous transmissions
Acknowledgment system: Ensures reliable delivery

Master Device Features:

Polls for ready devices every 100ms
Records answers with {question_number, device_id, answer, timestamp}
Tracks which devices have answered each question
Prevents duplicate submissions
Serial output displays real-time results
Can advance questions manually (send 'n') or print results (send 'r')

Slave Device Features:

Unique device ID (manually set in code)
2x2 keypad for A, B, C, D input
Orange LED: Lights up when waiting for acknowledgment
Green LED: Lights up when answer is confirmed
Automatic retry mechanism if no response
State machine prevents duplicate submissions

Hardware Connections
Master Device:

RF TX → Pin 12
RF RX → Pin 11

Slave Device (for each device):

RF TX → Pin 12
RF RX → Pin 11
Keypad Row 1 → Pin 2
Keypad Row 2 → Pin 3
Keypad Col 1 → Pin 4
Keypad Col 2 → Pin 5
Orange LED (+220Ω) → Pin 8
Green LED (+220Ω) → Pin 9

Setup Instructions:

Install RadioHead Library: Arduino IDE → Sketch → Include Library → Manage Libraries → Search "RadioHead" → Install
For each slave: Change DEVICE_ID value (1, 2, 3, etc.)
Upload master code to master Arduino
Upload slave code to each slave Arduino
Power on all devices

Usage:

Students press A/B/C/D on keypad → Orange LED turns on
Master grants permission → Device sends answer
Master acknowledges → Green LED turns on (2 seconds)
Repeat for next question

Manual controls on Master serial monitor:

Send n to advance to next question
Send r to print all results

The system handles multiple devices efficiently with built-in collision avoidance! 🎓
