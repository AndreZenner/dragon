/*
 * Serial - Drag:on Servo Control Software [normal & standalone]
 * 
 * Author: André Zenner
 * Date: January 2020
 * 
 * Libraries used:
 *    * ArduinoSerialCommand by Steven Cogswell - GNU LGPL 2.1 (https://github.com/scogswell/ArduinoSerialCommand)
 *    [* esp8266/Arduino - GNU LGPL 2.1 (https://github.com/esp8266/Arduino)] - only if using ESP8266 (Wemos)
 *    [* espsoftwareserial - (https://github.com/plerup/espsoftwareserial)] - only if using ESP8266 (Wemos)
 * 
 * LISTENS FOR COMMANDS VIA SERIAL
 * SEND ASCII TO THIS UNIT. Example:
 * "FANS 30 100"  --> Opens FAN A 30% and FAN B 100%
 * "FANS X 40"    --> Opens FAN B 40% and leaves FAN A as is
 * "STATE"        --> Returns current relative position of fans (in percent) (FAN-A 30 FAN-B 100)
 * 
 * SENDS ASCII WHEN BUTTON PRESSED/RELEASED
 * "BUTTON PRESSED"
 * "BUTTON RELEASED"
 * 
 * BALANCED VERSION - FAN B LIMITED DUE TO HAND/ARM RESTRICTIONS
 * 
 * COMBINED NORMAL & STANDALONE VERSION:
 *    --> TO START IN STANDALONE MODE: KEEP BUTTON PRESSED WHEN BOOTING ARDUINO --> GREEN LIGHT FLASHING 10X
 *    --> TO START IN NORMAL MODE: DO NOT PRESS BUTTON WHEN BOOTING
 * */

//include libs
#include <SoftwareSerial.h>
#include <SerialCommand.h>
#include <Servo.h>

//definations
#define VERSION 13           //the current version
#define SERVO_PIN_1 5      //labeled D5, see https://wiki.wemos.cc/products:d1:d1_mini#technical_specs
#define SERVO_PIN_2 6      //labeled D6
#define BUTTON_PIN 11       //labeled D11
#define BUILTIN_LED 13      //nano LED at 13

//servo parameters
Servo servoA;  // create servo object to control a servo
Servo servoB;
int degMax = 125;    //ATTENTION: 130 deg == FAN OPEN -- 0 deg = FAN CLOSED
int degMin = 0;
float limitationB = 0.86f;
int posA, posB;    // variable to store the servo position

//button
bool curPressed = false;

//serial communication
SerialCommand scmd;

//STANDALONE MODE
bool isStandalone = false;
int currentStateIndex = 0;
int numberStates = 5;
bool switchState = false;

void setup()
{
  //prepare pins
  Serial.begin(115200);
  while(!Serial);
  randomSeed(analogRead(1));      //random init
  pinMode(BUILTIN_LED, OUTPUT);   //status LED on chip
  pinMode(BUTTON_PIN, INPUT);
  servoA.attach(SERVO_PIN_1);
  servoB.attach(SERVO_PIN_2); 
  
  //servo at start position
  posA = degMin;
  posB = degMin;
  servoA.write(posA);              // tell servo to go to position in variable 'pos'
  servoB.write(posB);

  //wait for everything to be set up
  delay(2000);

  //output version
  Serial.print("Drag:on-Unit v");
  Serial.print(VERSION);
  Serial.println("-s&n");

  //STANDALONE OR NORMAL MODE
  isStandalone = digitalRead(BUTTON_PIN) == HIGH;

  if(isStandalone){
    //output mode
    Serial.println("STANDALONE MODE ACTIVE");
  
    //indicate initialization finished
    blinkLED(10,300);
  }else{
    //output mode
    Serial.println("NORMAL MODE ACTIVE");
      
    //serial communication protocol
    scmd.addCommand("FANS", fansHandler);
    scmd.addCommand("STATE", stateHandler);
    
    //indicate initialization finished
    blinkLED(1,2000);
  }
}


void loop()
{
  //BUTTON HANDLING
  //check for button state
  if(curPressed && digitalRead(BUTTON_PIN) == LOW){
    Serial.println("BUTTON RELEASED");
    curPressed = false;
    digitalWrite(BUILTIN_LED, LOW);
  }
  if(!curPressed && digitalRead(BUTTON_PIN) == HIGH){
    if(isStandalone){
      switchState = true;
    }
    Serial.println("BUTTON PRESSED");
    curPressed = true;
    digitalWrite(BUILTIN_LED, HIGH);
  }

  //NORMAL MODE: check for new commands
  if (!isStandalone && Serial.available() > 0)
    scmd.readSerial();

  //STANDALONE MODE: switch to next state
  if(isStandalone && switchState){
    switchState = false;
    currentStateIndex = (currentStateIndex + 1) % numberStates;
    if(currentStateIndex == 0){
      fansHandlerSTR("0", "0");
    } else if(currentStateIndex == 1){
      fansHandlerSTR("100", "100");
    } else if(currentStateIndex == 2){
      fansHandlerSTR("100", "0");
    } else if(currentStateIndex == 3){
      fansHandlerSTR("50", "50");
    } else if(currentStateIndex == 4){
      fansHandlerSTR("0", "100");
    } 
  }
}

//transform fans from serial command
void fansHandler () {
  const char *argA = scmd.next();
  const char *argB = scmd.next();
  fansHandlerSTR(argA, argB);
}

//transform fans from string
void fansHandlerSTR(const char *percentA, const char *percentB){
  const char *arg;
  int targetA = posA;
  int targetB = posB;
  
  //read target A
  arg = percentA;
  if(arg != NULL){
    if((strcmp(arg, "X") != 0) && (strcmp(arg, "x") != 0)){    //do nothing if "X", else parse target position
      targetA = (int)((atof(arg) / 100.0) * (degMax - degMin)) + degMin;
      if(degMin > targetA || targetA > degMax){
        targetA = posA;
      }
    }
  }

  //read target B 
  arg = percentB;
  if(arg != NULL){
    if((strcmp(arg, "X") != 0) && (strcmp(arg, "x") != 0)){    //do nothing if "X", else parse target position
      targetB = (int)((atof(arg) / 100.0) * (degMax - degMin) * limitationB) + degMin;
      if(degMin > targetB || targetB > degMax){
        targetB = posB;
      }
    }
  }

  //apply
  if(targetA != posA){
    posA = targetA;
    servoA.write(posA);
  }
  if(targetB != posB){
    posB = targetB;
    servoB.write(posB);
  }

  //send feedback
  stateHandler();
}

//send current positions of fan
void stateHandler(){
    int percentA = (int)((((float)posA - (float)degMin)/(float)(degMax - degMin))*100.0);
    int percentB = (int)((((float)posB - (float)degMin)/((float)(degMax - degMin) * limitationB))*100.0);
    Serial.print("FAN-A ");
    Serial.print(percentA);
    Serial.print(" FAN-B ");
    Serial.println(percentB);
}

//blink the LED
void blinkLED(int times, int duration){
  for(int i=0; i<times; i++){
    digitalWrite(BUILTIN_LED, HIGH);
    delay(duration);
    digitalWrite(BUILTIN_LED, LOW);
    delay(duration);
  }
}

