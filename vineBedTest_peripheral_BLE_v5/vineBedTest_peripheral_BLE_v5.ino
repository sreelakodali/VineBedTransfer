/*
  Motor Control for Vine Bed Transfer
  Sreela Kodali kodali@stanford.edu

  modified by Godson Osele (5/31/24) to add CommandOverTime command to BLE motorService

  This creates a BLE peripheral with a service that contains multiple
  characteristics to control different groups of the brushless motors.

  Godson wrote new functions 6-4-2024, Sreela incorporated them into BLE 6-6-2024
*/

#include <ArduinoBLE.h>
#include <Servo.h>


// ----------------------  INITIAL: CREATING DATA STRUCTS AND VARIABLES  -----------------------------------//

#define N_ACT 8   // Number of actuators, 8 motors
#define N_CMDS 3 // number of commands

// These are the range of PWM values the SPARK Max Controller understands
// See PWM Input Specs here for more details:
// https://docs.revrobotics.com/sparkmax/specifications
// The PWM signal that goes to the Spark Max is generated by a function called
// writeMicroseconds(x).
// motor.writeMicroseconds(1025) = reverse max speed
// motor.writeMicroseconds(1500) = neutral, no movement
// motor.writeMicroseconds(2000) = forwawrd max speed

typedef enum {
  MOTOR_MIN = 1025,
  MOTOR_NEUTRAL = 1500,
  MOTOR_MAX = 2000,
  MOTOR_NEUTRAL_MID = 1518
} MOTOR_LIMITS;

const int maxParams = 4;  // Maximum parameters that can be sent in a single command
const bool serialOn = false;
const int activeVines[3] = {6, 7, 8};
const int nVines = (sizeof(activeVines) / sizeof(activeVines[0]));
const int activeTCWS[3] = {2, 3, 4};
const int nTCWS = (sizeof(activeTCWS) / sizeof(activeTCWS[0]));

// IF YOU WANT TO SEND THE SAME COMMAND TO GROUPS OF MOTORS
// I created a little structure "cmd" (short for "command") where you give it a name,
// and identify which motors you want to run with a binary array so you can run different
// groups of motors at the same time! For example, if I want to control motors 1-4 at the
// same time, I'd add the following to my allCommands[] array:
//         initializeCmd("myCommand", {1, 1, 1, 1, 0, 0, 0, 0, 0})
// Then, when I open the app and connect with the Arduino, "myCommand" will appear as an
// option where I can write/send one value that'll go to motors 1-4.

struct cmd {
  char* strname;
  int* motors;
  BLEIntCharacteristic ble;
};

cmd initializeCmd(char* s, int* m) {
  BLEIntCharacteristic bleCharacteristic("2A59", BLEWrite);  // 2A59, analog output
  BLEDescriptor des("2901", s);                              // adding user description (2901) for the characteristic
  bleCharacteristic.addDescriptor(des);
  bleCharacteristic.writeValue(0);
  cmd c{ s, m, bleCharacteristic };
  return c;
}

BLEService motorService("01D");  // Bluetooth® Low Energy, motorized device


// IMPORTANT!!!
// IF YOU WANT TO ADJUST THE RANGE, RESOLUTION, SPEEDS FOR THE MAPPING OF BLE-TO-MOTOR CMDS,
// ETC. BEYOND THE DEFAULT TEN PRESET VALUES, READ THIS!

// The default works simply and as follows: We have a small integer array of length 10 called
// uSCommandValues (abbreviation for // microsecondCommandValues) that has 10 distinct motor
// commands within the SparkMax's valid range between 1000 and 2000 (https://docs.revrobotics.com/sparkmax/specifications),
// as shown on line 90.
// The BLE commands are the index for this array. So for example, if someone sends
// a "1" command via their phone to the motors, the motors would receive
// writeMicroseconds(usCommandValues[1])=MOTOR_MIN.
// If you'd like different values, feel free to change the content and/or length of USCommandVValues
const int uSCommandValues[10] = { MOTOR_NEUTRAL, MOTOR_MIN, 1185, 1285, 1385, MOTOR_NEUTRAL, 1650, 1750, 1850, MOTOR_MAX };


// which motors will be on for each command
// tcw1, tcw2, tcw3, tcw4, base1, base2, base3, base4, pressure regulator
int motors_CMD1[N_ACT] = { 0, 0, 0, 0, 1, 1, 1, 1 };
int motors_CMD2[N_ACT] = { 1, 1, 1, 1, 0, 0, 0, 0 };
int motors_ALL[N_ACT] = { 1, 1, 1, 1, 1, 1, 1, 1 };
int motors_NONE[N_ACT] = { 0, 0, 0, 0, 0, 0, 0, 0 };


cmd allCommands[N_CMDS] = {
  // initializeCmd("allBase", motors_CMD1),
  // initializeCmd("allTCWTurn", motors_CMD2),
  initializeCmd("individualMotor", motors_NONE),
  initializeCmd("PreLoadValues", motors_ALL),
  initializeCmd("executeCommand", motors_NONE)
  // initializeCmd("CommandOverTime", motors_ALL),      // New command for motor with duration
  // initializeCmd("motorsWithTwoSpeeds", motors_ALL),  // New command for all motors with two speeds and duration
  // initializeCmd("halfAndHalfSwap", motors_ALL)      // New command for sending different speeds to different halves and swapping
};

Servo motorArr[N_ACT];
const int pins_CommandOUTArr[N_ACT] = { 2, 3, 4, 5, 6, 7, 8, 9 }; // these pins correspond  to different motor's input

// ----------------------  END OF INITIAL: CREATING DATA STRUCTS AND VARIABLES  -----------------------------------//


// ----------------------  SETUP  -----------------------------------//

void setup() {

  if (serialOn) {
    Serial.begin(9600);
    while (!Serial);
  }
  
  if (!BLE.begin()) { // begin initialization
    if (serialOn) { 
      Serial.println("starting Bluetooth® Low Energy module failed!");
    }
    while (1);
  }
  // set advertised local name and service:
  BLE.setLocalName("Nano 33 BLE");
  BLE.setAdvertisedService(motorService);

  // connect each motor to PWM output and set to neutral
  for (int i = 0; i < N_ACT; ++i) {
    motorArr[i].attach(pins_CommandOUTArr[i]);  
    motorArr[i].writeMicroseconds(MOTOR_NEUTRAL);
  }

  //add characteristics to service
  for (int i = 0; i < N_CMDS; ++i) {
    motorService.addCharacteristic(((allCommands[i]).ble));  // add the characteristic to the service
  }
  BLE.addService(motorService);  // add service
  BLE.advertise();               // start advertising

  if (serialOn) {
    Serial.println("Initialized.");
  }
}

// ----------------------  END OF SETUP  -----------------------------------//


// --------------- LOOP ---------------//

void loop() {

  // listen for Bluetooth® Low Energy peripherals to connect:
  BLEDevice central = BLE.central();

  if (central) {   // if a central is connected to peripheral:
    while (central.connected()) {     // while the central is still connected to peripheral:
      for (int i = 0; i < N_CMDS; ++i) { // poll through the commands
        if (((allCommands[i]).ble).written()) {

          char* n = (allCommands[i]).strname;
          unsigned long x = ((allCommands[i]).ble).value();

          // INPUT: 8 HEX values via BLE         HOW MANY MOTORS RUN: 8 MOTORS
          // What it does: Specify speed/dir command for all 8 motors in 8-digit HEX value
          if (n == "PreLoadValues") {
            if (serialOn) {
              Serial.println("preload");
            }
            preloadValues(x);

          } else if (n == "executeCommand") {
            if (serialOn) {
              Serial.println("executeCommand");
            }
            executeCommand(x);
          } 

          // // INPUT: 3 HEX values via BLE         HOW MANY MOTORS RUN: 1 motor
          // // What it does: Specify a motor index, speed/dir value, and duration of time for single motor to run
          // else if (n == "CommandOverTime") {
          //   unsigned long x = allCommands[i].ble.value();
          //   commandOverTime(x);

          // // INPUT: 3 HEX values via BLE         HOW MANY MOTORS RUN: 8 motors
          // // What it does: specify 2 speeds and a duration for all motors to run and then stop
          // } else if (n == "motorsWithTwoSpeeds") {
          //   twoSpeeds(x);

          // // INPUT: 3 HEX values via BLE         HOW MANY MOTORS RUN: 8 motors
          // // What it does: specify 2 speeds and duration. first half motors run speed1, 
          // // second run speed2 for duration. then switch, first half speed2, second half
          // // speed1, then stop.
          // } else if (n == "halfAndHalfSwap") {
          //   handleHalfAndHalfSwapCommand(x);
          // }

          else {
          // DEFAULT BEHAVIOR FOR allBase, allTCWTurn, allBaseTCWTurn, individualMotors
            unsigned long x = ((allCommands[i]).ble).value();
            unsigned long z = (x & 0b11110000) >> 4;
            x = (x & 0b00001111);
            int y = 5;
            //            Serial.println(x, HEX);

            // if value > 16, change motorArr to make sure motor of MSB turns on
            if (z) {
              for (int j = 0; j < N_ACT; ++j) {
                if (z - 1 == j) {
                  (allCommands[i]).motors[j] = 1;
                } else {
                  (allCommands[i]).motors[j] = 0;
                }
              }
            }
            // otherwise read LSB as motor command
            if (x >= 0 && x < 10) {
              y = (int)x;
              //Serial.println(y);
            }

            // for motors that are on as per motorArr, pass commands. otherwise off
            for (int j = 0; j < N_ACT; ++j) {
              if ((allCommands[i]).motors[j]) {
                if (y != 0) {
                  if ( (j == 5) || (j == 7)) { // flip for motors 6 and 8
                    y = 5 + (5 - y);
                  } 
                }
                motorArr[j].writeMicroseconds(uSCommandValues[y]);
              } else {
                motorArr[j].writeMicroseconds(MOTOR_NEUTRAL);
              }
            }
          }
          delay(500);
        }
      }
    }
  }
}
// ---------------  END OF LOOP ---------------//




//  ---------------------- SUPPORT FUNCTIONS ------------------------------ //


// ------  FUNCTION 1: Extracting Bytes: extract right digits from hexadecimal BLE values ------ //
// pass in either 3 or 8
// length is how many bytes you want to send
int extractByte(int length, int idx, unsigned long commandValue) {

  if (idx > (length - 1)) {
    return -1; // -1 is error
  } else {
    int valueArr[length];
    if (length == 2) {
      valueArr[0] = (commandValue & 0xF0) >> 4;
      valueArr[1] = (commandValue & 0x0F) >> 0;
    } else if (length == 4) {
      valueArr[0] = (commandValue & 0x00F0) >> 4;
      valueArr[1] = (commandValue & 0x000F) >> 0;
      valueArr[2] = (commandValue & 0xF000) >> 12;
      valueArr[3] = (commandValue & 0x0F00) >> 8;
    } else if (length == 3) {
      valueArr[0] = (commandValue & 0x000F) >> 0;
      valueArr[1] = (commandValue & 0xF000) >> 12;
      valueArr[2] = (commandValue & 0x0F00) >> 8;
    } else if (length == 8) {
      valueArr[0] = (commandValue & 0x000000F0) >> 4;
      valueArr[1] = (commandValue & 0x0000000F) >> 0;
      valueArr[2] = (commandValue & 0x0000F000) >> 12;
      valueArr[3] = (commandValue & 0x00000F00) >> 8;
      valueArr[4] = (commandValue & 0x00F00000) >> 20;
      valueArr[5] = (commandValue & 0x000F0000) >> 16;
      valueArr[6] = (commandValue & 0xF0000000) >> 28;
      valueArr[7] = (commandValue & 0x0F000000) >> 24;    
    }
    return valueArr[idx];
  }
}
// ------  END OF FUNCTION 1: Extracting Bytes  ------ //

// ------  FUNCTION 2 ------ //
// pass in 8 hexadecimal digits via BLE
// A (or any Letter) causes immediate stop
void preloadValues(unsigned long commandValue) {
  
  // create and initialize
  int commands[N_ACT];
  bool sendCommand = true;
  for (int k = 0; k < N_ACT; ++k) {
    commands[k] = 0;
  }

  // extract values
  for (int k = 0; k < N_ACT; ++k) {
    commands[k] = extractByte(8, k, commandValue);
    // if any of the digits sent are A or greater, stop
    if (commands[k] >= 10 or commands[k] < 1) {
      sendCommand = false;
      break;
    }
  }
  // if no A / stop command sent, pass along command values
  if (sendCommand) {
    for (int k = 0; k < N_ACT; ++k) {
      int idx = 5; // neutral

      // for motors 6 and 8, flip the values
      if (commands[k] != 0) {
        if ( (k == 5) || (k == 7)) {
          idx = 5 + (5 - commands[k]);
        } else {
          idx = commands[k];
        }
      }
      // Serial.println(commands[k]);
      motorArr[k].writeMicroseconds(uSCommandValues[idx]);
    }
  } else { // otherwise stop 
    Estop();
  }
}

// ------  END OF FUNCTION 2 ------ //

// ------  FUNCTION 3 ------ //


// A (formerly 'K' in serial) - individual motor. Takes 2 params: motor index + speed/dir value
// B (formerly 'B' in serial) - allVines or allBases. Takes 1 param: speed/dir
// C (formerly 'T' in serial) - allTCW. Takes 1 param: speed/dir

// D - tunedVineDep. No params. Runs vines with pre-specified values.
// E - estop. No params. Stops all motors
// F (formerly 'L' in serial) - lift and return. No params. 

void executeCommand(unsigned long commandValue) {
  
  int command[maxParams];

  for (int i = 0; i < maxParams; ++i) {
    command[i] = extractByte(maxParams, i, commandValue);

    switch(command[i]) {

      // E-STOP
      case 14: //E,  IF any 'E' is DETECTED, E-STOP.  E IS 14 in HEX
        if (serialOn) { 
          Serial.println("Estop");
        }
        Estop();
        break;

      // TUNEDVINEDEP // FIX: FLIP
      case 13: // IF any 'D' is DETECTED, TUNEDVINEDEP. D IS 13 in HEX
        if (serialOn) { 
          Serial.println("TunedVineDep");
        }
        TunedVineDep();
        break;

      case 15: // IF any 'F' is DETECTED, LIFTANDRETURN. F IS 15
        if (serialOn) { 
          Serial.println("LiftAndReturn");
        }
        LiftandReturn();
        break;

      case 12: // IF any 'C' is DETECTED, AllTCW. C is 12
        if (serialOn) { 
          Serial.println("allTcw");
        }
        AllTCW(commandValue);
        break;
      
      case 11: // IF any 'B' is DETECTED, AllVines (like allBases). B is 11
        if (serialOn) { 
          Serial.println("allVines");
        }
        AllVines(commandValue);
        break;

      case 10: // IF any 'A' is DETECTED, runMotor. A is 10
        if (serialOn) { 
          Serial.println("runMotor");
        }
        runMotor(commandValue);
        break;

      default:
        Estop();
       break;

    }
  }
}


// Turn each motor individually
// in BLE, send as: [ A, MOTOR IDX, SPEED/DIR ]
// MOTOR IDX is 1-8. flip for motor6 and 8 accounted for
void runMotor(unsigned long commandValue) {
  // This function sets the specified motor to the given speed
  int n = 3;   // 2 param, 1 cmd = 3
  int command[n];
  for (int i = 0; i < n; ++i) {
    command[i] = extractByte(n, i, commandValue);
  }
  int speedIdx = command[2];
  int motorIdx = command[1]-1;
  
  // for motors 6 and 8, flip the values
  if (speedIdx != 0) {
    if ( (motorIdx == 5) || (motorIdx == 7)) {
      speedIdx = 5 + (5 - speedIdx);
    } 
  }

  if (serialOn) {
    Serial.println(motorIdx + 1); 
    Serial.println(speedIdx);
  }
  int speed = uSCommandValues[speedIdx];
  motorArr[motorIdx].writeMicroseconds(speed);
}


//Control all TCWs simultaneously
// TCWs are motors 1, 2, 3, 4
void AllTCW_arr(const int* speed) {
  // send array commands only if array right length
//  int l = (sizeof(speed) / sizeof(speed[0]));
//  if (l == N_ACT / 2) {
    if (1) {
    //int speed = uSCommandValues[speedIdx];
    for (int i = 0; i < nTCWS; ++i) {
      if (serialOn) {
        Serial.println(String(activeTCWS[i]) + ", " + String(speed[i]));  
      }
      motorArr[activeTCWS[i]-1].writeMicroseconds(speed[i]);
    }
  }
}

//Control all TCWs simultaneously, same command
// TAKES FULL HEXADECIMAL INPUT FROM BLE
void AllTCW(unsigned long commandValue) {
  int n = 2;   // 1 param, 1 cmd = 2
  int command[n];
  for (int i = 0; i < n; ++i) {
    command[i] = extractByte(n, i, commandValue);
  }
  int speedIdx = command[1];
  int speed = uSCommandValues[speedIdx];
  for (int i = 0; i < nTCWS; ++i) {
    if (serialOn) {
    Serial.println(activeTCWS[i]);
    Serial.println(speedIdx);
  }
    motorArr[activeTCWS[i]-1].writeMicroseconds(speed);
  }
}

//Control all Vine Bases simultaneously
// motors are 5, 6, 7, 8
// gets const int* array with explicit microsecond values, not speed idx
// Flip accounted for
void AllVines_arr(const int* speed) {
    // send array commands only if array right length
   
//  int l = (sizeof(*speed) / sizeof(speed[0]));
//  if (serialOn) {
//    Serial.println(l);
//   }
//  if (l == N_ACT / 2) {
    if (1) {
    for (int i = 0; i < nVines; ++i) {
      int s = speed[i];
      if ((activeVines[i] == 6) || (activeVines[i] == 8)) { // if motors 6 and 8, flip the value
        s = MOTOR_NEUTRAL_MID + (MOTOR_NEUTRAL_MID - s);
      }
      if (serialOn) {
        Serial.println(String(activeVines[i]) + ", " + String(s));  
      }
      motorArr[activeVines[i]-1].writeMicroseconds(s);
    }
  }
}

//Control all Vine Bases simultaneously, same command
// TAKES FULL HEXADECIMAL INPUT FROM BLE. Flip accounted for
void AllVines(unsigned long commandValue) {
  int n = 2;   // 1 param, 1 cmd = 2
  int command[n];
  for (int i = 0; i < n; ++i) {
    command[i] = extractByte(n, i, commandValue);
  }

  for (int i = 0; i < nVines; ++i) {
    int speedIdx = command[1];
      // for motors 6 and 8, flip the values
    if (speedIdx != 0) {
      if ( (activeVines[i] == 6) || (activeVines[i] == 8)) {
        speedIdx = 5 + (5 - speedIdx);
      } 
    }
    if (serialOn) {
      Serial.println(activeVines[i]); 
      Serial.println(speedIdx);
    }
    int speed = uSCommandValues[speedIdx];
    motorArr[i].writeMicroseconds(speed);
  }
}



//Initiate Lift and Return Sequence
//Lifting subject from bed and putting back down with preset speeds
void LiftandReturn() {
  int arraysz = N_ACT / 2;
  // speed are chosen based on the expected ratio relationship of the rotational velocities
  // of the base and TCW per their respective radii. The "delta" from motor neutral is based
  // this ratio.
  // I.E. TCW speed = NEUTRAL + 150 -> Base speed = NEUTRAL - (150*(R_TCW/R_base))

  const int TCWSpeedLift[arraysz] = {1436, 1436, 1436, MOTOR_NEUTRAL };    // TCW speeds during lift
  const int TCWSpeedReturn[arraysz] = { 1600, 1600, 1600, MOTOR_NEUTRAL };  // TCW speeds during return
  const int BaseSpeedLift[arraysz] = { 1791, 1791, 1791, MOTOR_NEUTRAL };    // Base speeds during lift
  const int BaseSpeedReturn[arraysz] = { 1245, 1245, 1245, MOTOR_NEUTRAL};  // Base speeds during return
  int duration = 5;                                                 // duration of lift and return respectively
  int carryTime = 3;

  // Set first half of motors to speed1 and second half to speed2
  AllTCW_arr(TCWSpeedLift);
  AllVines_arr(BaseSpeedLift);
  delay(duration * 1000);  // Convert duration of lift to seconds

  // Set all motors back to neutral
  Estop();
  delay(carryTime * 1000);  // 3 second delay between commands

  // Reverse the direction: Set first half to speed2 and second half to speed1
  AllTCW_arr(TCWSpeedReturn);
  AllVines_arr(BaseSpeedReturn);
  delay(duration * 1000);  // Convert duration of return to seconds

  // Set all motors back to neutral
  Estop();
}


//Function deploys all vines simultaneously with unique hard coded speeds.
// Written to address initial growth of vine down and under subject
void TunedVineDep() {
  int arraysz = N_ACT / 2;
  const int BaseSpeedDep[arraysz] = { 1385, 1385, 1385, MOTOR_NEUTRAL };  // Base speeds during initial deployment
  AllVines_arr(BaseSpeedDep);
}


//Turn off all motors
void Estop() {
  for (int i = 0; i < N_ACT; ++i) {
    motorArr[i].writeMicroseconds(MOTOR_NEUTRAL);
  }
}

// ------  FUNCTION 3 ------ //
// pass in 3 values
void commandOverTime(unsigned long commandValue) {
  int motorIndex = extractByte(3, 0, commandValue);  // Extract motor index from high byte
  int speedIdx = extractByte(3, 1, commandValue);    // Extract speed index from mid byte
  int duration = extractByte(3, 2, commandValue);            // Extract duration from low byte

  if (motorIndex < N_ACT && speedIdx >= 0 && speedIdx < 10) {
    int speed = uSCommandValues[speedIdx];
    motorArr[motorIndex].writeMicroseconds(speed);
    delay(duration * 1000);  // Convert duration to milliseconds
    motorArr[motorIndex].writeMicroseconds(MOTOR_NEUTRAL);
  }
}
// ------  END OF FUNCTION 3 ------ //

// ------  FUNCTION 4 ------ //
// pass in 3 values
void twoSpeeds(unsigned long commandValue) {
  int speedIdx1 = extractByte(3, 0, commandValue);  // Extract first speed index from high byte
  int speedIdx2 = extractByte(3, 1, commandValue);   // Extract second speed index from mid byte
  int duration = extractByte(3, 2, commandValue);           // Extract duration from low byte

  if (speedIdx1 >= 0 && speedIdx1 < 10 && speedIdx2 >= 0 && speedIdx2 < 10) {
    int speed1 = uSCommandValues[speedIdx1];
    int speed2 = uSCommandValues[speedIdx2];

    // write speed1 for duration
    for (int j = 0; j < N_ACT; ++j) {
      motorArr[j].writeMicroseconds(speed1);
    }
    delay(duration * 1000);  // Convert duration to milliseconds

    // stop motors for duration
    Estop();
    delay(duration * 1000);  // Neutral duration

    // write speed2 for duration
    for (int j = 0; j < N_ACT; ++j) {
      motorArr[j].writeMicroseconds(speed2);
    }
    delay(duration * 1000);  // Second speed duration

    // stop motors
    Estop();
  }
}
// ------ END OF FUNCTION 4 ------ //

// ------  FUNCTION 5 ------ //
// pass in 3 values
void handleHalfAndHalfSwapCommand(unsigned long commandValue) {
  //expected 3 values delivered as hexadecimal

  int speed1Idx = extractByte(3, 0, commandValue);//From most significant bit
  int speed2Idx = extractByte(3, 1, commandValue); // From 'mid'bit
  int duration = extractByte(3, 2, commandValue);// OR 

  int speed1 = uSCommandValues[speed1Idx];
  int speed2 = uSCommandValues[speed2Idx];

  // Set first half of motors to speed1 and second half to speed2
  for (int i = 0; i < N_ACT / 2; ++i) {
    motorArr[i].writeMicroseconds(speed1);
  }
  for (int i = N_ACT / 2; i < N_ACT; ++i) {
    motorArr[i].writeMicroseconds(speed2);
  }
  delay(duration * 1000);  // Convert duration to seconds

  // Set all motors back to neutral
  Estop();
  delay(3000);  // 3 second delay between commands

  // Swap the speeds: Set first half to speed2 and second half to speed1
  for (int i = 0; i < N_ACT / 2; ++i) {
    motorArr[i].writeMicroseconds(speed2);
  }
  for (int i = N_ACT / 2; i < N_ACT; ++i) {
    motorArr[i].writeMicroseconds(speed1);
  }
  delay(duration * 1000);  // Convert duration to seconds

  // Set all motors back to neutral
  Estop();
}

// ------  END OF FUNCTION 5  ------ //
