#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "acceleration.h"
#include "camera.h"

#define BUF_SIZE 1024



int writeTime = 0;
float pre_magnitude = 0.0;

void send_file(int sock, const char *file_path) {
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        perror("File open error");
        return;
    }

    char buffer[BUF_SIZE];
    int bytes_read;

    while ((bytes_read = fread(buffer, 1, BUF_SIZE, fp)) > 0) {
        if (send(sock, buffer, bytes_read, 0) == -1) {
            perror("File send error");
            fclose(fp);
            return;
        }
    }

    fclose(fp);
    printf("File sent: %s\n", file_path);
}

void *acceleration_thread_func(void *arg) {
    while(1){
        pre_magnitude = accelerationMain(pre_magnitude); // 가속도 센서 실행
        sleep(1);
    }
    return NULL;
}

void *camera_thread_func(void *arg) {
    while (1) {
        cameraMain(); // 카메라 관련 작업
        sleep(5); // 1초 대기
    }
    return NULL;
}

int main(void) {
    int sock;
    int isStart = 0;
    struct sockaddr_in serv_addr;
    char send_buf[64];  // 서버로 보낼 데이터 버퍼
    char recv_buf[1]; // 서버로부터 받은 데이터 버퍼
    pthread_t camera_thread, acceleration_thread;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket() error");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("192.168.0.5");
    serv_addr.sin_port = htons(9999);

    // 서버에 연결
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("connect() error");
        close(sock);
        exit(1);
    }

    printf("Connected to server.\n");

    memset(send_buf, 0, sizeof(send_buf)); // 버퍼 초기화
    
    //가속도 센서 초기화
    if (I2CInit() == -1) {
        return -1;
    }
    initSensor(); 
    
    while (1) {
        int read_bytes = read(sock, recv_buf, sizeof(recv_buf)); // 서버로부터 데이터 받기
        char isSit = recv_buf[0];
        

        if (read_bytes) {
            printf("Received from server: %s\n", recv_buf);
            if(isSit=='1' && isStart==0){ // pi4 실행
                pthread_create(&acceleration_thread, NULL, acceleration_thread_func, NULL);
                pthread_create(&camera_thread, NULL, camera_thread_func, NULL);
                cameraMain(); //카메라 센서 실행
                isStart = 1;
                break;
            }
            else if(isSit=='0' && isStart==1){ //휴식
                pthread_cancel(acceleration_thread);
                pthread_join(acceleration_thread, NULL);
                pthread_cancel(camera_thread);
                pthread_join(camera_thread, NULL);
                isStart = 0;
                break;
            }
            // 공부 종료, 데이터 전송
            else if(isSit=='2'){
                //필기 종료
                pthread_cancel(acceleration_thread);
                pthread_join(acceleration_thread, NULL);
                // 필기시간 정수형을 문자열로 변환
                sprintf(send_buf, "%d", writeTime);
                // 필기시간 전송
                if (write(sock, send_buf, strlen(send_buf)) == -1) {
                    perror("write() error");
                    exit(1);
                }

                sleep(1); //서버에서 필기시간간 수신하도록 대기기

                //촬영 종료
                pthread_cancel(camera_thread);
                pthread_join(camera_thread, NULL);
                // 사진파일 전송
                const char *zip_name = "photos.zip";
                create_photo_zip(zip_name); //사진 압축
                send_file(sock, zip_name); //압축 파일 전송
                close(sock);
                return 0;
            }
        } else if (read_bytes == 0) {
            printf("Server closed the connection.\n");
            break;
        } else {
            perror("read() error");
            break;
        }
        usleep(100000);  // 0.1s 대기        
    }
    return 0;
}