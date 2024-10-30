1
Arduino Code for Automated Rope Making Machine
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HX711.h>
#include <AccelStepper.h>
#include <Servo.h>
#include <Encoder.h>
// Pin Definitions
const int SERVO_PIN = 12; // Servo motor
const int LOAD_CELL_DOUT_PIN = 2; // Load cell HX711 DOUT
const int LOAD_CELL_SCK_PIN = 7; // Load cell HX711 SCK
const int ENCODER_CLK_PIN = 9; // Rotary encoder CLK
const int ENCODER_DT_PIN = 8; // Rotary encoder DT
const int ENCODER_SW_PIN = 13; // Rotary encoder SW
const int BTS7960_LPWM_PIN = 11; // BTS7960 LPWM
const int BTS7960_RPWM_PIN = 10; // BTS7960 RPWM
const int NEMA_STEP_PIN = 6; // Nema 17 step pin (A4988 STEP)
const int NEMA_DIR_PIN = 5; // Nema 17 direction pin (A4988 DIR)
const int REN = 3;
const int LEN = 4;
const int btn = A0;
// Load Cell
2
HX711 scale;
// Servo
Servo myServo;
// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2); // Address 0x27, 16 column, 2 rows
// Stepper Motor using A4988 Driver
AccelStepper stepper(AccelStepper::DRIVER, NEMA_STEP_PIN, NEMA_DIR_PIN);
// Rotary Encoder
Encoder myEnc(ENCODER_CLK_PIN, ENCODER_DT_PIN);
// Variables
volatile int encoderPos = 0; // Position of the rotary encoder
int lastEncoderPos = -1; // Last position of the rotary encoder
bool encoderPressed = false; // Flag for rotary encoder button press
bool mode = false; // Mode flag: false = tension, true = length
float tent; // Current tension value
float setTension = 0; // Desired tension value
int ropeLength = 0; // Current rope length
float setRopeLength = 0; // Desired rope length
bool running = false; // System running flag
unsigned long lastDebounceTime = 0; // Last time the encoder button was pressed
unsigned long debounceDelay = 50; // Debounce delay in milliseconds
unsigned long buttonPressTime = 0; // Time when button was pressed
3
bool buttonHeld = false; // Flag for long press detection
int servo_motor_angle = 0; // Position of the servo motor
int a = 0; // Initialize variable
float weight;
int motorSpeed = 20; // PWM value for motor speed (0-255)
const float PULSES_PER_REVOLUTION = 360.0; // Encoder pulses per revolution
const float DEGREES_PER_PULSE = 360.0 / PULSES_PER_REVOLUTION; // Degrees per pulse
const float STEPS_PER_REVOLUTION = 200.0; // Steps per revolution for stepper motor
const float MM_PER_STEP = 0.1; // Millimeters of rope per step (example value, needs
calibration)
const unsigned long LONG_PRESS_DURATION = 1000; // Long press duration in milliseconds
float calibration_factor = 2000;
// Setup function
void setup() {
 Serial.begin(9600); // Initialize serial communication for debugging
 pinMode(ENCODER_SW_PIN, INPUT_PULLUP); // Enable internal pull-up resistor for encoder
switch
 pinMode(BTS7960_LPWM_PIN, OUTPUT); // BTS7960 LPWM pin as output
 pinMode(BTS7960_RPWM_PIN, OUTPUT); // BTS7960 RPWM pin as output
 pinMode(NEMA_STEP_PIN, OUTPUT);
 pinMode(NEMA_DIR_PIN, OUTPUT);
 pinMode(REN, OUTPUT);
 pinMode(LEN, OUTPUT);
4
 digitalWrite(LEN, HIGH);
 digitalWrite(REN, HIGH);
 pinMode(btn,INPUT);
 lcd.init(); // Initialize the LCD
 lcd.backlight(); // Turn on the backlight
 lcd.setCursor(0, 0);
 lcd.print("Initializing...");
 Serial.println("Initializing...");
 myServo.attach(SERVO_PIN); // Attach servo motor
 Serial.println("Servo initialized");
 stepper.setMaxSpeed(1000); // Set max speed for stepper motor
 stepper.setAcceleration(500); // Set acceleration for stepper motor
 Serial.println("Stepper motor initialized");
 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("Set Tension:");
 Serial.println("Set Tension:");
 scale.begin(LOAD_CELL_DOUT_PIN, LOAD_CELL_SCK_PIN);
 scale.set_scale();
 scale.tare(); // Reset the scale to 0
 lcd.init();
5
 lcd.backlight();
 lcd.clear();
}
// Main loop function
void loop() {
 // Read encoder position
 encoderPos = myEnc.read();
 float position = encoderPos * DEGREES_PER_PULSE;

 // Update tension or length setting based on encoder input
 if (lastEncoderPos != encoderPos) {
 lastEncoderPos = encoderPos;
 if (!mode) {
 setTension = position;
 lcd.setCursor(0, 1);
 lcd.print("Tension: ");
 lcd.print(setTension);
 lcd.print(" ");
 Serial.print("Set Tension: ");
 Serial.println(setTension);
 } else {
 setRopeLength = position;
 lcd.setCursor(0, 1);
 lcd.print("Length: ");
6
 lcd.print(setRopeLength);
 lcd.print(" ");
 Serial.print("Set Length: ");
 Serial.println(setRopeLength);
 }
 }
 // Handle encoder button press with debouncing
 int buttonState = digitalRead(ENCODER_SW_PIN);
 if (buttonState == LOW && (millis() - lastDebounceTime) > debounceDelay) {
 lastDebounceTime = millis();
 if (!buttonHeld) {
 buttonPressTime = millis();
 buttonHeld = true;
 }
 } else if (buttonState == HIGH && buttonHeld) {
 buttonHeld = false;
 unsigned long pressDuration = millis() - buttonPressTime;
 if (pressDuration < LONG_PRESS_DURATION) {
 if (!running) { // Check if the system is not already running
 startSystem(); // Start system for short press
 }
 } else {
 mode = !mode; // Toggle mode for long press
 tent = (weight / 1000) * 10;
7
 lcd.clear();
 lcd.setCursor(0, 0);
 if (!mode) {
 lcd.print("Set Tension: ");
 Serial.println("Mode: Set Tension");
 } else {
 lcd.print("Set Length: ");
 Serial.println("Mode: Set Length");
 }
 }
 }
 // Perform system operation if running
 if (running) {
 // Update weight and tension
 updateWeightAndSerial();
 updateLCD();
 tent = (weight / 1000) * 10;
 // Check if desired tension is reached
 if (tent >= setTension) {
 stopAll(); // Stop all operations
 Serial.println("Tension reached, stopping...");
 }
 // Check if desired rope length is reached
8
 if (ropeLength >= setRopeLength) {
 stopAll();
 // Stop all operations
 Serial.println("Length reached, stopping...");
 } else {
 stepper.run(); // Continue running stepper motor
 }
 lcd.setCursor(0, 1);
 lcd.print("Length: ");
 lcd.print(ropeLength);
 lcd.print(" ");
 Serial.print("Current Length: ");
 Serial.println(ropeLength);
 }
}
// Function to start system operation
void startSystem() {
 weight = scale.get_units(10);
 tent = (weight / 1000) * 10;
 running = true; // Set running flag to true
 ropeLength = 0; // Reset rope length
 stepper.setCurrentPosition(0); // Reset stepper motor position
9
 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("Running...");
 analogWrite(BTS7960_LPWM_PIN, motorSpeed); // Set the PWM value for BTS7960 LPWM
 analogWrite(BTS7960_RPWM_PIN, motorSpeed); // Set the PWM value for BTS7960 RPWM
 Serial.print("Motor Speed: ");
 Serial.println(motorSpeed);
 // Start stepper motor first
 for (int i = 0; i < 1000; i++) {
 int btn_state = analogRead(btn);
 Serial.println(btn_state);
 if(btn_state == 1023){
 stopAll();
 break;
 }
 if (tent >= setTension) {
 stopAll(); // Stop all operations
 Serial.println("Tension reached, stopping...");
 break;
 }
 digitalWrite(NEMA_DIR_PIN, HIGH);
 digitalWrite(NEMA_STEP_PIN, HIGH);
 delayMicroseconds(1000); // Added delay
 digitalWrite(NEMA_STEP_PIN, LOW);
10
 delayMicroseconds(1000); // Added delay
 }
 delay(10); // Wait for 1 second before starting the servo motor
 // Then start the servo motor
 a = 0;
 while (a < 2) {
 tent = (weight / 1000) * 10;
 int btn_state = analogRead(btn);
 if(btn_state == 1023){
 stopAll();
 break;
 }
 if (tent >= setTension) {
 stopAll(); // Stop all operations
 Serial.println("Tension reached, stopping...");
 break;
 }
 updateWeightAndSerial(); // Update weight reading and Serial output
 updateLCD(); // Update the LCD display
 adjustCalibration(); // Adjust calibration factor based on Serial input

 delay(100); // Delay for stability
 for (servo_motor_angle = 45; servo_motor_angle <= 130; servo_motor_angle++) {
11
 int btn_state = analogRead(btn);
 if(btn_state == 1023){
 stopAll();
 break;
 }
 if (tent >= setTension) {
 stopAll(); // Stop all operations
 Serial.println("Tension reached, stopping...");
 break;
 }
 delay(150);
 myServo.write(servo_motor_angle);
 updateWeightAndSerial(); // Update weight reading and Serial output
 updateLCD(); // Update the LCD display
 adjustCalibration(); // Adjust calibration factor based on Serial input
 lcd.setCursor(0, 1);
 lcd.print("Length: ");
 lcd.print(ropeLength);
 lcd.print(" m");
 for (int i = 0; i < 1000; i++) {
 int btn_state = analogRead(btn);
 if(btn_state == 1023){
 stopAll();
 break;
 }
 if (tent >= setTension) {
12
 stopAll(); // Stop all operations
 Serial.println("Tension reached, stopping...");
 break;
 }
 digitalWrite(NEMA_DIR_PIN, HIGH);
 digitalWrite(NEMA_STEP_PIN, HIGH);
 delayMicroseconds(1000); // Added delay
 digitalWrite(NEMA_STEP_PIN, LOW);
 delayMicroseconds(1000); // Added delay

 }
 servo_motor_angle+=10;
 myServo.write(servo_motor_angle);
 }
 updateWeightAndSerial(); // Update weight reading and Serial output
 updateLCD(); // Update the LCD display
 adjustCalibration(); // Adjust calibration factor based on Serial input

 delay(100); // Delay for stability
 for (servo_motor_angle = 130; servo_motor_angle >= 45; servo_motor_angle--) {
 int btn_state = analogRead(btn);
 if(btn_state == 1023){
 stopAll();
 break;
 }
 if (tent >= setTension) {
 stopAll(); // Stop all operations
13
 Serial.println("Tension reached, stopping...");
 break;
 }
 delay(150);
 myServo.write(servo_motor_angle);
 updateWeightAndSerial(); // Update weight reading and Serial output
 updateLCD(); // Update the LCD display
 adjustCalibration(); // Adjust calibration factor based on Serial input
 lcd.setCursor(0, 1);
 lcd.print("Length: ");
 lcd.print(ropeLength);
 lcd.print(" m");
 for (int i = 0; i < 1000; i++) {
 int btn_state = analogRead(btn);
 if(btn_state == 1023){
 stopAll();
 break;
 }
 if (tent >= setTension) {
 stopAll(); // Stop all operations
 Serial.println("Tension reached, stopping...");
 break;
 }
 digitalWrite(NEMA_DIR_PIN, HIGH);
 digitalWrite(NEMA_STEP_PIN, HIGH);
 delayMicroseconds(1000); // Added delay
14
 digitalWrite(NEMA_STEP_PIN, LOW);
 delayMicroseconds(1000); // Added delay
 }
 servo_motor_angle-=10;
 myServo.write(servo_motor_angle);
 }
 ropeLength +=2;
 a = a + 1;
 }
 lcd.setCursor(0, 1);
 lcd.print("Length: ");
 lcd.print(ropeLength);
 lcd.print("m");
}
// Function to stop all operations
void stopAll() {
 running = false; // Set running flag to false
 analogWrite(BTS7960_LPWM_PIN, 0); // Deactivate BTS7960 LPWM
 analogWrite(BTS7960_RPWM_PIN, 0); // Deactivate BTS7960 RPWM
 stepper.stop(); // Stop stepper motor
 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("Stopped");
 lcd.setCursor(0, 1);
 lcd.print("Length: ");
15
 lcd.print(ropeLength);
 Serial.println("System Stopped");
 weight = scale.get_units(10);
 tent = (weight / 1000) * 10;
}
void updateWeightAndSerial() {
 scale.set_scale(calibration_factor);
 weight = scale.get_units(10);
 Serial.print("Reading: ");
 Serial.print(weight);
 Serial.print(" g\tCalibration Factor: ");
 Serial.println(calibration_factor);
}
// Function to update LCD display
void updateLCD() {
 scale.set_scale(calibration_factor);
 weight = scale.get_units(10);
 tent = (weight / 1000) * 10;

 lcd.setCursor(0, 0);
 lcd.print("Tension: ");
 lcd.print(tent, 1); // Convert grams to Newtons
 lcd.print("N");
}
16
// Function to adjust calibration factor based on Serial input
void adjustCalibration() {
 if (Serial.available()) {
 char temp = Serial.read();
 if (temp == '+') {
 calibration_factor += 10;
 } else if (temp == '-') {
 calibration_factor -= 10;
 }
 }
}