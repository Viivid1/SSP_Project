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

int main(void) {
    int sock;
    int isStart = 0;
    struct sockaddr_in serv_addr;
    char send_buf[64];  // 서버로 보낼 데이터 버퍼
    char recv_buf[1]; // 서버로부터 받은 데이터 버퍼
    

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket() error");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(9999);

    // 서버에 연결
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("connect() error");
        close(sock);
        exit(1);
    }

    printf("Connected to server.\n");

    while(1){
    memset(send_buf, 0, sizeof(send_buf)); // 버퍼 초기화
    int read_bytes = read(sock, recv_buf, sizeof(recv_buf)); // 서버로부터 데이터 받기
    printf("%s",recv_buf);
    usleep(100000);  // 0.1s 대기        
    }
    return 0;
}