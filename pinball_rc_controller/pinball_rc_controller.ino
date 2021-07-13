#include <Servo.h>
#include <LiquidCrystal.h>

#define time_t unsigned long

/*#define GATE_SERVO_PIN 5
#define X_SERVO_PIN 4
#define Y_SERVO_PIN 3
#define START_SENSOR A0*/

#define GATE_SERVO_PIN 10
#define X_SERVO_PIN 9
#define Y_SERVO_PIN 8
#define START_SENSOR A0


#define LCD_RS 12
#define LCD_E 11
#define LCD_D4 5
#define LCD_D5 4
#define LCD_D6 3
#define LCD_D7 2

#define UPDATE_DELAY 20
#define TILT_MIN_VALUE 35
#define TILT_MID_VALUE 90
#define TILT_MAX_VALUE 145
// +/- 90 / TILT_FULL_SWING_DELAY * UPDATE_DELAY
#define TILT_SPEED 7

#define GATE_UP 70
#define GATE_DOWN 90

time_t lastStartGateTime = 0;

Servo gateServo; int gatePos = 90; int gateTargetPos = 90;
Servo xServo; int xPos = 90; int xTargetPos = 90;
Servo yServo; int yPos = 90; int yTargetPos = 90;

LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
long lcd_timer = -1; long lcd_time_offset;

void setup() {
  Serial.begin(57600);
  
  lcd.begin(16,2);
  
  gateServo.attach(GATE_SERVO_PIN);
  xServo.attach(X_SERVO_PIN);
  yServo.attach(Y_SERVO_PIN);
  gateServo.write(GATE_DOWN);
}

int lastValue = 0;
void loop() {
  processCommands();
  sendEvents();
  updateTiltServos();
  updateDisplay();
  delay(UPDATE_DELAY);
}

void processCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    int value; String state;
    
    switch (command[0]) {
      case 'x':
      case 'y':
        value = command.substring(1).toInt();
        if (value != 0) {
          //Serial.println(command);
          if (command[0] == 'x') {
            setServoTargetPos(xTargetPos, value);            
          } else {
            setServoTargetPos(yTargetPos, value);            
          }
        }
        break;
      case 'g':
        state = command.substring(1);
        if (state.equals("up")) {
          gateServo.write(GATE_UP);
        }
        if (state.equals("down")) {
          gateServo.write(GATE_DOWN);
        }
        break;
      case 'd':
        value = command[1] - '0';
        state = command.substring(2);
        lcd.setCursor(0,value);
        lcd.print(state);
        if (value == 1) {
          lcd_timer = -1;
        }
        break;
      case 'c':
        lcd_timer = millis();
        lcd_time_offset = command.substring(1).toInt();
        break;       
        
      default:
        break;
    }
  }
}

float baseValue = 256;
float downEdgeThreshold = 0.75f;
float upEdgeThreshold = 0.85f;
boolean startGateDown = false;

void sendEvents() {
  int currentValue = analogRead(START_SENSOR);
  if (currentValue > baseValue) baseValue = currentValue;
  else baseValue = baseValue * 0.98f + currentValue * 0.02f;

  if (currentValue < baseValue * downEdgeThreshold && !startGateDown) {
    time_t now = millis();
    time_t time = now - lastStartGateTime;
    lastStartGateTime = now;
    Serial.print((int)(time / 1000));
    Serial.print(".");
    Serial.println((int)(time % 1000));
    
    startGateDown = true;
  } else if (currentValue > baseValue * upEdgeThreshold && startGateDown) {
    startGateDown = false;
  }
}

void updateTiltServos() {
  if (xPos != xTargetPos) { 
    xPos = stepTo(xPos, xTargetPos, TILT_SPEED);
    xServo.write(xPos);
  }
  if (yPos != yTargetPos) {
    yPos = stepTo(yPos, yTargetPos, TILT_SPEED);
    yServo.write(yPos);
  }
}

char lineBuffer[16];
void updateDisplay() {
  if (lcd_timer >= 0) {
    long time = millis() - lcd_timer + lcd_time_offset;
    int millisec = time % 1000;
    int seconds = (time / 1000) % 60;
    int minutes = time / 60000;
    sprintf(lineBuffer, "    %02d:%02d:%03d   ",minutes,seconds,millisec);
    lcd.setCursor(0,1);
    lcd.print(lineBuffer);
  }
}

void setServoTargetPos(int& servoTargetPos, int pos) {
  if (pos == 90) servoTargetPos = TILT_MID_VALUE;
  else if (pos < 90) servoTargetPos = TILT_MIN_VALUE;
  else servoTargetPos = TILT_MAX_VALUE;
}

int stepTo(int a, int b, int x) {
  if (abs(a-b) <= x) return b;
  else if (a < b) return a + x;
  else return a - x;
}
