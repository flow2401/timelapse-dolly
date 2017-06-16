/*
  based on
  Pro-Timer Free
  Gunther Wegner
  http://gwegner.de
  http://lrtimelapse.com
*/

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include "LCD_Keypad_Reader.h"			// credits to: http://www.hellonull.com/?p=282

//motor shield library
#include "Adafruit_MotorShield.h"
#include "utility/Adafruit_MS_PWMServoDriver.h"
//when using a different motor shield, add // to above and add yours here

//initialize your keypad here
LCD_Keypad_Reader keypad;
LiquidCrystal lcd(8, 9, 7, 6, 5, 4);	//Pin assignments for SainSmart LCD Keypad Shield

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

const String CAPTION = "Pro-MoCo 0.92";

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

}
