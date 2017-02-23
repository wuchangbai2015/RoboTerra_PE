/**********************************************************************************
 RoboTerraMotor.cpp
 Copyright (c) 2015 ROBOTERRA, Inc. All rights reserved.

 Current Revision
 1.7

 Description
 This is a library for controlling ROBOTERRA DC motor.
 PWM output generated by Timer 0 is used to control DC Motor.
 
 History
 When         Who           Revision    What/Why            
 ---------    ----------    --------    ---------------
 8/11/2015    Quincy Liu    1.0         Initially create   
 9/14/2015    Bai Chen      1.1         Rename functions
 10/20/2015   Bai Chen      1.2         1.Implement stop function
                                        2.Add speed mapping
 12/20/2015   Bai Chen      1.3         Rewrite for event-driven implementation 
 01/03/2016   Bai Chen      1.4         1. Implement event queue to store EVENTs
                                        2. Send event message to UI immediately 
                                        3. Keep track No. of active motors  
 01/15/2016   Bai Chen      1.5         Implement RoboTerraMotor state diagram 
 05/16/2016   Zan Li        1.6         Change pin mapping for RoboCore V1.3, V1.4 & V1.5  
 07/30/2016   Bai Chen      1.7         1. Make it deactived by default
                                        2. Rotate function now takes zero                                                    
 **********************************************************************************/

#include <RoboTerraMotor.h>

#define MOTOR_A_ID      5
#define MOTOR_B_ID      6

// pin map for RoboCore V1.3, V1.4 & V1.5
#define MOTOR_A_PWM_PIN 6
#define MOTOR_A_DIR_PIN 2
#define MOTOR_B_PWM_PIN 5
#define MOTOR_B_DIR_PIN 4

// pin map for RoboCore V1.1 & V1.2
// #define MOTOR_A_PWM_PIN 5
// #define MOTOR_A_DIR_PIN 4
// #define MOTOR_B_PWM_PIN 6
// #define MOTOR_B_DIR_PIN 2

#define STATE_STOP      1
#define STATE_MOVE      2

#define DEVICE_ID       130
#define MSG_LENGTH      6

/***************************** Module Variable *****************************/

unsigned char activeMotorNum = 0; // No. of active motors

/************************** Class Member Functions *************************/

void RoboTerraMotor::activate() {
    if (isActive) {
        return;
    }
    RoboTerraElectronics::activate(); // Parent class
    activeMotorNum++;
    
    state = STATE_STOP;
    stateMachineFlag = false; // Let kernal NOT call runStateMachine()

    sendEventMessage(STATE_STOP, ACTIVATE, (int)activeMotorNum, 0);
    generateEvent(ACTIVATE, (int)activeMotorNum, 0);
}

void RoboTerraMotor::deactivate() {
    if (!isActive) {
        return;
    }
    RoboTerraElectronics::deactivate(); // Parent class 
    if (activeMotorNum != 0) {
        activeMotorNum--;
    }

    if (state == STATE_STOP) {
        sendEventMessage(STATE_INACTIVE, DEACTIVATE, (int)activeMotorNum, 0);
        generateEvent(DEACTIVATE, (int)activeMotorNum, 0);
    }
    else if (state == STATE_MOVE) {
        analogWrite(motorSpeedPin, 0); // Motor stops w/o changing speed

        sendEventMessage(STATE_INACTIVE, MOTOR_SPEED_ZERO, 0, (int)direction);
        generateEvent(MOTOR_SPEED_ZERO, 0, (int)direction);
        sendEventMessage(STATE_INACTIVE, DEACTIVATE, (int)activeMotorNum, 0);
        generateEvent(DEACTIVATE, (int)activeMotorNum, 0);
    }

    state = STATE_INACTIVE;
    stateMachineFlag = false; // To let kernal NOT call runStateMachine()
}

void RoboTerraMotor::rotate(int speedToSet) {
    if (!isActive) {
        return;
    }
    if (speedToSet == 0) {
        if (state == STATE_MOVE) {
            speed = 0;
            pause();
        }
        return;
    }
    if (speedToSet < -10 || speedToSet > 10) { // Outside range
        return;
    }

    if (state == STATE_STOP) {
        state = STATE_MOVE;
        stateMachineFlag = false;

        speed = (char)speedToSet;
        if (speed > 0) {
            direction = true;
            digitalWrite(motorDirPin, direction);
            analogWrite(motorSpeedPin, speed * 20 + 20); // Mapping to 40 - 220
        }
        else {
            direction = false;
            digitalWrite(motorDirPin, direction);
            analogWrite(motorSpeedPin, (-1) * speed * 20 + 20); // Mapping to 40 - 220
        }

        sendEventMessage(STATE_MOVE, MOTOR_SPEED_CHANGE, (int)abs(speed), (int)direction);
        generateEvent(MOTOR_SPEED_CHANGE, (int)abs(speed), (int)direction);
    }
    else if (state == STATE_MOVE) {
        if (speedToSet == speed) { // same speed
            return;
        }
        else {
            if (speedToSet > 0) {
                direction = true;
                digitalWrite(motorDirPin, direction);
                analogWrite(motorSpeedPin, speedToSet * 20 + 20); // Mapping to 40 - 220
            }
            else {
                direction = false;
                digitalWrite(motorDirPin, direction);
                analogWrite(motorSpeedPin, (-1) * speedToSet * 20 + 20); // Mapping to 40 - 220
            }

            if ((speed > 0 && speedToSet < 0) || (speed < 0 && speedToSet > 0)) {
                sendEventMessage(STATE_MOVE, MOTOR_REVERSE, abs(speedToSet), (int)direction);
                generateEvent(MOTOR_REVERSE, abs(speedToSet), (int)direction);
            }
            if (abs(speed) != abs(speedToSet)) {
                sendEventMessage(STATE_MOVE, MOTOR_SPEED_CHANGE, abs(speedToSet), (int)direction);
                generateEvent(MOTOR_SPEED_CHANGE, abs(speedToSet), (int)direction);
            }
            speed = (char)speedToSet; // Update speed
        }
    }  
}

void RoboTerraMotor::reverse() {
    if (!isActive) {
        return;
    }

    if (state == STATE_MOVE) {
        direction = !direction;
        digitalWrite(motorDirPin, direction);
        
        sendEventMessage(STATE_MOVE, MOTOR_REVERSE, (int)abs(speed), (int)direction);
        generateEvent(MOTOR_REVERSE, (int)abs(speed), (int)direction);
    }
}

void RoboTerraMotor::pause() {
    if (!isActive) {
        return;
    }

    if (state == STATE_MOVE) {
        analogWrite(motorSpeedPin, 0);
        
        state = STATE_STOP;
        stateMachineFlag = false;  

        sendEventMessage(STATE_STOP, MOTOR_SPEED_ZERO, 0, (int)direction);
        generateEvent(MOTOR_SPEED_ZERO, 0, (int)direction);
    }
}

void RoboTerraMotor::resume() {
    if (!isActive) {
        return;
    }

    if (state == STATE_STOP && speed != 0) {
        if (speed > 0) {
            direction = true;
            digitalWrite(motorDirPin, direction);
            analogWrite(motorSpeedPin, speed * 20 + 20); // Mapping to 40 - 220
        }
        else {
            direction = false;
            digitalWrite(motorDirPin, direction);
            analogWrite(motorSpeedPin, (-1) * speed * 20 + 20); // Mapping to 40 - 220
        }
        
        state = STATE_MOVE;
        stateMachineFlag = false;  

        sendEventMessage(STATE_MOVE, MOTOR_SPEED_CHANGE, (int)abs(speed), (int)direction);
        generateEvent(MOTOR_SPEED_CHANGE, (int)abs(speed), (int)direction);
    }
}

void RoboTerraMotor::attach(int portID) {
    // Allocate memomry for RoboTerraEventQueue
    sourceEventQueue = new RoboTerraEventQueue; 

    if (portID == MOTOR_A_ID) {
        pin = MOTOR_A_ID;
        motorSpeedPin = MOTOR_A_PWM_PIN;
        motorDirPin = MOTOR_A_DIR_PIN;
    } 
    else if (portID == MOTOR_B_ID) {
        pin = MOTOR_B_ID;
        motorSpeedPin = MOTOR_B_PWM_PIN;
        motorDirPin = MOTOR_B_DIR_PIN;
    }
    else {
        return; // Invalid Motor PortID
    }
    pinMode(motorDirPin, OUTPUT);
    speed = 0;
    direction = true;    
    
    state = STATE_INACTIVE;
    RoboTerraElectronics::deactivate();
    sendEventMessage(STATE_INACTIVE, DEACTIVATE, (int)activeMotorNum, 0);
    generateEvent(DEACTIVATE, (int)activeMotorNum, 0);
}

bool RoboTerraMotor::readStateMachineFlag() {
    return stateMachineFlag;
}

void RoboTerraMotor::runStateMachine() {
    // Left blank intentionally b/c kernal doesn't need to call this function 
    // The reason is that RoboTerraMotor class doesn't require active polling
}

/************************** Private Class Functions *************************/

void RoboTerraMotor::sendEventMessage(char stateToSend, RoboTerraEventType typeToSend, int firstDataToSend, int secondDataToSend) {
    uint8_t eventMessageLength = 6 + MSG_LENGTH;
    uint8_t eventMessage[eventMessageLength]; // EVENT Message
    
    eventMessage[0] = 0xF0;                   // EVENT Message Begin
    eventMessage[1] = 0x01;                   // EVENT Count
    eventMessage[2] = (uint8_t)DEVICE_ID;     // EVENT Source Device ID
    eventMessage[3] = (uint8_t)pin;           // EVENT Source Port
    eventMessage[4] = (uint8_t)MSG_LENGTH;    // Message Length
    eventMessage[5] = (uint8_t)stateToSend;
    eventMessage[6] = (uint8_t)typeToSend;
    eventMessage[7] = (uint8_t)firstDataToSend;
    eventMessage[8] = (uint8_t)(firstDataToSend >> 8);
    eventMessage[9] = (uint8_t)secondDataToSend;
    eventMessage[10] = (uint8_t)(secondDataToSend >> 8);
    eventMessage[11] = 0xFF;                  // End marker

    Serial.write(eventMessage, eventMessageLength);
}

void RoboTerraMotor::generateEvent(RoboTerraEventType type, int firstData, int secondData) {
    RoboTerraEvent newEvent(this, type, firstData);
    newEvent.setEventData(secondData, 1);
    sourceEventQueue->enqueue(newEvent);
}