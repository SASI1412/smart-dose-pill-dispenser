#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <Servo.h>

// Initialize components
LiquidCrystal_I2C lcd(0x27, 16, 2); // Change LCD address if needed
RTC_DS3231 rtc;

// Servo objects
Servo servo1, servo2, servo3, servo4;

// Pin definitions
const int BUTTON_PIN = 7;
const int BUZZER_PIN = 8;
const int SERVO1_PIN = 9;c
const int SERVO2_PIN = 10;
const int SERVO3_PIN = 11;
const int SERVO4_PIN = 12;

// LED pins for each slot
const int LED1_PIN = 2;
const int LED2_PIN = 3;
const int LED3_PIN = 4;
const int LED4_PIN = 5;

// Manual override button pins
const int MANUAL_BUTTON1_PIN = A0;
const int MANUAL_BUTTON2_PIN = A1;
const int MANUAL_BUTTON3_PIN = A2;
const int MANUAL_BUTTON4_PIN = A3;

// Default time slots (will be overwritten by app data)
int SLOT_START_HOURS[4] = {9, 14, 19, 21};
int SLOT_START_MINUTES[4] = {0, 0, 0, 0};
int SLOT_END_HOURS[4] = {9, 14, 19, 21};
int SLOT_END_MINUTES[4] = {3, 3, 3, 3}; // Changed to 3 minutes duration

// Tablet names and counts for each slot
String tabletNames[4] = {"", "", "", ""};
int tabletCounts[4] = {0, 0, 0, 0};
String slotNames[4] = {"Morning", "Afternoon", "Evening", "Night"};

// State variables for each slot
enum SystemState { SLOT_OPEN, SLOT_CLOSE, TABLET_TAKEN, SLOT_MISSED, OUTSIDE_SLOT };
SystemState currentSlotState[4] = {SLOT_CLOSE, SLOT_CLOSE, SLOT_CLOSE, SLOT_CLOSE};
bool slotCompleted[4] = {false, false, false, false};
bool slotActiveToday[4] = {false, false, false, false};
bool slotMissedChecked[4] = {false, false, false, false};
bool ledActive[4] = {false, false, false, false};
bool slotConfigured[4] = {false, false, false, false};

// Manual override states
bool manualOverrideActive = false;
bool servoManualState[4] = {false, false, false, false};
unsigned long lastManualButtonPress[4] = {0, 0, 0, 0};
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_DELAY = 300;
const unsigned long MANUAL_DEBOUNCE_DELAY = 500;

int currentActiveSlot = -1;
bool dayReset = false;

// Serial communication
String inputString = "";
bool stringComplete = false;

// LCD display state management
unsigned long lastLCDUpdate = 0;
const unsigned long LCD_UPDATE_INTERVAL = 500;
String currentLine1 = "";
String currentLine2 = "";
bool displayInitialized = false;
// --- Function Prototypes ---
void showTemporaryMessage(String line1, String line2, unsigned long duration);
void updateLCD();
void checkSerialData();
void checkManualOverrideButtons();
void resetAllSlots();
void updateLEDFromServoState();
int getCurrentActiveSlot(DateTime now);
void handleTimeSlot(DateTime now, int slotIndex);
void handleOutsideTimeSlot(DateTime now);
void checkMissedSlotsImmediately(DateTime now);
String getTimeString(DateTime now);
int getTimeRemainingInSlot(DateTime now, int slotIndex);
String truncateString(String input, int maxLength);
String getNextSlotName(DateTime now);
void displayPreviousSlotStatus(DateTime now);
void activateBuzzer(int duration);



unsigned long displayMessageTimeout = 0;
bool showingTemporaryMessage = false;
String tempLine1 = "";
String tempLine2 = "";

// Buzzer timing variables
unsigned long lastMinuteBuzzerTime[4] = {0, 0, 0, 0};
bool initialBuzzerSounded[4] = {false, false, false, false};
bool minuteBuzzerActive[4] = {false, false, false, false};

void setup() {
  // Initialize Serial (pins 0 and 1) for HC-05
  Serial.begin(9600);
  inputString.reserve(200);
  
  // Initialize pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Initialize LED pins
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(LED4_PIN, OUTPUT);
  
  // Initialize manual button pins
  pinMode(MANUAL_BUTTON1_PIN, INPUT_PULLUP);
  pinMode(MANUAL_BUTTON2_PIN, INPUT_PULLUP);
  pinMode(MANUAL_BUTTON3_PIN, INPUT_PULLUP);
  pinMode(MANUAL_BUTTON4_PIN, INPUT_PULLUP);
  
  // Initialize Servos
  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo3.attach(SERVO3_PIN);
  servo4.attach(SERVO4_PIN);
  
  // Initialize all servos to closed position (90 degrees)
  servo1.write(90);
  servo2.write(90);
  servo3.write(90);
  servo4.write(90);
  
  // Turn off all LEDs initially
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(LED3_PIN, LOW);
  digitalWrite(LED4_PIN, LOW);
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // Initialize RTC
  if (!rtc.begin()) {
    lcd.print("RTC Not Found!");
    while (1);
  }
  
  // Set RTC time initially
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  showTemporaryMessage("4-Slot System", "Initializing...", 2000);
  Serial.println("4-Slot System Ready");
  
  // Initialize display
  currentLine1 = "System Ready";
  currentLine2 = "Waiting...";
  updateLCD();
  displayInitialized = true;
}

void loop() {
  DateTime now = rtc.now();
  
  // Check for serial data from app
  checkSerialData();
  
  // Check for manual override buttons first (highest priority)
  checkManualOverrideButtons();
  
  // If manual override is active, skip automatic operations
  if (manualOverrideActive) {
    return;
  }
  
  // Reset all slots at midnight
  if (now.hour() == 0 && now.minute() == 0 && !dayReset) {
    resetAllSlots();
    dayReset = true;
  }
  if (now.hour() != 0) {
    dayReset = false;
  }
  
  // Update LED indicators based on servo state
  updateLEDFromServoState();
  
  // Check which slot is currently active
  currentActiveSlot = getCurrentActiveSlot(now);
  if (currentActiveSlot != -1) {
    handleTimeSlot(now, currentActiveSlot);
  } else {
    handleOutsideTimeSlot(now);
  }
  
  // Check for missed slots immediately after each slot's end time
  checkMissedSlotsImmediately(now);
  
  // Update LCD display with throttling
  updateMainDisplay(now);
  
  delay(100);
}

void updateMainDisplay(DateTime now) {
  // Check if we're showing a temporary message
  if (showingTemporaryMessage) {
  if (millis() > displayMessageTimeout) {
    // Just stop showing temporary message
    showingTemporaryMessage = false;
    return;  // Don't clear LCD — keep message visible until next refresh
  } else {
    return;  // Still showing temp message, skip regular display updates
  }
}



  if (millis() - lastLCDUpdate > LCD_UPDATE_INTERVAL) {
    lastLCDUpdate = millis();
    String newLine1, newLine2;

    if (currentActiveSlot != -1) {
      // ✅ During active slot
      newLine1 = getTimeString(now);

      // Calculate time remaining
      int timeRemaining = getTimeRemainingInSlot(now, currentActiveSlot);

      // Only show OPEN if servo is actually open
      String stateIndicator = (currentSlotState[currentActiveSlot] == SLOT_OPEN) ? "OPEN" : "";

      // Build second line dynamically
      newLine2 = truncateString(tabletNames[currentActiveSlot], 8) + " C:" + String(tabletCounts[currentActiveSlot]);

      // Append OPEN only when needed
      if (stateIndicator != "") {
        newLine2 += " " + stateIndicator;
      }

      // Append remaining time
      newLine2 += " " + String(timeRemaining) + "Min";
    } 
    else {
      // ✅ Outside slot: show current time and next slot info
      newLine1 = getTimeString(now);

      // Determine next slot name or show "No upcoming slots"
      String nextSlotMsg = getNextSlotName(now);
      if (nextSlotMsg == "None") {
        newLine2 = "No upcoming slots";
      } else {
        newLine2 = "Next Slot: " + nextSlotMsg;
      }
    }

    // Update LCD only if content changed
    if (newLine1 != currentLine1 || newLine2 != currentLine2) {
      currentLine1 = newLine1;
      currentLine2 = newLine2;
      updateLCD();
    }
  }
}
String getNextSlotName(DateTime now) {
  int currentTotalMinutes = now.hour() * 60 + now.minute();
  int nextSlot = -1;
  bool anyConfigured = false;

  for (int i = 0; i < 4; i++) {
    if (!slotConfigured[i]) continue;
    anyConfigured = true;

    int slotStartTotalMinutes = SLOT_START_HOURS[i] * 60 + SLOT_START_MINUTES[i];
    if (slotStartTotalMinutes > currentTotalMinutes) {
      nextSlot = i;
      break;
    }
  }

  if (!anyConfigured) return "None";
  if (nextSlot == -1) return "None";

  // Example: "Dolo@10:30"
  String formatted = tabletNames[nextSlot] + "@";
  if (SLOT_START_HOURS[nextSlot] < 10) formatted += "0";
  formatted += String(SLOT_START_HOURS[nextSlot]);
  formatted += ":";
  if (SLOT_START_MINUTES[nextSlot] < 10) formatted += "0";
  formatted += String(SLOT_START_MINUTES[nextSlot]);
  return formatted;
}

String getTimeString(DateTime now) {
  String timeStr = "Time: ";
  if (now.hour() < 10) timeStr += "0";
  timeStr += String(now.hour());
  timeStr += ":";
  if (now.minute() < 10) timeStr += "0";
  timeStr += String(now.minute());
  timeStr += ":";
  if (now.second() < 10) timeStr += "0";
  timeStr += String(now.second());
  return timeStr;
}


int getTimeRemainingInSlot(DateTime now, int slotIndex) {
  int currentTotalMinutes = now.hour() * 60 + now.minute();
  int slotEndTotalMinutes = SLOT_END_HOURS[slotIndex] * 60 + SLOT_END_MINUTES[slotIndex];
  return slotEndTotalMinutes - currentTotalMinutes;
}

String getStatusMessage(DateTime now) {
  int currentHour = now.hour();
  int currentMinute = now.minute();
  
  // Count configured slots
  int configuredCount = 0;
  for (int i = 0; i < 4; i++) {
    if (slotConfigured[i]) configuredCount++;
  }
  
  if (configuredCount == 0) {
    return "No tablets set";
  }
  
  // Find next upcoming slot
  int nextSlot = -1;
  unsigned long minTimeToNext = 24 * 60; // Large value
  
  for (int i = 0; i < 4; i++) {
    if (!slotConfigured[i]) continue;
    
    int slotStartTotalMinutes = SLOT_START_HOURS[i] * 60 + SLOT_START_MINUTES[i];
    int currentTotalMinutes = currentHour * 60 + currentMinute;
    
    if (slotStartTotalMinutes > currentTotalMinutes) {
      unsigned long timeToNext = slotStartTotalMinutes - currentTotalMinutes;
      if (timeToNext < minTimeToNext) {
        minTimeToNext = timeToNext;
        nextSlot = i;
      }
    }
  }
  
  if (nextSlot != -1) {
    return "Next: " + truncateString(tabletNames[nextSlot], 12);
  }
  
  // Check if we're after last slot
  int lastSlot = -1;
  for (int i = 3; i >= 0; i--) {
    if (slotConfigured[i]) {
      lastSlot = i;
      break;
    }
  }
  
  if (lastSlot != -1) {
    if (currentHour > SLOT_END_HOURS[lastSlot] || 
        (currentHour == SLOT_END_HOURS[lastSlot] && currentMinute >= SLOT_END_MINUTES[lastSlot])) {
      return "Day Complete";
    }
  }
  
  return "Outside Time";
}

String truncateString(String input, int maxLength) {
  if (input.length() <= maxLength) {
    return input;
  } else {
    return input.substring(0, maxLength);
  }
}

void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(currentLine1);
  lcd.setCursor(0, 1);
  lcd.print(currentLine2);
}

void showTemporaryMessage(String line1, String line2, unsigned long duration) {
  tempLine1 = truncateString(line1, 16);
  tempLine2 = truncateString(line2, 16);
  showingTemporaryMessage = true;
  displayMessageTimeout = millis() + duration;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(tempLine1);
  lcd.setCursor(0, 1);
  lcd.print(tempLine2);
}


void checkSerialData() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n' || inChar == '\r') {
      if (inputString.length() > 0) {
        stringComplete = true;
        break;
      }
    } else {
      if (isPrintable(inChar)) {
        inputString += inChar;
      }
    }
  }
  
  if (stringComplete) {
    inputString.trim();
    Serial.print("Raw received: '");
    Serial.print(inputString);
    Serial.println("'");
    
    int firstUnderscore = inputString.indexOf('_');
    if (firstUnderscore != -1) {
      int secondUnderscore = inputString.indexOf('_', firstUnderscore + 1);
      if (secondUnderscore != -1) {
        processTimeConfiguration(inputString);
      } else {
        Serial.println("ERROR: Invalid format - missing count");
      }
    } else {
      Serial.println("ERROR: Invalid format - no underscores");
    }
    
    inputString = "";
    stringComplete = false;
  }
}

void processTimeConfiguration(String data) {
  data.trim();
  Serial.print("Processing: '");
  Serial.print(data);
  Serial.println("'");
  
  int underscoreIndex1 = data.indexOf('_');
  if (underscoreIndex1 == -1) {
    Serial.println("ERROR: Invalid format - no first underscore");
    return;
  }
  
  String tabletName = data.substring(0, underscoreIndex1);
  tabletName.trim();
  
  int underscoreIndex2 = data.indexOf('_', underscoreIndex1 + 1);
  if (underscoreIndex2 == -1) {
    Serial.println("ERROR: Invalid format - missing count");
    return;
  }
  
  String timeStr = data.substring(underscoreIndex1 + 1, underscoreIndex2);
  timeStr.trim();
  
  String countStr = data.substring(underscoreIndex2 + 1);
  countStr.trim();
  
  int colonIndex = timeStr.indexOf(':');
  if (colonIndex == -1) {
    Serial.println("ERROR: Invalid time format");
    return;
  }
  
  int hour = timeStr.substring(0, colonIndex).toInt();
  int minute = timeStr.substring(colonIndex + 1).toInt();
  int count = countStr.toInt();
  
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
    Serial.println("ERROR: Invalid time values");
    return;
  }
  
  if (count <= 0) {
    Serial.println("ERROR: Invalid count value");
    return;
  }
  
  int slotIndex = -1;
  if (hour >= 0 && hour < 11) {
    slotIndex = 0;
  } else if (hour >= 11 && hour < 16) {
    slotIndex = 1;
  } else if (hour >= 16 && hour < 20) {
    slotIndex = 2;
  } else {
    slotIndex = 3;
  }
  
  SLOT_START_HOURS[slotIndex] = hour;
  SLOT_START_MINUTES[slotIndex] = minute;
  
  // Set end time to start time + 3 minutes
  SLOT_END_HOURS[slotIndex] = hour;
  SLOT_END_MINUTES[slotIndex] = minute + 3;
  
  // Handle minute overflow
  if (SLOT_END_MINUTES[slotIndex] >= 60) {
    SLOT_END_MINUTES[slotIndex] -= 60;
    SLOT_END_HOURS[slotIndex] += 1;
    if (SLOT_END_HOURS[slotIndex] >= 24) {
      SLOT_END_HOURS[slotIndex] -= 24;
    }
  }
  
  tabletNames[slotIndex] = tabletName;
  tabletCounts[slotIndex] = count;
  slotConfigured[slotIndex] = true;
  
  Serial.print("CONFIRMED: ");
  Serial.print(slotNames[slotIndex]);
  Serial.print(" slot set for ");
  Serial.print(tabletName);
  Serial.print(" at ");
  Serial.print(hour);
  Serial.print(":");
  if (minute < 10) Serial.print("0");
  Serial.print(minute);
  Serial.print(" to ");
  Serial.print(SLOT_END_HOURS[slotIndex]);
  Serial.print(":");
  if (SLOT_END_MINUTES[slotIndex] < 10) Serial.print("0");
  Serial.print(SLOT_END_MINUTES[slotIndex]);
  Serial.print(" Count: ");
  Serial.println(count);
  
  // Show confirmation on LCD
  String confirmLine1 = slotNames[slotIndex] + " Set";
  String confirmLine2 = tabletName + " C:" + String(count);
  showTemporaryMessage(confirmLine1, confirmLine2, 3000);
}

void updateLEDFromServoState() {
  DateTime now = rtc.now();
  int currentTotalMinutes = now.hour() * 60 + now.minute();

  for (int i = 0; i < 4; i++) {
    bool shouldLEDBeOn = false;

    if (!slotConfigured[i]) {
      shouldLEDBeOn = false;
    } else {
      int slotStartTotalMinutes = SLOT_START_HOURS[i] * 60 + SLOT_START_MINUTES[i];
      int slotEndTotalMinutes = SLOT_END_HOURS[i] * 60 + SLOT_END_MINUTES[i];

      // LED stays ON if:
      // 1️⃣ Slot is currently active (time within slot)
      // 2️⃣ Servo is open (manual or auto)
      // 3️⃣ Tablet not yet taken
      if ((currentTotalMinutes >= slotStartTotalMinutes && currentTotalMinutes < slotEndTotalMinutes && !slotCompleted[i]) ||
          (currentSlotState[i] == SLOT_OPEN)) {
        shouldLEDBeOn = true;
      } else {
        shouldLEDBeOn = false;
      }
    }

    switch (i) {
      case 0: digitalWrite(LED1_PIN, shouldLEDBeOn ? HIGH : LOW); break;
      case 1: digitalWrite(LED2_PIN, shouldLEDBeOn ? HIGH : LOW); break;
      case 2: digitalWrite(LED3_PIN, shouldLEDBeOn ? HIGH : LOW); break;
      case 3: digitalWrite(LED4_PIN, shouldLEDBeOn ? HIGH : LOW); break;
    }

    ledActive[i] = shouldLEDBeOn;  // Keep internal LED state synced
  }
}

void checkManualOverrideButtons() {
  int manualButtonPins[4] = {MANUAL_BUTTON1_PIN, MANUAL_BUTTON2_PIN, MANUAL_BUTTON3_PIN, MANUAL_BUTTON4_PIN};
  int ledPins[4] = {LED1_PIN, LED2_PIN, LED3_PIN, LED4_PIN};

  for (int i = 0; i < 4; i++) {
    if (digitalRead(manualButtonPins[i]) == LOW &&
        millis() - lastManualButtonPress[i] > MANUAL_DEBOUNCE_DELAY) {

      lastManualButtonPress[i] = millis();
      manualOverrideActive = true;

      // Toggle servo state
      servoManualState[i] = !servoManualState[i];

      // Control servo movement
      if (servoManualState[i]) {
        switch (i) {
          case 0: servo1.write(180); break;
          case 1: servo2.write(180); break;
          case 2: servo3.write(180); break;
          case 3: servo4.write(180); break;
        }
        Serial.println("MANUAL: Servo " + String(i + 1) + " opened");
      } else {
        switch (i) {
          case 0: servo1.write(90); break;
          case 1: servo2.write(90); break;
          case 2: servo3.write(90); break;
          case 3: servo4.write(90); break;
        }
        Serial.println("MANUAL: Servo " + String(i + 1) + " closed");
      }

      // ✅ LED mirrors servo state (ON when OPEN, OFF when CLOSED)
      ledActive[i] = servoManualState[i];
      digitalWrite(ledPins[i], ledActive[i] ? HIGH : LOW);

      // Feedback
      activateBuzzer(200);
      showTemporaryMessage("Manual Mode", slotNames[i] + " " + (servoManualState[i] ? "OPEN" : "CLOSE"), 2000);
    }
  }

  // Auto exit manual mode after 3 seconds of no button activity
  static unsigned long manualModeTimer = 0;
  if (manualOverrideActive) {
    bool anyButtonPressed = false;
    for (int i = 0; i < 4; i++) {
      if (digitalRead(manualButtonPins[i]) == LOW) {
        anyButtonPressed = true;
        manualModeTimer = millis();
        break;
      }
    }

   if (!anyButtonPressed && millis() - manualModeTimer > 3000) {
  manualOverrideActive = false;
  Serial.println("Exiting Manual Mode");
  showingTemporaryMessage = false;  // Stop manual message
  // Let updateMainDisplay() refresh the regular screen automatically
}


  } else {
    manualModeTimer = millis();
  }
}


void resetAllSlots() {
  for(int i = 0; i < 4; i++) {
    slotCompleted[i] = false;
    slotActiveToday[i] = false;
    slotMissedChecked[i] = false;
    currentSlotState[i] = SLOT_CLOSE;
    servoManualState[i] = false;
    ledActive[i] = false;
    initialBuzzerSounded[i] = false;
    minuteBuzzerActive[i] = false;
    lastMinuteBuzzerTime[i] = 0;
  }
  
  // Close all servos
  servo1.write(90);
  servo2.write(90);
  servo3.write(90);
  servo4.write(90);
  
  // Turn off all LEDs
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(LED3_PIN, LOW);
  digitalWrite(LED4_PIN, LOW);
  
  showTemporaryMessage("New Day", "All slots reset", 2000);
  Serial.println("All slots reset for new day");
}

int getCurrentActiveSlot(DateTime now) {
  int currentHour = now.hour();
  int currentMinute = now.minute();
  
  for (int i = 0; i < 4; i++) {
    if (!slotConfigured[i]) continue;
    
    int slotStartTotalMinutes = SLOT_START_HOURS[i] * 60 + SLOT_START_MINUTES[i];
    int slotEndTotalMinutes = SLOT_END_HOURS[i] * 60 + SLOT_END_MINUTES[i];
    int currentTotalMinutes = currentHour * 60 + currentMinute;
    
    if (currentTotalMinutes >= slotStartTotalMinutes && currentTotalMinutes < slotEndTotalMinutes) {
      return i;
    }
  }
  return -1;
}

void handleTimeSlot(DateTime now, int slotIndex) {
  slotMissedChecked[slotIndex] = false;
  
  // Sound 3-second buzzer at start of slot (only once)
  if (!initialBuzzerSounded[slotIndex]) {
    activateBuzzer(3000); // 3-second buzzer
    initialBuzzerSounded[slotIndex] = true;
    showTemporaryMessage("Time for:", tabletNames[slotIndex], 2000);
    Serial.println("Initial 3-second buzzer for " + tabletNames[slotIndex]);
  }
  
  // Sound 2-second buzzer every minute until tablet is taken
  if (!slotCompleted[slotIndex] && now.second() == 0) {
    unsigned long currentTime = millis();
    if (currentTime - lastMinuteBuzzerTime[slotIndex] >= 60000) { // Every minute
      activateBuzzer(2000); // 2-second buzzer
      lastMinuteBuzzerTime[slotIndex] = currentTime;
      minuteBuzzerActive[slotIndex] = true;
      Serial.println("Minute reminder buzzer for " + tabletNames[slotIndex]);
    }
  }
  
  if (digitalRead(BUTTON_PIN) == LOW && millis() - lastButtonPress > DEBOUNCE_DELAY) {
    lastButtonPress = millis();
    slotActiveToday[slotIndex] = true;
    
    if (currentSlotState[slotIndex] == SLOT_CLOSE && !slotCompleted[slotIndex]) {
      openSlot(slotIndex);
      showTemporaryMessage("Take your", tabletNames[slotIndex], 2000);
    } else if (currentSlotState[slotIndex] == SLOT_OPEN && !slotCompleted[slotIndex]) {
      closeSlot(slotIndex);
      if (tabletCounts[slotIndex] > 0) {
        tabletCounts[slotIndex]--;
      }
      showTemporaryMessage("Tablet Taken", "Remaining: " + String(tabletCounts[slotIndex]), 2000);
      Serial.print(tabletNames[slotIndex]);
      Serial.print(" -- taken ; remaining count : ");
      Serial.println(tabletCounts[slotIndex]);
      
      // Stop minute buzzers once tablet is taken
      minuteBuzzerActive[slotIndex] = false;
    } else if (slotCompleted[slotIndex]) {
      Serial.print(tabletNames[slotIndex]);
      Serial.print(" already taken ; remaining count : ");
      Serial.println(tabletCounts[slotIndex]);
      showTemporaryMessage("Already Taken", tabletNames[slotIndex], 2000);
      activateBuzzer(2000);
    }
  }
}

void openSlot(int slotIndex) {
  currentSlotState[slotIndex] = SLOT_OPEN;
  switch(slotIndex) {
    case 0: servo1.write(180); break;
    case 1: servo2.write(180); break;
    case 2: servo3.write(180); break;
    case 3: servo4.write(180); break;
  }
  Serial.print("AUTO: ");
  Serial.print(slotNames[slotIndex]);
  Serial.println(" slot opened");
}

void closeSlot(int slotIndex) {
  currentSlotState[slotIndex] = SLOT_CLOSE;
  slotCompleted[slotIndex] = true;
  switch(slotIndex) {
    case 0: servo1.write(90); break;
    case 1: servo2.write(90); break;
    case 2: servo3.write(90); break;
    case 3: servo4.write(90); break;
  }
  Serial.print("AUTO: ");
  Serial.print(slotNames[slotIndex]);
  Serial.println(" slot closed");
}

void handleOutsideTimeSlot(DateTime now) {
  if (digitalRead(BUTTON_PIN) == LOW && millis() - lastButtonPress > DEBOUNCE_DELAY) {
    lastButtonPress = millis();

    // ✅ Display previous slot status immediately and hold for 3s
    displayPreviousSlotStatus(now);
    activateBuzzer(1000);

    // ✅ Freeze normal LCD updates for 3s to prevent flicker
    showingTemporaryMessage = true;
    displayMessageTimeout = millis() + 3000;
  }
}



void displayPreviousSlotStatus(DateTime now) {
  int currentHour = now.hour();
  int currentMinute = now.minute();
  int previousSlot = -1;

  for (int i = 3; i >= 0; i--) {
    if (!slotConfigured[i]) continue;
    if (currentHour > SLOT_END_HOURS[i] ||
        (currentHour == SLOT_END_HOURS[i] && currentMinute >= SLOT_END_MINUTES[i])) {
      previousSlot = i;
      break;
    }
  }

  if (previousSlot == -1) {
    showTemporaryMessage("No previous", "slot today", 3000);
    return;
  }

  // ✅ Build display lines
  String statusLine1 = tabletNames[previousSlot];
  statusLine1 += (slotCompleted[previousSlot]) ? ": Taken" : ": Missed";

  String statusLine2 = "Remaining: " + String(tabletCounts[previousSlot]);

  // ✅ Display both lines immediately
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(truncateString(statusLine1, 16));
  lcd.setCursor(0, 1);
  lcd.print(truncateString(statusLine2, 16));

  // ✅ Update global display buffer to prevent overwriting
  currentLine1 = statusLine1;
  currentLine2 = statusLine2;

  // ✅ Keep message visible for 3 seconds
  showingTemporaryMessage = true;
  displayMessageTimeout = millis() + 3000;
}




void checkMissedSlotsImmediately(DateTime now) {
  int currentHour = now.hour();
  int currentMinute = now.minute();
  
  for (int i = 0; i < 4; i++) {
    if (!slotConfigured[i]) continue;
    
    if (currentHour == SLOT_END_HOURS[i] && currentMinute == SLOT_END_MINUTES[i] && !slotMissedChecked[i]) {
      if (!slotCompleted[i]) {
        currentSlotState[i] = SLOT_MISSED;
        slotMissedChecked[i] = true;
        
        // Close servo if it was open
        switch(i) {
          case 0: servo1.write(90); break;
          case 1: servo2.write(90); break;
          case 2: servo3.write(90); break;
          case 3: servo4.write(90); break;
        }
        
        // 5-second buzzer for missed tablet
        activateBuzzer(5000);
        
        Serial.print(tabletNames[i]);
        Serial.print(" -- missed ; remaining count : ");
        Serial.println(tabletCounts[i]);
        
        showTemporaryMessage("MISSED!", tabletNames[i], 3000);
      } else {
        slotMissedChecked[i] = true;
      }
    }
  }
}

void activateBuzzer(int duration) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}
