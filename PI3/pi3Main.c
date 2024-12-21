#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define VCC 5.0          // 전압
#define R1 1000.0        // 저항값
#define SPI_DEVICE "/dev/spidev0.0" // MCP3008
#define SPI_SPEED 500000 // SPI 속도 (500kHz)
#define PRESSURE_THRESHOLD 2.0  // 압력 감지 임계값 (전압 2.0V 이상이면 압력 감지)
#define INVALID_THRESHOLD 1020  // 채널의 부동 상태 확인 임계값 (예: 1020 이상)


void error_handling(char *message) {
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}


// SPI 초기화 함수
int spi_init(const char *device) {
    int spi_fd = open(device, O_RDWR);
    if (spi_fd < 0) {
        perror("SPI 장치 열기 실패");
        return -1;
    }

    u_int8_t mode = SPI_MODE_0;
    u_int8_t bits = 8;
    u_int32_t speed = SPI_SPEED;

    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) == -1 ||
        ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1 ||
        ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1) {
        perror("SPI 설정 실패");
        close(spi_fd);
        return -1;
    }

    return spi_fd;
}

// MCP3008에서 특정 채널의 데이터를 읽는 함수
int read_channel(int spi_fd, int channel) {
    u_int8_t tx[3], rx[3];
    tx[0] = 1;                       // Start bit
    tx[1] = (8 + channel) << 4;      // SGL/DIFF + D2~D0
    tx[2] = 0;                       // Don't care

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = 3,
        .speed_hz = SPI_SPEED,
        .bits_per_word = 8,
    };

    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 1) {
        perror("SPI 데이터 전송 실패");
        return -1;
    }

    int value = ((rx[1] & 3) << 8) + rx[2]; // 10-bit 데이터 추출

    // 부동 상태(임의 값)를 확인하고 유효하지 않으면 -1 반환
    if (value >= INVALID_THRESHOLD || value <= 0) {
        return -1; // 유효하지 않은 값
    }
    return value;
}

// 저항 계산 함수
double calculate_resistance(double voltage) {
    return (R1 * VCC / voltage) - R1;
}

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in serv_addr;
    char msg[2];
    char on[2] = "1";
    char off[2] = "2";
    int str_len;

  sock = socket(PF_INET, SOCK_STREAM, 0);
  if (sock == -1) error_handling("socket() error");

  memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("192.168.0.5");
    serv_addr.sin_port = htons(9999);

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    error_handling("connect() error");

  printf("Connection established\n");
    
    int spi_fd = spi_init(SPI_DEVICE);
    if (spi_fd < 0) {
        return 1;
    }

    FILE *file = fopen("fsr402.dat", "w");
    if (!file) {
        perror("파일 열기 실패");
        close(spi_fd);
        return 1;
    }

    printf("FSR402 데이터 수집 시작...\n");

    int index = 0;
    double delay = 1.0; // 측정 간격 (초)
    
    int sensor1_count = 0;
    int sensor2_count = 0;
    int total_samples = 0;
    double sensor1_per = 0;
    double sensor2_per = 0;
    char result_string[100] = "No valid samples were collected.";
    
    int is_on = 0;
    
    while (1) {
        
        str_len = read(sock, msg, sizeof(msg));
        if (str_len == -1) error_handling("read() error");

        printf("Receive message from Server : %s\n", msg);
        if (is_on ==1 || strncmp(on, msg, 1) == 0)
            is_on = 1;
        else
            continue;
        
        if (strncmp(off, msg, 1) == 0 && is_on == 1){
            break;
        }
            
        // 채널 0 (Sensor 1)
        int analog_value_1 = read_channel(spi_fd, 0);
        printf("222 : %d\n", analog_value_1);
        if (analog_value_1 == -1) {
            printf("Sensor 1 -> No valid data (floating state detected)\n");
        } else {
            double voltage_1 = analog_value_1 * VCC / 1024.0;
            double resistance_1 = calculate_resistance(voltage_1);
            printf("Sensor 1 -> Digital: %d, Voltage: %.3f V, R(K Ohm): %.3f\n", 
                   analog_value_1, voltage_1, resistance_1 / 1000.0);

            fprintf(file, "%d %d %.3f %.3f ", 
                    index, analog_value_1, voltage_1, resistance_1 / 1000.0);
        }
        
         

        // 채널 7 (Sensor 2)
        int analog_value_2 = read_channel(spi_fd, 7);
        printf("222 : %d\n", analog_value_2);
        if (analog_value_2 == -1) {
            printf("Sensor 2 -> No valid data (floating state detected)\n");
        } else {
            analog_value_2 *= 2.8;
            double voltage_2 = analog_value_2 * VCC / 1024.0;
            double resistance_2 = calculate_resistance(voltage_2);
            printf("Sensor 2 -> Digital: %d, Voltage: %.3f V, R(K Ohm): %.3f\n", 
                   analog_value_2, voltage_2, resistance_2 / 1000.0);

            fprintf(file, "%d %.3f %.3f\n", 
                    analog_value_2, voltage_2, resistance_2 / 1000.0);
        }
        
        if (analog_value_1 != -1 || analog_value_2 != -1){
            if ((analog_value_1 != -1 && analog_value_2 == -1) || analog_value_1 > analog_value_2){
                sensor1_count++;
            } else if ((analog_value_1 == -1 && analog_value_2 != -1) || analog_value_2 > analog_value_1) {
                sensor2_count++;
            }
            total_samples++;
        }
        

        usleep((int)(delay * 1000000)); // 딜레이
        index++;
        
        
    }
    if (total_samples > 0){
        sensor1_per = sensor1_count / (double)total_samples * 100.0;
        sensor2_per = sensor2_count / (double)total_samples * 100.0;
        
        snprintf(result_string, sizeof(result_string),
            "Left %.2f%% , Right %.2f%%.",
                 sensor1_per, sensor2_per);
        printf("%s\n", result_string);
    } else{
        printf("%s\n", result_string);
    }
    if (write(sock, result_string, strlen(result_string)) == -1) {
        perror("write() error");
    } else {
        printf("Result sent to server: %s\n", result_string);
    }
    
    
    
    close(sock);
    
    fclose(file);
    close(spi_fd);
    return 0;
}
    
    
