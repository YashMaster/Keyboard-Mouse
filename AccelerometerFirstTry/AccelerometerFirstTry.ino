// MPU-6050 Short Example Sketch
// By Arduino User JohnChi
// August 17, 2014
// Public Domain
#include<Wire.h>
#include <RunningAverage.h>
#include <Mouse.h>
const int MPU_addr=0x68;  // I2C address of the MPU-6050

int16_t AcX,AcY,AcZ,Tmp,GyX,GyY,GyZ;

RunningAverage XRA;
RunningAverage YRA;
RunningAverage ZRA;



void setup(){
  Mouse.begin();
  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);  // PWR_MGMT_1 register
  Wire.write(0);     // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);
  Wire.beginTransmission(0b1101000); //I2C address of the MPU
  Wire.write(0x1C); //Accessing the register 1C - Acccelerometer Configuration (Sec. 4.5)
  Wire.write(0b00010000); //Setting the accel to +/- 2g
  Wire.endTransmission();
  Serial.begin(115200);
}

const int minMove = 100, divisor = 50;

static inline int8_t sign(int val) {
 if (val < 0) return -1;
 if (val==0) return 0;
 return 1;
}

inline int simplifyInt(int in){
  if(abs(in) < minMove){
    return 0;
  }
  return in / divisor;
}

void moveMouse(int16_t xAccel, int16_t yAccel){
  if(abs(xAccel) < minMove && abs(yAccel) < minMove){
    return;
  }
  int xVal = simplifyInt(xAccel);
  int yVal = simplifyInt(yAccel);
  Mouse.move(xVal, yVal, 0);
  Serial.println("INPUT - X: " + String(xAccel) + "  Y: " + String(yAccel));
  Serial.println("X: " + String(xVal) + "  Y: " + String(yVal));
}

long xSum = 0, ySum = 0, zSum = 0, lastResetTime = 0;
void loop(){
  if(micros() - lastResetTime > 1000){
    xSum = ySum = zSum = 0;
    lastResetTime = micros();
  }
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  
  Wire.requestFrom(MPU_addr,6,true);  // request a total of 14 registers
  AcX=Wire.read()<<8|Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)    
  AcY=Wire.read()<<8|Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  AcZ=Wire.read()<<8|Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)

  
//  Serial.print("AcX = "); Serial.print(AcX - XRA.getRunningAverage());
//  Serial.print(" | AcY = "); Serial.print(AcY - YRA.getRunningAverage());
//  Serial.print(" | AcZ = "); Serial.print(AcZ - ZRA.getRunningAverage());

  xSum += (AcX - XRA.getRunningAverage());
  ySum += (AcY - YRA.getRunningAverage());
  zSum += (AcZ - ZRA.getRunningAverage());
//
//  Serial.print("XSUM: "); Serial.print(xSum );
//  Serial.print(" | YSUM: "); Serial.print(ySum);
//  Serial.print(" | ZSUM: "); Serial.println(zSum);

  moveMouse(xSum, ySum);
  

  XRA.nextVal(AcX);
  YRA.nextVal(AcY);
  ZRA.nextVal(AcZ);

  delay(10);
}
