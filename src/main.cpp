#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_Keypad.h>

// Define the serial communication for the Nextion display
#define nexSerial Serial1

#include <Nextion.h>

// Button Definitions
#define BUTTON1 2 //Green
#define BUTTON2 3   //RED
#define TRIGWIRE0 6   // yellow
#define TRIGWIRE1 7   //red
#define TRIGWIRE2 8   //white
#define TRIGWIRE3 9    //orange
#define TRIGWIRE4 10   //blue
#define TRIGWIRE5 11 // green
#define SOLENOID 11
#define LOCK 14
#define LBUTTON 15

// Define the outcomes for each wire
const int correctWire = TRIGWIRE2; // Change this to the correct wire
const int deductTimeWire1 = TRIGWIRE0; // Change this to the first wire that deducts time
const int deductTimeWire2 = TRIGWIRE1; // Change this to the second wire that deducts time
const int FireWire = TRIGWIRE5; // Change this to the second wire that deducts time
// Create a Nextion object
NexText countdownText = NexText(0, 1, "t1"); // Page 0, component ID 1, component name "t1"
NexText promptText = NexText(0, 2, "t0"); // Page 0, component ID 2, component name "t0"

const int countdownStart = 15 * 60; // 15 minutes in seconds
int countdownValue = countdownStart;
unsigned long previousMillis = 0;
unsigned long interval = 1000; // 1 second interval

// Debounce variables
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
unsigned long debounceDelay = 50; // 50 milliseconds debounce delay
bool lastButtonState1 = HIGH;
bool lastButtonState2 = HIGH;

enum ButtonPhase {
  PHASE_ONE,
  PHASE_TWO,
  PHASE_WIRE,
  PHASE_WIRE_PROMPT,
  PHASE_KEYCODE,
  PHASE_COMPLETE
};

ButtonPhase buttonPhase = PHASE_ONE;
unsigned long phaseStartTime = 0;

// Define the correct answers for the questions
const int correctAnswer1 = BUTTON1; // Change to BUTTON2 if needed
const int correctAnswer2 = BUTTON2; // Change to BUTTON1 if needed

// MCP23017 and LIS3DH objects
Adafruit_MCP23X17 mcp;
Adafruit_LIS3DH lis = Adafruit_LIS3DH();

// Accelerometer baseline
sensors_event_t baselineEvent;

// Keypad configuration
const byte ROWS = 4; // Four rows
const byte COLS = 4; // Four columns
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {8, 9, 10, 11}; // Connect to the row pinouts of the MCP23017 (A4, A5, A6, A7)
byte colPins[COLS] = {12, 13, 14, 15}; // Connect to the column pinouts of the MCP23017 (B4, B5, B6, B7)

Adafruit_Keypad keypad = Adafruit_Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void typewriterEffect(const char* message, NexText& textComponent, unsigned long delayTime) {
  String currentText = "";
  int charCount = 0;
  for (int i = 0; message[i] != '\0'; i++) {
    currentText += message[i];
    charCount++;
    if (charCount >= 30 && message[i] == ' ') {
      currentText += "\r\n";
      charCount = 0;
    }
    textComponent.setText(currentText.c_str());
    delay(delayTime);
  }
}

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);
  // Initialize serial communication for the Nextion display
  nexSerial.begin(9600);
  // Wait for the serial communication to initialize
  delay(500);

  // Initialize the Nextion display
  nexInit();

  // Set button pins as input with internal pull-up resistors
  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);
  pinMode(TRIGWIRE0, INPUT);
  pinMode(TRIGWIRE1, INPUT);
  pinMode(TRIGWIRE2, INPUT);
  pinMode(TRIGWIRE3, INPUT);
  pinMode(TRIGWIRE4, INPUT);
  pinMode(TRIGWIRE5, INPUT);
  pinMode(SOLENOID, OUTPUT);
  pinMode(LOCK, INPUT_PULLUP);
  pinMode(LBUTTON, INPUT_PULLUP);

  // Initialize I2C communication
  Wire.begin();

  // Initialize MCP23017
  mcp.begin_I2C();

  // Set MCP23017 pins for keypad as input with pull-up resistors
  for (byte i = 8; i <= 15; i++) {
    mcp.pinMode(i, INPUT);
    mcp.digitalWrite(i, HIGH); // Enable pull-up resistor
  }

  // Initialize LIS3DH
  if (!lis.begin(0x18)) {   // change this to 0x19 for alternative i2c address
    Serial.println("Could not start LIS3DH");
    while (1);
  }
  lis.setRange(LIS3DH_RANGE_2_G);   // 2, 4, 8 or 16 G!

  // Get the baseline accelerometer reading
  lis.getEvent(&baselineEvent);

  // Initialize the keypad
  keypad.begin();

  // Display the first prompt with typewriter effect
  typewriterEffect("Keep safety glasses on", promptText, 100);
  // Wait for 5 seconds
  delay(5000);
  // Display the second prompt with typewriter effect
  typewriterEffect("Disarm the device", promptText, 100);
  // Wait for 5 seconds
  delay(5000);
  // Display the third prompt with typewriter effect
  typewriterEffect("You have fifteen minutes to disarm starting now", promptText, 100);
  // Wait for 5 seconds
  delay(5000);
  // Display the fourth prompt with typewriter effect
  typewriterEffect("These instructions that follow will be provided once; each instruction will be displayed for a few seconds.", promptText, 100);
  // Wait for 5 seconds
  delay(5000);
  // Display the fifth prompt with typewriter effect
  typewriterEffect("Wires: Find the correct wire", promptText, 100);
  // Wait for 5 seconds
  delay(5000);
  // Display the sixth prompt with typewriter effect
  typewriterEffect("One wire will add additional time", promptText, 100);
  // Wait for 5 seconds
  delay(5000);
  // Display the seventh prompt with typewriter effect
  typewriterEffect("Some wires will subtract time or speed it up", promptText, 100);
  // Wait for 5 seconds
  delay(5000);
  // Display the eighth prompt with typewriter effect
  typewriterEffect("Other wires will do nothing", promptText, 100);
  // Wait for 5 seconds
  delay(5000);
  // Display the ninth prompt with typewriter effect
  typewriterEffect("Buttons: pressing buttons at the wrong time or wrong sequence will deduct time", promptText, 100);
  // Wait for 5 seconds
  delay(5000);
  // Display the tenth prompt with typewriter effect
  typewriterEffect("Keycode: A numeric sequence will disarm the device. Any incorrect sequence will deduct time ", promptText, 100);
  // Wait for 5 seconds
  delay(5000);  
  // Display the eleventh prompt with typewriter effect
  typewriterEffect("Key and lock: Each device has a corresponding physical key, which when inserted and turned", promptText, 100);
  // Wait for 5 seconds
  delay(5000);
  // Display the twelfth prompt with typewriter effect
  typewriterEffect("You may only use items on the table, the keycode can be decrypted by answering the questions in the manual on your time.", promptText, 100);
  // Wait for 5 seconds
  delay(5000);
  // Display the thirteenth prompt with typewriter effect
  typewriterEffect("Good luck", promptText, 100);
  // Wait for 5 seconds
  delay(5000);
  // Display the fourteenth prompt with typewriter effect
  typewriterEffect("See Manual", promptText, 100);
  // Wait for 3 seconds
  delay(3000);
}

void loop() {
  unsigned long currentMillis = millis();
  interval = 1000; // Normal countdown speed

  // Check if it's time to update the countdown
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Convert the countdown number to minutes and seconds
    int minutes = countdownValue / 60;
    int seconds = countdownValue % 60;
    char buffer[10];
    sprintf(buffer, "%02d:%02d", minutes, seconds);
    // Set the text of the Nextion component to the countdown number
    countdownText.setText(buffer);
    // Decrement the countdown value
    countdownValue--;

    // Reset the countdown if it reaches 0
    if (countdownValue < 0) {
      countdownValue = countdownStart;
      // Trigger the solenoid
      digitalWrite(SOLENOID, HIGH);
      delay(5000); // Keep the solenoid on for 5 seconds
      digitalWrite(SOLENOID, LOW);
    }
  }

  // Check the state of BUTTON1 with debouncing
  bool reading1 = digitalRead(BUTTON1);
  if (reading1 != lastButtonState1) {
    lastDebounceTime1 = currentMillis;
  }
  if ((currentMillis - lastDebounceTime1) > debounceDelay) {
    if (reading1 == LOW) {
      // Perform action for button 1 press
      if (buttonPhase == PHASE_ONE) {
        if (correctAnswer1 == BUTTON1) {
          typewriterEffect("Correct", promptText, 100);
          phaseStartTime = currentMillis;
          buttonPhase = PHASE_TWO;
        } else {
          countdownValue -= 30; // Subtract 30 seconds from the countdown
          typewriterEffect("Incorrect: Time deducted", promptText, 100);
        }
      } else if (buttonPhase == PHASE_TWO) {
        if (correctAnswer2 == BUTTON1) {
          typewriterEffect("Correct", promptText, 100);
          // Stop the countdown here
          countdownValue = -1; // Assuming this stops the countdown
        } else {
          countdownValue -= 30; // Subtract 30 seconds from the countdown
          typewriterEffect("Incorrect: Time deducted", promptText, 100);
        }
      }
    }
  }
  lastButtonState1 = reading1;

  // Check the state of BUTTON2 with debouncing
  bool reading2 = digitalRead(BUTTON2);
  if (reading2 != lastButtonState2) {
    lastDebounceTime2 = currentMillis;
  }
  if ((currentMillis - lastDebounceTime2) > debounceDelay) {
    if (reading2 == LOW) {
      // Perform action for button 2 press
      if (buttonPhase == PHASE_ONE) {
        if (correctAnswer1 == BUTTON2) { // Change to BUTTON1 if needed
          typewriterEffect("Correct", promptText, 100);
          phaseStartTime = currentMillis;
          buttonPhase = PHASE_TWO;
        } else {
          countdownValue -= 30; // Subtract 30 seconds from the countdown
          typewriterEffect("Incorrect: Time deducted", promptText, 100);
        }
      } else if (buttonPhase == PHASE_TWO) {
        if (correctAnswer2 == BUTTON2) { // Change to BUTTON1 if needed
          typewriterEffect("Correct", promptText, 100);
          // Stop the countdown here
          countdownValue = -1; // Assuming this stops the countdown
        } else {
          countdownValue -= 30; // Subtract 30 seconds from the countdown
          typewriterEffect("Incorrect: Time deducted", promptText, 100);
        }
      }
    }
  }
  lastButtonState2 = reading2;

  // Handle phase transitions
  if (buttonPhase == PHASE_ONE && (currentMillis - phaseStartTime) > 3000) {
    typewriterEffect("Classification velocity", promptText, 100);
    typewriterEffect("If your explosive is a high explosive, press green button , If low, press red button ", promptText, 100);
    phaseStartTime = currentMillis;
    buttonPhase = PHASE_TWO;
  }

  if (buttonPhase == PHASE_TWO && (currentMillis - phaseStartTime) > 3000) {
    typewriterEffect("Initiation suspectibility", promptText, 100);
    typewriterEffect("If your explosive is a primary explosive, press green button , If secondary, press red button ", promptText, 100);
    phaseStartTime = currentMillis;
    buttonPhase = PHASE_WIRE;
  }

  if (buttonPhase == PHASE_WIRE && (currentMillis - phaseStartTime) > 3000) {
    typewriterEffect(" Oxygen Balance", promptText, 100);
    phaseStartTime = currentMillis;
    buttonPhase = PHASE_WIRE_PROMPT;
  }

  if (buttonPhase == PHASE_WIRE_PROMPT) {
    if (currentMillis - phaseStartTime > 3000 && currentMillis - phaseStartTime <= 5000) {
      typewriterEffect("If your explosive has a negative oxygen balance, cut the orange wire ", promptText, 100);
    } else if (currentMillis - phaseStartTime > 5000 && currentMillis - phaseStartTime <= 7000) {
      typewriterEffect("If your explosive has a neutral oxygen balance, cut the blue wire", promptText, 100);
    } else if (currentMillis - phaseStartTime > 7000 && currentMillis - phaseStartTime <= 9000) {
      typewriterEffect("If your explosive has a neutral oxygen balance, cut the yellow wire", promptText, 100);
    }
  }

  // Check the state of TRIGWIREs
  if (buttonPhase == PHASE_WIRE_PROMPT) {
    if (digitalRead(correctWire) == HIGH) {
      // Handle correct wire cut logic
      typewriterEffect("Correct wire cut: Time added", promptText, 100);
      countdownValue += 20; // Add 20 seconds to the countdown
      buttonPhase = PHASE_KEYCODE;
    } else if (digitalRead(deductTimeWire1) == HIGH || digitalRead(deductTimeWire2) == HIGH) {
      // Handle time deduction wire cut logic
      typewriterEffect("Incorrect wire cut: Time deducted", promptText, 100);
      countdownValue -= 20; // Subtract 20 seconds from the countdown
    } else if (digitalRead(TRIGWIRE3) == HIGH || digitalRead(TRIGWIRE4) == HIGH || digitalRead(TRIGWIRE5) == HIGH) {
      // Handle no change wire cut logic
      typewriterEffect("Wire cut: No change", promptText, 100);
    }
  }

  // Define the correct keycode
  const String correctKeycode = "1234"; // Change this to the correct keycode
  static String enteredKeycode = "";

  if (buttonPhase == PHASE_KEYCODE) {
     typewriterEffect("Enter keycode: ", promptText, 100);
    keypad.tick();
    keypadEvent keyEvent = keypad.read();
    char key = keyEvent.bit.KEY;
    if (key != 0) {
      if (key == '4') {
        if (enteredKeycode == correctKeycode) {
          if (digitalRead(LOCK) == LOW && digitalRead(LBUTTON) == LOW) {
            typewriterEffect("Success", promptText, 100);
            // Stop the countdown
            countdownValue = -1;
            buttonPhase = PHASE_COMPLETE; // Assuming you have a phase to indicate completion
          }
        } else {
          typewriterEffect("Incorrect keycode: Time deducted", promptText, 100);
          countdownValue -= 90; // Subtract 90 seconds from the countdown
          enteredKeycode = ""; // Reset the entered keycode
        }
      } else if (key == '*' || key == '#' || key == 'A' || key == 'B' || key == 'C' || key == 'D') {
        typewriterEffect("Wrong button pressed: Time deducted", promptText, 100);
        countdownValue -= 15; // Subtract 15 seconds from the countdown
      } else {
        enteredKeycode += key; // Append the key to the entered keycode
      }
    }
  }

  // Check if LOCK and LBUTTON go low before correct keycode
  if (digitalRead(LOCK) == LOW && digitalRead(LBUTTON) == LOW && buttonPhase != PHASE_COMPLETE) {
    // Trigger the solenoid
    digitalWrite(SOLENOID, HIGH);
    delay(5000); // Keep the solenoid on for 5 seconds
    digitalWrite(SOLENOID, LOW);
  }

  // Check accelerometer for movement
  sensors_event_t event;
  lis.getEvent(&event);

  // Calculate the difference in acceleration
  float deltaX = abs(event.acceleration.x - baselineEvent.acceleration.x);
  float deltaY = abs(event.acceleration.y - baselineEvent.acceleration.y);
  float deltaZ = abs(event.acceleration.z - baselineEvent.acceleration.z);

  // Determine the severity of the movement
  float totalMovement = deltaX + deltaY + deltaZ;

  if (totalMovement > 1.0) { // Adjusted sensitivity
    int timeDeduction = 0;
    if (totalMovement > 5.0) {
      timeDeduction = 30; // Severe movement
    } else if (totalMovement > 6) {
      timeDeduction = 20; // Moderate movement
    } else {
      timeDeduction = 10; // Minor movement
    }
    countdownValue -= timeDeduction;
    typewriterEffect("Movement detected: Time deducted", promptText, 100);
  }
}