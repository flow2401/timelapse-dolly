/*
  based on
  Pro-Timer Free
  Gunther Wegner
  http://gwegner.de
  http://lrtimelapse.com
*/

#include <Arduino.h>
#include <Wire.h>
#include "LiquidCrystal.h"
#include "LCD_Keypad_Reader.h"			// credits to: http://www.hellonull.com/?p=282

//motor shield library
#include "Adafruit_MotorShield.h"
#include "utility/Adafruit_MS_PWMServoDriver.h"
//when using a different motor shield, add // to above and add yours here

//initialize your keypad here
LCD_Keypad_Reader keypad;
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);	//Pin assignments for SainSmart LCD Keypad Shield

//initialize your motor shield here
Adafruit_MotorShield AFMS = Adafruit_MotorShield();

Adafruit_DCMotor *drive = AFMS.getMotor(1);
Adafruit_DCMotor *pan = AFMS.getMotor(2);
Adafruit_DCMotor *tilt = AFMS.getMotor(3);
Adafruit_DCMotor *trigger = AFMS.getMotor(4);

// The shield uses the I2C SCL and SDA pins. On classic Arduinos
// this is Analog 4 and 5 so you can't use those for analogRead() anymore
// However, you can connect other I2C sensors to the I2C bus and share
// the I2C bus.

const String CAPTION = "Pro-MoCo 0.96";

const int NONE = 0;						// Key constants
const int SELECT = 1;
const int LEFT = 2;
const int UP = 3;
const int DOWN = 4;
const int RIGHT = 5;

const int BACK_LIGHT = 10;

const int RELEASE_TIME = 500;			// Shutter release time for camera

const int keyRepeatRate = 200;			// when held, key repeats 1000 / keyRepeatRate times per second
const int keySampleRate = 100;			// ms between checking keypad for key


int localKey = 0;						// The current pressed key
int lastKeyPressed = -1;				// The last pressed key

unsigned long lastKeyCheckTime = 0;
unsigned long lastKeyPressTime = 0;

int sameKeyCount = 0;
unsigned long previousMillis = 0;		// Timestamp of last shutter release
unsigned long runningTime = 0;

int mode = 1;
int motorNr = 0;
int motorSpeed [3] = {100, 100, 100};
float motorDurration [3] = {0.2, 0.2, 0.2};
int mode_i = 0;

float interval = 4.0;					// the current interval
long maxNoOfShots = 0;
int isRunning = 0;						// flag indicates intervalometer is running

int imageCount = 0;                   	// Image count since start of intervalometer

unsigned long rampDuration = 10;		// ramping duration
float rampTo = 0.0;						// ramping interval
unsigned long rampingStartTime = 0;		// ramping start time
unsigned long rampingEndTime = 0;		// ramping end time
float intervalBeforeRamping = 0;		// interval before ramping

boolean backLight = HIGH;				// The current settings for the backlight

const int SCR_TL_MODE = 0;
const int SCR_SMS = 1;
const int SCR_CONTINUOUS = 2;
const int SCR_CALIBRATE = 3;
const int SCR_INTERVAL = 4;				// menu workflow constants
const int SCR_SHOOTS = 5;
const int SCR_RUNNING = 6;
const int SCR_CONFIRM_END = 7;
const int SCR_SETTINGS = 8;
const int SCR_PAUSE = 9;
const int SCR_RAMP_TIME = 10;
const int SCR_RAMP_TO = 11;
const int SCR_DONE = 12;


int currentMenu = 0;					// the currently selected menu
int settingsSel = 1;					// the currently selected settings option


//   Initialize everything

void setup() {

  //start your backlight here
  pinMode(BACK_LIGHT, OUTPUT);
  digitalWrite(BACK_LIGHT, HIGH);

  //start your motor shield here
  AFMS.begin();

  //initialize output pin for camera release here
  pinMode(13, OUTPUT);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);

  // print welcome screen))
  lcd.print("LRTimelape.com");
  lcd.setCursor(0, 1);
  lcd.print( CAPTION );

  delay(2000);							// wait a moment...

  Serial.begin(9600);
  Serial.println( CAPTION );

  lcd.clear();
}


//   The main loop
void loop() {

  if (millis() > lastKeyCheckTime + keySampleRate) {
    lastKeyCheckTime = millis();
    localKey = keypad.getKey();
    //Serial.println( localKey );
    //Serial.println( lastKeyPressed );

    if (localKey != lastKeyPressed) {
      processKey();
    } else {
      // key value has not changed, key is being held down, has it been long enough?
      // (but don't process localKey = 0 = no key pressed)
      if (localKey != 0 && millis() > lastKeyPressTime + keyRepeatRate) {
        // yes, repeat this key
        if ( (localKey == UP) || (localKey == DOWN) ) {
          processKey();
        }
      }
    }

    if ( currentMenu == SCR_RUNNING ) {
      printScreen();	// update running screen in any case
    }
  }
  if ( isRunning ) {	// release camera, do ramping if running
    running();
  }
}

/**
   Process the key presses - do the Menu Navigation
*/
void processKey() {

  lastKeyPressed = localKey;
  lastKeyPressTime = millis();

  // select key will switch backlight at any time
  if ( localKey == SELECT ) {
    if ( backLight == HIGH ) {
      backLight = LOW;
    } else {
      backLight = HIGH;
    }
    digitalWrite(BACK_LIGHT, backLight); // Turn backlight on.
  }

  // do the menu navigation
  switch ( currentMenu ) {

    case SCR_TL_MODE:

      if (localKey == UP) {
        mode--;
        if (mode < 1) {
          mode = 1;
        }
      }

      if (localKey == DOWN) {
        mode++;
        if (mode > 3) {
          mode = 3;
        }
      }

      if ((localKey == RIGHT) && mode == 1) {
        lcd.clear();
        motorSpeed[0] = 100;
        motorSpeed[1] = 100;
        motorSpeed[2] = 100;
        currentMenu = SCR_SMS;
      }

      if ((localKey == RIGHT) && mode == 2) {
        lcd.clear();
        motorSpeed[0] = 100;
        motorSpeed[1] = 100;
        motorSpeed[2] = 100;
        currentMenu = SCR_CONTINUOUS;
      }

      if ((localKey == RIGHT) && mode == 3) {
        lcd.clear();
        motorSpeed[0] = 0;
        motorSpeed[1] = 0;
        motorSpeed[2] = 0;
        currentMenu = SCR_CALIBRATE;
      }

      motorNr = 0;
      mode_i = 0;
      break;

    case SCR_SMS:
      Serial.println("SMS-Mode");
      Serial.print("i = ");
      Serial.println(mode_i);
      if ((localKey == UP) && (mode_i == 0 || mode_i == 2 || mode_i == 4)) {
        motorSpeed[motorNr] = motorSpeed[motorNr] + 10;
        Serial.print("Motorspeed M");
        Serial.print(motorNr + 1);
        Serial.print(": ");
        Serial.println(motorSpeed[motorNr]);
        if (motorSpeed[motorNr] > 250) {
          motorSpeed[motorNr] = 255;
        }
      }

      if ((localKey == DOWN) && (mode_i == 0 || mode_i == 2 || mode_i == 4)) {
        motorSpeed[motorNr] = motorSpeed[motorNr] - 10;
        Serial.print("Motorspeed M");
        Serial.print(motorNr + 1);
        Serial.print(": ");
        Serial.println(motorSpeed[motorNr]);
        if (motorSpeed[motorNr] < 10) {
          motorSpeed[motorNr] = 0;
        }
      }

      if ((localKey == UP) && (mode_i == 1 || mode_i == 3 || mode_i == 5)) {
        motorDurration[motorNr] = (float)((int)(motorDurration[motorNr] * 10) + 1) / 10; // round to 1 decimal place
        Serial.print("Motortime M");
        Serial.print(motorNr + 1);
        Serial.print(": ");
        Serial.println(motorDurration[motorNr]);
        if ( motorDurration[motorNr] > 30 ) { // no intervals longer as 99secs - those would scramble the display
          motorDurration[motorNr] = 30;
        }
      }

      if ((localKey == DOWN) && (mode_i == 1 || mode_i == 3 || mode_i == 5)) {
        Serial.print("Motortime M");
        Serial.print(motorNr + 1);
        Serial.print(": ");
        if ( motorDurration[motorNr] > 0.0) {
          motorDurration[motorNr] = (float)((int)(motorDurration[motorNr] * 10) - 1) / 10; // round to 1 decimal place
          Serial.println(motorDurration[motorNr]);
        }
      }

      if ((localKey == RIGHT) && mode_i < 6) {
        mode_i++;
        if (mode_i == 2 || mode_i == 4) {
          motorNr++;
        }
      }

      if (localKey == LEFT) {
        mode_i--;
        if (mode_i == 1 || mode_i == 3 || mode_i == 5) {
          motorNr--;
        }
        if (mode_i < 1) {
          mode_i = 0;
          lcd.clear();
          currentMenu = SCR_TL_MODE;
        }
      }

      if ((localKey == RIGHT) && mode_i == 6) {
        Serial.println("NEXT");
        lcd.clear();
        currentMenu = SCR_INTERVAL;
      }

      break;

    case SCR_CONTINUOUS:
      Serial.println("Continuous-Mode");
      Serial.print("i = ");
      Serial.println(motorNr);
      if ((localKey == UP)) {
        motorSpeed[motorNr] = motorSpeed[motorNr] + 10;
        Serial.print("Motorspeed M");
        Serial.print(motorNr + 1);
        Serial.print(": ");
        Serial.println(motorSpeed[motorNr]);
        if (motorSpeed[motorNr] > 250) {
          motorSpeed[motorNr] = 255;
        }
      }

      if ((localKey == DOWN)) {
        motorSpeed[motorNr] = motorSpeed[motorNr] - 10;
        Serial.print("Motorspeed M");
        Serial.print(motorNr + 1);
        Serial.print(": ");
        Serial.println(motorSpeed[motorNr]);
        if (motorSpeed[motorNr] < 10) {
          motorSpeed[motorNr] = 0;
        }
      }

      if ((localKey == RIGHT) && motorNr < 3) {
        motorNr++;
      }

      if (localKey == LEFT) {
        motorNr--;
        if (mode_i < 1) {
          mode_i = 0;
          lcd.clear();
          currentMenu = SCR_TL_MODE;
        }
      }

      if ((localKey == RIGHT) && motorNr >2 ) {
        Serial.println("NEXT");
        lcd.clear();
        currentMenu = SCR_INTERVAL;
      }

      break;

    case SCR_CALIBRATE:

      if (localKey == UP) {
        mode_i++;
        if (mode_i > 2) {
          mode_i = 3;
        }
        motorSpeed[motorNr] = mode_i * 80;
        Serial.print("M");
        Serial.print(motorNr + 1);
        Serial.println(" driving +...");
        Serial.print("mode_i = ");
        Serial.println(mode_i);
        Serial.print("Speed = ");
        Serial.println(motorSpeed[motorNr]);
      }

      if (localKey == DOWN) {
        mode_i--;
        if (mode_i < -2) {
          mode_i = -3;
        }
        motorSpeed[motorNr] = mode_i * 80;
        Serial.print("M");
        Serial.print(motorNr + 1);
        Serial.println(" driving -...");
        Serial.print("mode_i = ");
        Serial.println(mode_i);
        Serial.print("Speed = ");
        Serial.println(motorSpeed[motorNr]);
      }

      if ((localKey == RIGHT) && motorNr < 3) {
        motorSpeed[motorNr] = 0;
        motorNr++;
      }

      if (localKey == LEFT) {
        motorSpeed[motorNr] = 0;
        motorNr--;
        if (motorNr < 0) {
          lcd.clear();
          currentMenu = SCR_TL_MODE;
        }
      }

      if ((localKey == RIGHT) && motorNr > 2) {
        lcd.clear();
        currentMenu = SCR_TL_MODE;
      }

      break;

    case SCR_INTERVAL:

      if (localKey == UP) {
        interval = (float)((int)(interval * 10) + 1) / 10; // round to 1 decimal place
        if ( interval > 99 ) { // no intervals longer as 99secs - those would scramble the display
          interval = 99;
        }
      }

      if (localKey == DOWN) {
        if ( interval > 0.2) {
          interval = (float)((int)(interval * 10) - 1) / 10; // round to 1 decimal place
        }
      }

      if (localKey == RIGHT) {
        lcd.clear();
        rampTo = interval;			// set rampTo default to the current interval
        currentMenu = SCR_SHOOTS;
      }
      if ((localKey == LEFT) && mode == 1) {
        lcd.clear();
        currentMenu = SCR_SMS;
      }
      if ((localKey == LEFT) && mode == 2) {
        lcd.clear();
        currentMenu = SCR_CONTINUOUS;
      }
      break;

    case SCR_SHOOTS:

      if (localKey == UP) {
        if ( maxNoOfShots >= 2500 ) {
          maxNoOfShots += 100;
        } else if ( maxNoOfShots >= 1000 ) {
          maxNoOfShots += 50;
        } else if ( maxNoOfShots >= 100 ) {
          maxNoOfShots += 25;
        } else if ( maxNoOfShots >= 10 ) {
          maxNoOfShots += 10;
        } else {
          maxNoOfShots ++;
        }
        if ( maxNoOfShots >= 9999 ) { // prevents screwing the ui
          maxNoOfShots = 9999;
        }
      }

      if (localKey == DOWN) {
        if ( maxNoOfShots > 2500 ) {
          maxNoOfShots -= 100;
        } else if ( maxNoOfShots > 1000 ) {
          maxNoOfShots -= 50;
        } else if ( maxNoOfShots > 100 ) {
          maxNoOfShots -= 25;
        } else if ( maxNoOfShots > 10 ) {
          maxNoOfShots -= 10;
        } else if ( maxNoOfShots > 0) {
          maxNoOfShots -= 1;
        } else {
          maxNoOfShots = 0;
        }
      }

      if (localKey == LEFT) {
        currentMenu = SCR_INTERVAL;
        isRunning = 0;
        lcd.clear();
      }

      if (localKey == RIGHT) { // Start shooting
        currentMenu = SCR_RUNNING;
        previousMillis = millis();
        runningTime = 0;
        isRunning = 1;

        lcd.clear();

        // do the first release instantly, the subsequent ones will happen in the loop
        releaseCamera();
        if (mode == 1) {
          driveDolly();
        }
        if (mode == 2) {
          drive->setSpeed(motorSpeed[0]);
          pan->setSpeed(motorSpeed[1]);
          tilt->setSpeed(motorSpeed[2]);
          drive->run(FORWARD);
          pan->run(FORWARD);
          tilt->run(FORWARD);
        }
        imageCount++;
      }
      break;

    case SCR_RUNNING:

      if (localKey == LEFT) { // BUTTON_LEFTfrom Running Screen aborts

        if ( rampingEndTime == 0 ) { 	// if ramping not active, stop the whole shot, other otherwise only the ramping
          if ( isRunning ) { 		// if is still runing, show confirm dialog
            currentMenu = SCR_CONFIRM_END;
          } else {				// if finished, go to start screen
            currentMenu = SCR_INTERVAL;
          }
          lcd.clear();
        } else { // stop ramping
          rampingStartTime = 0;
          rampingEndTime = 0;
        }

      }

      if (localKey == RIGHT) {
        currentMenu = SCR_SETTINGS;
        lcd.clear();
      }
      break;

    case SCR_CONFIRM_END:
      if (localKey == LEFT) { // Really abort
        currentMenu = SCR_TL_MODE;
        drive->run(RELEASE);
        pan->run(RELEASE);
        tilt->run(RELEASE);
        isRunning = 0;
        imageCount = 0;
        runningTime = 0;
        lcd.clear();
      }
      if (localKey == RIGHT) { // resume
        currentMenu = SCR_RUNNING;
        lcd.clear();
      }
      break;

    case SCR_SETTINGS:

      if (localKey == DOWN && settingsSel == 1 ) {
        settingsSel = 2;
      }

      if (localKey == UP && settingsSel == 2 ) {
        settingsSel = 1;
      }

      if (localKey == LEFT) {
        settingsSel = 1;
        currentMenu = SCR_RUNNING;
        lcd.clear();
      }

      if (localKey == RIGHT && settingsSel == 1 ) {
        isRunning = 0;
        currentMenu = SCR_PAUSE;
        lcd.clear();
      }

      if (localKey == RIGHT && settingsSel == 2 ) {
        currentMenu = SCR_RAMP_TIME;
        lcd.clear();
      }
      break;

    case SCR_PAUSE:

      if (localKey == LEFT) {
        currentMenu = SCR_RUNNING;
        isRunning = 1;
        previousMillis = millis() - (imageCount * 1000); // prevent counting the paused time as running time;
        lcd.clear();
      }
      break;

    case SCR_RAMP_TIME:

      if (localKey == RIGHT) {
        currentMenu = SCR_RAMP_TO;
        lcd.clear();
      }

      if (localKey == LEFT) {
        currentMenu = SCR_SETTINGS;
        settingsSel = 2;
        lcd.clear();
      }

      if (localKey == UP) {
        if ( rampDuration >= 10) {
          rampDuration += 10;
        } else {
          rampDuration += 1;
        }
      }

      if (localKey == DOWN) {
        if ( rampDuration > 10 ) {
          rampDuration -= 10;
        } else {
          rampDuration -= 1;
        }
        if ( rampDuration <= 1 ) {
          rampDuration = 1;
        }
      }
      break;

    case SCR_RAMP_TO:

      if (localKey == LEFT) {
        currentMenu = SCR_RAMP_TIME;
        lcd.clear();
      }

      if (localKey == UP) {
        rampTo = (float)((int)(rampTo * 10) + 1) / 10; // round to 1 decimal place
        if ( rampTo > 99 ) { // no intervals longer as 99secs - those would scramble the display
          rampTo = 99;
        }
      }

      if (localKey == DOWN) {
        if ( rampTo > 0.2) {
          rampTo = (float)((int)(rampTo * 10) - 1) / 10; // round to 1 decimal place
        }
      }

      if (localKey == RIGHT) { // start Interval ramping
        if ( rampTo != interval ) { // only if a different Ramping To interval has been set!
          intervalBeforeRamping = interval;
          rampingStartTime = millis();
          rampingEndTime = rampingStartTime + rampDuration * 60 * 1000;
        }

        // go back to main screen
        currentMenu = SCR_RUNNING;
        lcd.clear();
      }
      break;

    case SCR_DONE:

      if (localKey == LEFT||localKey == RIGHT) {
        currentMenu = SCR_TL_MODE;
        isRunning = 0;
        imageCount = 0;
        runningTime = 0;
        lcd.clear();
      }
      break;
  }
  printScreen();

}

void printScreen() {

    switch ( currentMenu ) {

    case SCR_TL_MODE:
      printTLMenu();
      break;

    case SCR_SMS:
      printSMSMenu();
      break;

    case SCR_CONTINUOUS:
      printContinuousMenu();
      break;

    case SCR_CALIBRATE:
      printCalibrateMenu();
      break;

    case SCR_INTERVAL:
      printIntervalMenu();
      break;

    case SCR_SHOOTS:
      printNoOfShotsMenu();
      break;

    case SCR_RUNNING:
      printRunningScreen();
      break;

    case SCR_CONFIRM_END:
      printConfirmEndScreen();
      break;

    case SCR_SETTINGS:
      printSettingsMenu();
      break;

    case SCR_PAUSE:
      printPauseMenu();
      break;

    case SCR_RAMP_TIME:
      printRampDurationMenu();
      break;

    case SCR_RAMP_TO:
      printRampToMenu();
      break;

    case SCR_DONE:
      printDoneScreen();
      break;
  }
}


/**
   Running, releasing Camera
*/
void running() {

  // do this every interval only
  if ( ( millis() - previousMillis ) >=  ( ( interval * 1000 )) ) {

    if ( ( maxNoOfShots != 0 ) && ( imageCount >= maxNoOfShots ) ) { // sequence is finished
      // stop shooting
      isRunning = 0;
      currentMenu = SCR_DONE;
      lcd.clear();
      printDoneScreen(); // invoke manually
    } else { // is running
      runningTime += (millis() - previousMillis );
      previousMillis = millis();
      releaseCamera();
      if (mode == 1) {
        driveDolly();
      }
      imageCount++;
    }
  }

  // do this always (multiple times per interval)
  possiblyRampInterval();
}

/**
   If ramping was enabled do the ramping
*/
void possiblyRampInterval() {

  if ( ( millis() < rampingEndTime ) && ( millis() >= rampingStartTime ) ) {
    interval = intervalBeforeRamping + ( (float)( millis() - rampingStartTime ) / (float)( rampingEndTime - rampingStartTime ) * ( rampTo - intervalBeforeRamping ) );
  } else {
    rampingStartTime = 0;
    rampingEndTime = 0;
  }
}

/**
   Actually release the camera
*/
void releaseCamera() {

  lcd.setCursor(15, 0);
  lcd.print((char)255);

  trigger->run(FORWARD);
  delay(RELEASE_TIME);
  trigger->run(RELEASE);

  lcd.setCursor(15, 0);
  lcd.print(" ");
}

/**
   Drive the Dolly
*/
void driveDolly() {

  lcd.setCursor(15, 0);
  lcd.print((char)255);

  drive->setSpeed(motorSpeed[0]);
  drive->run(FORWARD);
  delay((int)(motorDurration[0]*1000));
  drive->run(RELEASE);

  pan->setSpeed(motorSpeed[1]);
  pan->run(FORWARD);
  delay((int)(motorDurration[1]*1000));
  pan->run(RELEASE);

  tilt->setSpeed(motorSpeed[2]);
  tilt->run(FORWARD);
  delay((int)(motorDurration[2]*1000));
  tilt->run(RELEASE);

  lcd.setCursor(15, 0);
  lcd.print(" ");
}

/**
   Pause Mode
*/
void printPauseMenu() {
  drive->run(RELEASE);
  pan->run(RELEASE);
  tilt->run(RELEASE);
  lcd.setCursor(0, 0);
  lcd.print("PAUSE...        ");
  lcd.setCursor(0, 1);
  lcd.print("< Continue");
}

// ---------------------- SCREENS AND MENUS -------------------------------

/**
   Configure TL setting (main screen)
*/
void printTLMenu() {
  drive->run(RELEASE);
  pan->run(RELEASE);
  tilt->run(RELEASE);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Timelapse-Mode");
  lcd.setCursor(0, 1);
  if (mode == 1) {
    lcd.print("      SMS");
  }
  if (mode == 2) {
    lcd.print("   Continuous");
  }
  if (mode == 3) {
    lcd.print("  Calibration");
  }
}

/**
   Configure SMS setting (main screen)
*/
void printSMSMenu() {
  lcd.setCursor(0, 0);
  lcd.print("SMS-Mode");
  lcd.setCursor(0, 1);
  if (mode_i == 0) {
    lcd.print("V(M1):          ");
    lcd.setCursor(14,1);
    if (motorSpeed[0] > 99) {
      lcd.setCursor(13, 1);
    }
    lcd.print( motorSpeed[0] );
  }
  if (mode_i == 1) {
    lcd.print("t(M1):         ");
    lcd.setCursor(12,1);
    lcd.print( motorDurration[0] );
  }
  if (mode_i == 2) {
    lcd.print("V(M2):          ");
    lcd.setCursor(14,1);
    if (motorSpeed[1] > 99) {
      lcd.setCursor(13, 1);
    }
    lcd.print( motorSpeed[1] );
  }
  if (mode_i == 3) {
    lcd.print("t(M2):          ");
    lcd.setCursor(12,1);
    lcd.print( motorDurration[1] );
  }
  if (mode_i == 4) {
    lcd.print("V(M3):          ");
    lcd.setCursor(14,1);
    if (motorSpeed[2] > 99) {
      lcd.setCursor(13, 1);
    }
    lcd.print( motorSpeed[2] );
  }
  if (mode_i == 5) {
    lcd.print("t(M3):          ");
    lcd.setCursor(12,1);
    lcd.print( motorDurration[2] );
  }
}

/**
   Configure Continuous setting (main screen)
*/
void printContinuousMenu() {
  lcd.setCursor(0, 0);
  lcd.print("Continuous-Mode");
  lcd.setCursor(0, 1);
  if (motorNr == 0) {
    lcd.print("V(M1):          ");
    lcd.setCursor(14,1);
    if (motorSpeed[0] > 99) {
      lcd.setCursor(13, 1);
    }
    lcd.print( motorSpeed[0] );
  }
  if (motorNr == 1) {
    lcd.print("V(M2):          ");
    lcd.setCursor(14,1);
    if (motorSpeed[1] > 99) {
      lcd.setCursor(13, 1);
    }
    lcd.print( motorSpeed[1] );
  }
  if (motorNr == 2) {
    lcd.print("V(M3):          ");
    lcd.setCursor(14,1);
    if (motorSpeed[2] > 99) {
      lcd.setCursor(13, 1);
    }
    lcd.print( motorSpeed[2] );
  }
}

/**
   Configure Continuous setting (main screen)
*/
void printCalibrateMenu() {
  lcd.setCursor(0, 0);
  lcd.print("Calibration-Menu");
  lcd.setCursor(0, 1);
  lcd.print("M");
  lcd.print(motorNr+1);
  if (motorNr == 0 && mode_i > 0) {
    drive->setSpeed(motorSpeed[0]);
    drive->run(FORWARD);
  }
  if (motorNr == 0 && mode_i < 0) {
    drive->setSpeed(-motorSpeed[0]);
    drive->run(BACKWARD);
  }
  if (motorNr == 1 && mode_i > 0) {
    pan->setSpeed(motorSpeed[1]);
    pan->run(FORWARD);
  }
  if (motorNr == 1 && mode_i < 0) {
    pan->setSpeed(-motorSpeed[1]);
    pan->run(BACKWARD);
  }
  if (motorNr == 2 && mode_i > 0) {
    tilt->setSpeed(motorSpeed[2]);
    tilt->run(FORWARD);
  }
  if (motorNr == 2 && mode_i < 0) {
    tilt->setSpeed(-motorSpeed[2]);
    tilt->run(BACKWARD);
  }
  if (mode_i == 0) {
    drive->run(RELEASE);
    pan->run(RELEASE);
    tilt->run(RELEASE);
    lcd.setCursor(3, 1);
    lcd.print("selected     ");
  }
  if (mode_i != 0) {
    lcd.setCursor(3, 1);
    lcd.print("running ");
    if (mode_i == 1) {
      lcd.print("+  ");
    }
    if (mode_i == 2) {
      lcd.print("++ ");
    }
    if (mode_i == 3) {
      lcd.print("+++");
    }
    if (mode_i == -1) {
      lcd.print("-  ");
    }
    if (mode_i == -2) {
      lcd.print("-- ");
    }
    if (mode_i == -3) {
      lcd.print("---");
    }
  }
}

/**
   Configure Interval setting (main screen)
*/
void printIntervalMenu() {

  lcd.setCursor(0, 0);
  lcd.print("Interval");
  lcd.setCursor(0, 1);
  lcd.print( interval );
  lcd.print( "     " );
}

/**
   Configure no of shots - 0 means infinity
*/
void printNoOfShotsMenu() {

  lcd.setCursor(0, 0);
  lcd.print("No of shots");
  lcd.setCursor(0, 1);
  if ( maxNoOfShots > 0 ) {
    lcd.print( printInt( maxNoOfShots, 4 ) );
  } else {
    lcd.print( "----" );
  }
}

/**
   Print running screen
*/
void printRunningScreen() {

  lcd.setCursor(0, 0);
  lcd.print( printInt( imageCount, 4 ) );

  if ( maxNoOfShots > 0 ) {
    lcd.print( " R:" );
    lcd.print( printInt( maxNoOfShots - imageCount, 4 ) );
    lcd.print( " " );

    lcd.setCursor(0, 1);
    // print remaining time
    unsigned long remainingSecs = (maxNoOfShots - imageCount) * interval;
    lcd.print( "T-");
    lcd.print( fillZero( remainingSecs / 60 / 60 ) );
    lcd.print( ":" );
    lcd.print( fillZero( ( remainingSecs / 60 ) % 60 ) );
  }

  updateTime();

  lcd.setCursor(11, 0);
  if ( millis() < rampingEndTime ) {
    lcd.print( "*" );
  } else {
    lcd.print( " " );
  }
  lcd.print( printFloat( interval, 4, 1 ) );
}

void printDoneScreen() {

  drive->run(RELEASE);
  pan->run(RELEASE);
  tilt->run(RELEASE);
  // print elapsed image count
  lcd.setCursor(0, 0);
  lcd.print("Done ");
  lcd.print( imageCount );
  lcd.print( " shots.");

  // print elapsed time when done
  lcd.setCursor(0, 1);
  lcd.print( "t=");
  lcd.print( fillZero( runningTime / 1000 / 60 / 60 ) );
  lcd.print( ":" );
  lcd.print( fillZero( ( runningTime / 1000 / 60 ) % 60 ) );
  lcd.print("    ;-)");
}

void printConfirmEndScreen() {
  lcd.setCursor(0, 0);
  lcd.print( "Stop shooting?");
  lcd.setCursor(0, 1);
  lcd.print( "< Stop    Cont.>");
}

/**
   Update the time display in the main screen
*/
void updateTime() {

  unsigned long finerRunningTime = runningTime + (millis() - previousMillis);

  if ( isRunning ) {

    int hours = finerRunningTime / 1000 / 60 / 60;
    int minutes = (finerRunningTime / 1000 / 60) % 60;
    int secs = (finerRunningTime / 1000 ) % 60;

    String sHours = fillZero( hours );
    String sMinutes = fillZero( minutes );
    String sSecs = fillZero( secs );

    lcd.setCursor(8, 1);
    lcd.print( sHours );
    lcd.setCursor(10, 1);
    lcd.print(":");
    lcd.setCursor(11, 1);
    lcd.print( sMinutes );
    lcd.setCursor(13, 1);
    lcd.print(":");
    lcd.setCursor(14, 1);
    lcd.print( sSecs );
  } else {
    lcd.setCursor(8, 1);
    lcd.print("   Done!");
  }
}

/**
   Print Settings Menu
*/
void printSettingsMenu() {

  lcd.setCursor(1, 0);
  lcd.print("Pause          ");
  lcd.setCursor(1, 1);
  lcd.print("Ramp Interval  ");
  lcd.setCursor(0, settingsSel - 1);
  lcd.print(">");
  lcd.setCursor(0, 1 - (settingsSel - 1));
  lcd.print(" ");
}

/**
   Print Ramping Duration Menu
*/
void printRampDurationMenu() {

  lcd.setCursor(0, 0);
  lcd.print("Ramp Time (min) ");

  lcd.setCursor(0, 1);
  lcd.print( rampDuration );
  lcd.print( "     " );
}

/**
   Print Ramping To Menu
*/
void printRampToMenu() {

  lcd.setCursor(0, 0);
  lcd.print("Ramp to (Intvl)");

  lcd.setCursor(0, 1);
  lcd.print( rampTo );
  lcd.print( "     " );
}




// ----------- HELPER METHODS -------------------------------------

/**
   Fill in leading zero to numbers in order to always have 2 digits
*/
String fillZero( int input ) {

  String sInput = String( input );
  if ( sInput.length() < 2 ) {
    sInput = "0";
    sInput.concat( String( input ));
  }
  return sInput;
}

String printFloat(float f, int total, int dec) {

  static char dtostrfbuffer[8];
  String s = dtostrf(f, total, dec, dtostrfbuffer);
  return s;
}

String printInt( int i, int total) {
  float f = i;
  static char dtostrfbuffer[8];
  String s = dtostrf(f, total, 0, dtostrfbuffer);
  return s;
}
