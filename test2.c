#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>       // sqrt 사용을 위해 추가
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#define I2C_DEV "/dev/i2c-1"   // I²C 디바이스 파일
#define SENSOR_ADDR 0x68       // 가속도 센서 주소 (예: MPU6050)
#define ACCEL_XOUT_H 0x3B      // X축 데이터 레지스터 (MSB 시작)
#define ACCEL_YOUT_H 0x3D      // Y축 데이터 레지스터 (MSB 시작)
#define ACCEL_ZOUT_H 0x3F      // Z축 데이터 레지스터 (MSB 시작)
#define PWR_MGMT_1 0x6B        // 전원 관리 레지스터
#define SMPLRT_DIV 0x19        // 샘플링 속도 분주기
#define CONFIG 0x1A            // 설정 레지스터
#define GYRO_CONFIG 0x1B       // 자이로 설정 레지스터
#define ACCEL_CONFIG 0x1C      // 가속도 설정 레지스터
#define THRESHOLD 0.1        // 임계값 (m/s²의 차이)
#define SENSITIVITY 8192.0    // 가속도 센서의 감도 (1g = 16384)
int i2c_fd;
// I²C 초기화
int I2CInit() {
    if ((i2c_fd = open(I2C_DEV, O_RDWR)) < 0) {
        perror("Failed to open I2C device");
        return -1;
    }
    if (ioctl(i2c_fd, I2C_SLAVE, SENSOR_ADDR) < 0) {
        perror("Failed to connect to sensor");
        return -1;
    }
    return 0;
}
// 센서 초기화
void initSensor() {
    // 슬립 모드 해제
    unsigned char config[2];
    config[0] = PWR_MGMT_1;
    config[1] = 0x00;  // 슬립 모드 해제
    if (write(i2c_fd, config, 2) != 2) {
        perror("Failed to write to PWR_MGMT_1");
        exit(1);
    }
    // 샘플링 속도 설정
    config[0] = SMPLRT_DIV;
    config[1] = 0x07;  // 샘플링 분주기 설정 (1kHz 기준 125Hz)
    if (write(i2c_fd, config, 2) != 2) {
        perror("Failed to write to SMPLRT_DIV");
        exit(1);
    }
    // 설정 레지스터
    config[0] = CONFIG;
    config[1] = 0x06;  // 기본 설정
    if (write(i2c_fd, config, 2) != 2) {
        perror("Failed to write to CONFIG");
        exit(1);
    }
    // 가속도 설정
    config[0] = ACCEL_CONFIG;
    config[1] = 0x00;  // ±2g 범위 설정
    if (write(i2c_fd, config, 2) != 2) {
        perror("Failed to write to ACCEL_CONFIG");
        exit(1);
    }
}
// 가속도 센서에서 데이터 읽기
short readAccelData(int reg) {
    unsigned char buf[2];
    if (write(i2c_fd, &reg, 1) != 1) {
        perror("Failed to write to sensor");
        return -1;
    }
    if (read(i2c_fd, buf, 2) != 2) {
        perror("Failed to read from sensor");
        return -1;
    }
    return (buf[0] << 8) | buf[1];  // 두 바이트를 합쳐 16비트 데이터 생성
}
// X, Y, Z 축 가속도 읽기
void readAccelXYZ(float *x, float *y, float *z) {
    *x = (float)readAccelData(ACCEL_XOUT_H) / SENSITIVITY;
    *y = (float)readAccelData(ACCEL_YOUT_H) / SENSITIVITY;
    *z = (float)readAccelData(ACCEL_ZOUT_H) / SENSITIVITY;
}
// 가속도 벡터의 크기 계산
float calculateMagnitude(float x, float y, float z) {
    return sqrt(x * x + y * y + z * z);
}
int main() {
    if (I2CInit() == -1) {
        return -1;
    }
    initSensor();  // 센서 초기화 추가
    struct timeval start_t, end_t;
    double studyTime;
    int over_threshold = 0;  // 임계값 초과 상태 플래그
    float pre_magnitude = 0; // 이전 값
    while (1) {
        float accel_x, accel_y, accel_z;
        readAccelXYZ(&accel_x, &accel_y, &accel_z);  // X, Y, Z축 가속도 읽기
        float post_magnitude = calculateMagnitude(accel_x, accel_y, accel_z);  // 합성 가속도 계산
        // printf("Accel Magnitude: %.2f m/s² (X: %.2f, Y: %.2f, Z: %.2f)\n",
        //        post_magnitude, accel_x, accel_y, accel_z);
        printf("Movement:%f",post_magnitude-pre_magnitude);
        if (fabs(post_magnitude - pre_magnitude) > THRESHOLD) {
            if (!over_threshold) {  // 임계값 초과 시작
                gettimeofday(&start_t, NULL);
                over_threshold = 1;
                printf("Threshold exceeded, starting timer...\n");
            }
        } else {
            if (over_threshold) {  // 임계값 초과 종료
                gettimeofday(&end_t, NULL);
                studyTime = (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_usec - start_t.tv_usec) / 1e6;
                printf("Threshold exceeded for %.4f seconds\n", studyTime);
                over_threshold = 0;
            }
        }
        pre_magnitude = post_magnitude;
        usleep(1000000);  // 1000ms 대기
    }
    close(i2c_fd);  // I²C 디바이스 닫기
    return 0;
}