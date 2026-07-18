#include "motors.h"
#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"

static Servo leftMotor;
static Servo rightMotor;

void motorsInit() {
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    leftMotor.setPeriodHertz(50);
    rightMotor.setPeriodHertz(50);
    leftMotor.attach(MOTOR_LEFT_PIN,  ESC_MIN_US, ESC_MAX_US);
    rightMotor.attach(MOTOR_RIGHT_PIN, ESC_MIN_US, ESC_MAX_US);
    motorsStop();
    Serial.println("Motors: arming ESCs...");
    delay(ESC_ARM_DELAY_MS);
    Serial.println("Motors: ready");
}

void motorsSet(float throttle, float yaw) {
    float left  = constrain(throttle + yaw, 0.0f, 1.0f);
    float right = constrain(throttle - yaw, 0.0f, 1.0f);
    leftMotor.writeMicroseconds(ESC_MIN_US  + (int)(left  * (ESC_MAX_US - ESC_MIN_US)));
    rightMotor.writeMicroseconds(ESC_MIN_US + (int)(right * (ESC_MAX_US - ESC_MIN_US)));
}

void motorsStop() {
    leftMotor.writeMicroseconds(ESC_MIN_US);
    rightMotor.writeMicroseconds(ESC_MIN_US);
}
