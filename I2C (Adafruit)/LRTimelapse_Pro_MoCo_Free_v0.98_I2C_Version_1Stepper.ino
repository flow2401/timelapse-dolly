/*
  based on
  Pro-Timer Free
  Gunther Wegner
  http://gwegner.de
  http://lrtimelapse.com
*/

#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_RGBLCDShield.h"
#include "utility/Adafruit_MCP23017.h"
#include <Adafruit_MotorShield.h>
#include "utility/Adafruit_MS_PWMServoDriver.h"

Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

Adafruit_MotorShield AFMS;

Adafruit_StepperMotor *drive = AFMS.getStepper(200,1);
Adafruit_DCMotor *trigger = AFMS.getMotor(4);

// The shield uses the I2C SCL and SDA pins. On classic Arduinos
// this is Analog 4 and 5 so you can't use those for analogRead() anymore
// However, you can connect other I2C sensors to the I2C bus and share
// the I2C bus.

const String CAPTION = "Pro-MoCo 0.98";

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
int motorSpeed = 400;         // max 600
int motorSteps = 10;      // max 65535
int motorDirection = 1;
int mode_i = 0;

int sliderStart = 0;
int sliderEnd = 0;
int sliderLength = 1000; // unit = steps
int sliderRemaining = 1000;
int sliderMode = 1; // 1 = stop at the end of the slider, 2 = change direction

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
const int SCR_M1 = 4;
const int SCR_M2 = 5;
const int SCR_M3 = 6;
const int SCR_INTERVAL = 7;				// menu workflow constants
const int SCR_SHOOTS = 8;
const int SCR_RUNNING = 9;
const int SCR_CONFIRM_END = 10;
const int SCR_SETTINGS = 11;
const int SCR_PAUSE = 12;
const int SCR_RAMP_TIME = 13;
const int SCR_RAMP_TO = 14;
const int SCR_DONE = 15;


int currentMenu = 0;					// the currently selected menu
int settingsSel = 1;					// the currently selected settings option


//   Initialize everything

void setup() {
  
  lcd.setBacklight(0x1);		// Turn backlight on.
  AFMS.begin();
  trigger->setSpeed(255);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);

  // print welcome screen))
  lcd.print("LRTimelapse.com");
  lcd.setCursor(0, 1);
  lcd.print( CAPTION );

  delay(2000);							// wait a moment...

  Serial.begin(9600);
  Serial.println( CAPTION );

  lcd.clear();
}


//   The main loop
uint8_t i=0;

void loop() {
  uint8_t buttons = lcd.readButtons();

  if (millis() > lastKeyCheckTime + keySampleRate) {
    lastKeyCheckTime = millis();
    localKey = buttons;
    //Serial.println( localKey );
    //Serial.println( lastKeyPressed );

    if (localKey != lastKeyPressed) {
      processKey();
    } else {
      // key value has not changed, key is being held BUTTON_DOWN, has it been long enough?
      // (but don't process localKey = 0 = no key pressed)
      if (localKey != 0 && millis() > lastKeyPressTime + keyRepeatRate) {
        // yes, repeat this key
        if ( (localKey & BUTTON_UP) || (localKey & BUTTON_DOWN) ) {
          processKey();
        }
      }
    }

    if ( currentMenu == SCR_RUNNING ) {
      printScreen();	// BUTTON_UPdate running screen in any case
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
  uint8_t buttons = lcd.readButtons();

  lastKeyPressed = localKey;
  lastKeyPressTime = millis();

  if(buttons) {

  // select key will switch backlight at any time
  if (localKey & BUTTON_SELECT) {
    if ( backLight == HIGH ) {
      backLight = LOW;
      lcd.setBacklight(0x0); // Turn backlight off.
    } else {
      backLight = HIGH;
      lcd.setBacklight(0x1); // Turn backlight on.
    }
  }

  // do the menu navigation
  switch ( currentMenu ) {

    case SCR_TL_MODE:

      if (localKey & BUTTON_UP) {
        mode--;
        if (mode < 1) {
          mode = 0;
        }
      }

      if (localKey & BUTTON_DOWN) {
        mode++;
        if (mode > 2) {
          mode = 3;
        }
      }
      
      if ((localKey & BUTTON_RIGHT) && mode == 1) {
        sliderRemaining = sliderLength;
        lcd.clear();
        currentMenu = SCR_SMS;
      }

      if ((localKey & BUTTON_RIGHT) && mode == 2) {
        lcd.clear();
        currentMenu = SCR_CONTINUOUS;
      }

      if ((localKey & BUTTON_RIGHT) && mode == 3) {
        sliderLength = 0;
        lcd.clear();
        currentMenu = SCR_CALIBRATE;
      }

      mode_i = 0;
      break;

      case SCR_SMS:
        Serial.println("SMS-Mode");
        Serial.print("i = ");
        Serial.println(mode_i);
        if ((localKey & BUTTON_UP) && (mode_i == 0)) {
          motorSpeed += 50;
          Serial.print("Motorspeed: ");
          Serial.println(motorSpeed);
          motorDirection = 1;
          if (motorSpeed > 550) {
            motorSpeed = 600;
          }
        }
  
        if ((localKey & BUTTON_DOWN) && (mode_i == 0)) {
          motorSpeed -= 50;
          Serial.print("Motorspeed: ");
          Serial.println(motorSpeed);
          if (motorSpeed < 0) {
            motorDirection = -1;
            if (motorSpeed < -550) {
              motorSpeed = -600;
            }
          }
        }
  
        if ((localKey & BUTTON_UP) && (mode_i == 1)) {
          motorSteps += 1;
          if (motorSteps > sliderLength) {
              motorSteps = sliderLength;
          }
          Serial.print("Steps: ");
          Serial.print(motorSteps);
        }
  
        if ((localKey & BUTTON_DOWN) && (mode_i == 1)) {
          motorSteps -= 1;
          if (motorSteps < 1) {
              motorSteps = 0;
          }
          Serial.print("Steps: ");
        }
  
        //if ((motorSteps * maxNoOfShots > sliderLength) && (localKey & BUTTON_RIGHT)) {

        if ((localKey & BUTTON_UP) && (mode_i == 2)) {
          sliderMode -= 1;
          if (sliderMode < 1) {
              sliderMode = 1;
          }
          Serial.print("SliderMode: ");
          Serial.print(sliderMode);
        }
  
        if ((localKey & BUTTON_DOWN) && (mode_i == 2)) {
          sliderMode += 1;
          if (sliderMode > 2) {
              sliderMode = 2;
          }
          Serial.print("sliderMode: ");
        }
        
        if ((localKey & BUTTON_RIGHT)) {  
          mode_i++;
          if (mode_i > 2) {
            Serial.println("NEXT");
            lcd.clear();
            currentMenu = SCR_INTERVAL;
          }
        }
  
        if (localKey & BUTTON_LEFT) {
          mode_i--;
          if (mode_i < 1) {
            mode_i = 0;
            lcd.clear();
            currentMenu = SCR_TL_MODE;
          }
        }
  
        break;

      case SCR_CONTINUOUS:
      Serial.println("Continuous-Mode");
      if ((localKey & BUTTON_UP)) {
        motorSpeed += 50;
        Serial.print("Motorspeed: ");
        Serial.println(motorSpeed);
        motorDirection = 1;
        if (motorSpeed > 550) {
          motorSpeed = 600;
        }
      }

      if ((localKey & BUTTON_DOWN)) {
        motorSpeed -= 50;
        Serial.print("Motorspeed: ");
        Serial.println(motorSpeed);
        if (motorSpeed < 0) {
          motorDirection = -1;
          if (motorSpeed < -550) {
            motorSpeed = -600;
          }
        }
      }

      if (localKey & BUTTON_LEFT) {
        lcd.clear();
        currentMenu = SCR_TL_MODE;
      }

      if ((localKey & BUTTON_RIGHT)) {
        mode_i++;
        Serial.println("NEXT");
        lcd.clear();
        currentMenu = SCR_INTERVAL;
      }
   
      break;

    case SCR_CALIBRATE:

      if ((localKey & BUTTON_UP) && (mode_i == 0)) {
        drive->step(5, FORWARD, MICROSTEP);
        sliderLength -= 5;
        Serial.println("Motor driving +...");
        Serial.print("mode_i = ");
        Serial.println(mode_i);
        Serial.print("Speed = ");
        Serial.println(motorSpeed);
      }

      if ((localKey & BUTTON_DOWN) && (mode_i == 0)) {
        drive->step(5, BACKWARD, MICROSTEP);
        sliderLength += 5;
        Serial.println("Motor driving -...");
        Serial.print("mode_i = ");
        Serial.println(mode_i);
        Serial.print("Speed = ");
        Serial.println(motorSpeed);
      }

      if ((localKey & BUTTON_UP) && (mode_i == 1)) {
        drive->step(5, FORWARD, MICROSTEP);
        sliderLength += 5;
        Serial.println("Motor driving +...");
        Serial.print("mode_i = ");
        Serial.println(mode_i);
        Serial.print("Speed = ");
        Serial.println(motorSpeed);
      }

      if ((localKey & BUTTON_DOWN) && (mode_i == 1)) {
        drive->step(5, BACKWARD, MICROSTEP);
        sliderLength -= 5;
        Serial.println("Motor driving -...");
        Serial.print("mode_i = ");
        Serial.println(mode_i);
        Serial.print("Speed = ");
        Serial.println(motorSpeed);
      }

      if (localKey & BUTTON_LEFT) {
        mode_i--;
        if (mode_i < 1) {
          lcd.clear();
          currentMenu = SCR_TL_MODE;
        }
      }
      
      if ((localKey & BUTTON_RIGHT)) {
        mode_i++ ;
      }

      if (mode_i == 2) {
        if (sliderLength < 1) {
          drive->step(-sliderLength, FORWARD, MICROSTEP);
          mode_i++;
        }
        
        if (sliderLength > 0) {
          drive->step(sliderLength, BACKWARD, MICROSTEP);
          mode_i++;
        }
      }
      
      if (mode_i > 2) {
        if (sliderLength < 1) {
          sliderLength = -sliderLength;
        }
        sliderRemaining = sliderLength;
        lcd.clear();
        currentMenu = SCR_TL_MODE;
      }

      break;

    case SCR_INTERVAL:

      if (localKey & BUTTON_UP) {
        interval = (float)((int)(interval * 10) + 1) / 10; // round to 1 decimal place
        if ( interval > 99 ) { // no intervals longer as 99secs - those would scramble the display
          interval = 99;
        }
      }

      if (localKey & BUTTON_DOWN) {
        if ( interval > 0.2) {
          interval = (float)((int)(interval * 10) - 1) / 10; // round to 1 decimal place
        }
      }

      if (localKey & BUTTON_RIGHT) {
        lcd.clear();
        rampTo = interval;      // set rampTo default to the current interval
        currentMenu = SCR_SHOOTS;
      }
      if ((localKey & BUTTON_LEFT) && mode == 1) {
        lcd.clear();
        currentMenu = SCR_SMS;
      }
      if ((localKey & BUTTON_LEFT) && mode == 2) {
        lcd.clear();
        currentMenu = SCR_CONTINUOUS;
      }
      break;

    case SCR_SHOOTS:

      if (localKey & BUTTON_UP) {
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

      if (localKey & BUTTON_DOWN) {
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

      if (localKey & BUTTON_LEFT) {
        currentMenu = SCR_INTERVAL;
        isRunning = 0;
        lcd.clear();
      }

      if (localKey & BUTTON_RIGHT) { // Start shooting
        currentMenu = SCR_RUNNING;
        previousMillis = millis();
        runningTime = 0;
        isRunning = 1;

        lcd.clear();

        // do the first release instantly, the subsequent ones will happen in the loop
        if ((mode == 0 )) {
          releaseCamera();
          driveDolly();
          imageCount++;
        }

        if ((mode == 1 )) {
          driveDolly();
          releaseCamera();
          imageCount++;
        }

        if (mode == 2) {
          driveDolly();
        }
        
        if (mode == 3) {
          motorSteps = (int)interval*15;
        }
      }
      break;

    case SCR_RUNNING:

      if (localKey & BUTTON_LEFT) { // BUTTON_LEFTfrom Running Screen aborts

        if ( rampingEndTime == 0 ) {  // if ramping not active, stop the whole shot, other otherwise only the ramping
          if ( isRunning ) {    // if is still runing, show confirm dialog
            currentMenu = SCR_CONFIRM_END;
          } else {        // if finished, go to start screen
            currentMenu = SCR_INTERVAL;
          }
          lcd.clear();
        } else { // stop ramping
          rampingStartTime = 0;
          rampingEndTime = 0;
        }

      }

      if (localKey & BUTTON_RIGHT) {
        currentMenu = SCR_SETTINGS;
        lcd.clear();
      }
      break;

    case SCR_CONFIRM_END:
      if (localKey & BUTTON_LEFT) { // Really abort
        currentMenu = SCR_TL_MODE;
        drive->step(0, FORWARD, MICROSTEP);
        isRunning = 0;
        imageCount = 0;
        runningTime = 0;
        lcd.clear();
      }
      if (localKey & BUTTON_RIGHT) { // resume
        currentMenu = SCR_RUNNING;
        lcd.clear();
      }
      break;

    case SCR_SETTINGS:

      if (localKey & BUTTON_DOWN && settingsSel == 1 ) {
        settingsSel = 2;
      }

      if (localKey & BUTTON_UP && settingsSel == 2 ) {
        settingsSel = 1;
      }

      if (localKey & BUTTON_LEFT) {
        settingsSel = 1;
        currentMenu = SCR_RUNNING;
        lcd.clear();
      }

      if (localKey & BUTTON_RIGHT && settingsSel == 1 ) {
        isRunning = 0;
        currentMenu = SCR_PAUSE;
        lcd.clear();
      }

      if (localKey & BUTTON_RIGHT && settingsSel == 2 ) {
        currentMenu = SCR_RAMP_TIME;
        lcd.clear();
      }
      break;

    case SCR_PAUSE:

      if (localKey & BUTTON_LEFT) {
        currentMenu = SCR_RUNNING;
        isRunning = 1;
        previousMillis = millis() - (imageCount * 1000); // prevent counting the paused time as running time;
        lcd.clear();
      }
      break;

    case SCR_RAMP_TIME:

      if (localKey & BUTTON_RIGHT) {
        currentMenu = SCR_RAMP_TO;
        lcd.clear();
      }

      if (localKey & BUTTON_LEFT) {
        currentMenu = SCR_SETTINGS;
        settingsSel = 2;
        lcd.clear();
      }

      if (localKey & BUTTON_UP) {
        if ( rampDuration >= 10) {
          rampDuration += 10;
        } else {
          rampDuration += 1;
        }
      }

      if (localKey & BUTTON_DOWN) {
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

      if (localKey & BUTTON_LEFT) {
        currentMenu = SCR_RAMP_TIME;
        lcd.clear();
      }

      if (localKey & BUTTON_UP) {
        rampTo = (float)((int)(rampTo * 10) + 1) / 10; // round to 1 decimal place
        if ( rampTo > 99 ) { // no intervals longer as 99secs - those would scramble the display
          rampTo = 99;
        }
      }

      if (localKey & BUTTON_DOWN) {
        if ( rampTo > 0.2) {
          rampTo = (float)((int)(rampTo * 10) - 1) / 10; // round to 1 decimal place
        }
      }

      if (localKey & BUTTON_RIGHT) { // start Interval ramping
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

      if (localKey & BUTTON_LEFT||localKey & BUTTON_RIGHT) {
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
      driveDolly();
      runningTime += (millis() - previousMillis );
      previousMillis = millis();
      releaseCamera();
      
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

  Serial.println(sliderRemaining);
  Serial.println(sliderLength);

  if (((sliderRemaining >= motorSteps)) && (motorSpeed < 0)) {
    drive->setSpeed(-motorSpeed);
    drive->step(motorSteps, FORWARD, MICROSTEP);
    sliderRemaining -= motorSteps;
  }

  if (((sliderRemaining >= motorSteps)) && (motorSpeed > 0)) {
    drive->setSpeed(motorSpeed);
    drive->step(motorSteps, BACKWARD, MICROSTEP);
    sliderRemaining -= motorSteps;
  }
  
  if ((sliderRemaining < motorSteps) && (sliderMode == 1)) {
    isRunning = 0;
    lcd.clear();
    currentMenu = SCR_DONE;
    //lcd.clear();
  }
    
  if ((sliderRemaining < motorSteps) && (sliderMode == 2)) {
    motorSpeed = -motorSpeed;
    sliderRemaining = sliderLength - sliderRemaining;
  }

  lcd.setCursor(15, 0);
  lcd.print(" ");
}

/**
   Pause Mode
*/
void printPauseMenu() {
  drive->step(0, FORWARD, MICROSTEP);
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
    lcd.print("Speed:          ");
    lcd.setCursor(14,1);
    if (motorSpeed > 99) {
      lcd.setCursor(13, 1);
    }
    if (motorSpeed < -9) {
      lcd.setCursor(13, 1);
    }
    if (motorSpeed < -99) {
      lcd.setCursor(12, 1);
    }
    lcd.print( motorSpeed );
  }
  
  if (mode_i == 1) {
    lcd.print("Steps:          ");
    lcd.setCursor(14,1);
    lcd.print( motorSteps );
    if (motorSteps > 99) {
      lcd.setCursor(13, 1);
    }
  }

  if (mode_i == 2) {
    lcd.print("Slider-Mode:    ");
    lcd.setCursor(13,1);
    if (sliderMode == 1) {
      lcd.print("->|");
    }
    if (sliderMode == 2) {
      lcd.print("<->");
    }
  }
}

/**
   Configure Continuous setting (main screen)
*/
void printContinuousMenu() {
  lcd.setCursor(0, 0);
  lcd.print("Continuous-Mode");
  lcd.setCursor(0, 1);
  lcd.print("Speed:          ");
  lcd.setCursor(14,1);
  if (motorSpeed > 99) {
    lcd.setCursor(13, 1);
  }
  lcd.print( motorSpeed );
}

/**
   Configure Continuous setting (main screen)
*/
void printCalibrateMenu() {
  lcd.setCursor(0, 0);
  if (mode_i == 0) {
    lcd.print("Startpoint      ");
  }
  
  if (mode_i == 1) {
    lcd.print("Endpoint        ");
  }
  
  lcd.setCursor(0, 1);
  lcd.print("v   drive   ^");

  if (mode_i == 2) {
    lcd.clear();
    lcd.print("Driving home... ");
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

  motorSteps = 0;
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
   BUTTON_UPdate the time display in the main screen
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
