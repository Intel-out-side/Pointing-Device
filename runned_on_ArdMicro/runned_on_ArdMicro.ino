#include <Mouse.h>
#include <Wire.h>
#include <LiquidCrystal.h>

#define tx 1
#define rx 0

int16_t axRaw, ayRaw, azRaw, gxRaw, gyRaw, gzRaw, temperature; //raw data given by the sensor
unsigned long t; //the time since the beginning of the loop() function
float kx = 0.2; //gain for x
float ky = 0.2; //gain for y
char mode = 0; //mode of control type
int move_x; //relative distance to move in X-axis
int move_y; //relative distance to move in Y-axis
float C = 80; // constant to normalize
float acc_x, acc_y, acc_z; //raw data from the sensor
float acc_angX, acc_angY, acc_angZ; //calculated angles
unsigned long dt; //duration for a single loop
int s; // number of times the user pushed the switch 1
int f; // number of times the user pushed the switch 2
const float lmg = 0.12 * 66 * 9.6 / 100 * 9.81; //estimated rigidity of the human head
float Xoffset, Yoffset;

#define RAD_TO_DEG 180/PI
#define DEG_TO_RAD PI/180

#define IndicateMode 9

#define IndicateLow 10
#define IndicateMid 11
#define IndicateHigh 12

void setup() {
  // put your setup code here, to run once:
  Wire.begin();
  TWBR = 12;

  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0X00);
  Wire.endTransmission();

  Wire.beginTransmission(0x68);
  Wire.write(0x1C);
  Wire.write(0x10);
  Wire.endTransmission();

  Wire.beginTransmission(0x68);
  Wire.write(0x1B);
  Wire.write(0x08);
  Wire.endTransmission();

  Wire.beginTransmission(0x68);
  Wire.write(0x1A);
  Wire.write(0x05);
  Wire.endTransmission();

  Mouse.begin();
  Serial.begin(19200);
  calibrate();

  //Initialize the all LED
  pinMode(IndicateMode, OUTPUT);
  pinMode(IndicateLow, OUTPUT);
  pinMode(IndicateMid, OUTPUT);
  pinMode(IndicateHigh, OUTPUT);
  digitalWrite(IndicateMode, HIGH);
  digitalWrite(IndicateLow, HIGH);
  digitalWrite(IndicateMid, HIGH);
  digitalWrite(IndicateHigh, HIGH);
  delay(1000);
  digitalWrite(IndicateMode, LOW);
  digitalWrite(IndicateLow, LOW);
  digitalWrite(IndicateMid, LOW);
  digitalWrite(IndicateHigh, LOW);
}

void loop() {
  // put your main code here, to run repeatedly:
  t = millis();
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);
  Wire.endTransmission();
  Wire.requestFrom(0x68, 14);
  while (Wire.available() < 14);
  axRaw = Wire.read() << 8 | Wire.read();
  ayRaw = Wire.read() << 8 | Wire.read();
  azRaw = Wire.read() << 8 | Wire.read();

  acc_x = axRaw / 16384.0; //convert raw data into m/s^2
  acc_y = ayRaw / 16384.0;
  acc_z = azRaw / 16384.0;

  acc_angX = atan2(acc_y, acc_x) * 360 / 2.0 / PI; //calculatete the tilt angle about X-axis
  //Serial.print(acc_angX); Serial.print(",");
  acc_angY = atan2(acc_z, acc_x) * 360 / 2.0 / PI; //calculate the tilt angle about Y-axis
  //Serial.println(acc_angY);

  if (digitalRead(4) == LOW) { //if the left switch is pushed then s+=1
    s += 1;
    if (s > 2) s = 0;
    delay(100);
    while (digitalRead(4) == LOW) {}
    twinkle(1);
  }

  if (digitalRead(5) == LOW) { //if the right switch is pushed then f+=1
    f += 1;
    if (f > 2) f = 0;
    delay(100);
    while (digitalRead(5) == LOW) {}
    twinkle(2);
    calibrate();
  }
  distinguisher();
  sensitivity_changer();
  Mouse.move(move_x, move_y, 0); // Move the cursor according to the tilt angles
  dt = (double) (millis() - t);

  if (dt < 20) delay(20 - dt);
}

//1: distinguisher
//2: sensitivity_changer
void twinkle(int func_name) {
  if (func_name == 1) {
    int n = IndicateMode; //light LED1 on
    int rep_num =  s;
    Serial.println(n);
    for (int i = 0; i <= s; i++) {
      digitalWrite(n, HIGH);
      delay(100);
      digitalWrite(n, LOW);
      delay(100);
    }
  }

  if (func_name == 2) {
    int rep_num = f;
    if (f == 0) {
      digitalWrite(IndicateLow, HIGH);
      delay(1000);
      digitalWrite(IndicateLow, LOW);
    }
    if (f == 1) {
      digitalWrite(IndicateMid, HIGH);
      delay(1000);
      digitalWrite(IndicateMid, LOW);
    }
    if (f == 2) {
      digitalWrite(IndicateHigh, HIGH);
      delay(1000);
      digitalWrite(IndicateHigh, LOW);
    }
  }
}

void distinguisher() {
  float X = acc_angY - Yoffset;
  float Y = acc_angX - Xoffset;
  //this is reversed because the sensor is on its side
  if (s == 0) { //OFF MODE
    move_x = 0;
    move_y = 0;
  }

  if (s == 1) { //Linear Mode
    move_x = (int) (kx *  X / 40);
    move_y = (int) (ky * -Y / 40);
  }

  if (s == 2) { //non-linear mode
    if (0 <= X && X < 40) {
      move_x = (int) (kx * 100 / 8 * (1 - sin(X * DEG_TO_RAD) / (X * DEG_TO_RAD)));
    } else if (-40 < X && X < 0) {
      move_x = (int) (-kx * 100 / 8 * (1 - sin(X * DEG_TO_RAD) / (X * DEG_TO_RAD)));
    }

    if (0 <= Y && Y< 40) {
      move_y = (int) (-ky * 100 / 8 * (1 - sin(Y * DEG_TO_RAD) / (Y * DEG_TO_RAD)));
    } else if (-40 < Y && Y< 0) {
      move_y = (int) (ky * 100 / 8 * (1 - sin(Y * DEG_TO_RAD) / (Y * DEG_TO_RAD)));
    }
  }
  Serial.print(X); Serial.print(';'); Serial.println(-Y);
}

void calibrate() {
  float Xcalib, Ycalib;
  for (int i = 0; i < 100; i++) {
    Wire.beginTransmission(0x68);
    Wire.write(0x3B);
    Wire.endTransmission();
    Wire.requestFrom(0x68, 14);
    while (Wire.available() < 14);
    axRaw = Wire.read() << 8 | Wire.read();
    ayRaw = Wire.read() << 8 | Wire.read();
    azRaw = Wire.read() << 8 | Wire.read();

    acc_x = axRaw / 16384.0; //convert raw data into m/s^2
    acc_y = ayRaw / 16384.0;
    acc_z = azRaw / 16384.0;

    Xcalib += atan2(acc_y, acc_x) * 360 / 2.0 / PI; //calculatete the tilt angle about X-axis
    //Serial.print(acc_angX); Serial.print(",");
    Ycalib += atan2(acc_z, acc_x) * 360 / 2.0 / PI; //calculate the tilt angle about Y-axis
  }
  Xoffset = Xcalib/100;
  Yoffset = Ycalib/100; //an average value for 100 iteration
  
}

void sensitivity_changer() {
  if (f == 0) {
    kx = 15; ky = 15;
    delay(5);
  }
  if (f == 1) {
    kx = 20; ky = 20;
    delay(5);
  }
  if (f == 2) {
    kx = 25; ky = 25;
    delay(5);
  }
}
