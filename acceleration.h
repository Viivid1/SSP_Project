#ifndef ACCELERATION_H
#define ACCELERATION_H

extern double studyTime;

int I2CInit();
void initSensor();
short readAccelData(int reg);
void readAccelXYZ(float *x, float *y, float *z);
float calculateMagnitude(float x, float y, float z);
int accelerationMain();

#endif

/*
int I2CInit();
void initSensor();
int accelerationMain(); 순으로 실행시키면 됨
*/
