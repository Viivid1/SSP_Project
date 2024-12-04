#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "accleration.h"
#include "camera.h"

int writeTime;

int main(int argc, char *argv[]) {
    int sock;
    int isSit = 0;
    struct sockaddr_in serv_addr;
    char send_buf[64];  // 서버로 보낼 데이터 버퍼
    char recv_buf[64]; // 서버로부터 받은 데이터 버퍼
    char writeTime_buf[10];
    float accel_x, accel_y, accel_z;

    // if (argc != 3) {
    //     printf("Usage: %s <Server_IP> <port>\n", argv[0]);
    //     exit(1);
    // }

    // 소켓 생성
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket() error");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]); //argv[1]을 IP주소로 변환
    serv_addr.sin_port = htons(atoi(argv[2])); //argv[2]를 포트 번호로 변환

    // 서버에 연결
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("connect() error");
        close(sock);
        exit(1);
    }

    printf("Connected to server.\n");

    while (1) {
        memset(recv_buf, 0, sizeof(recv_buf)); // 버퍼 초기화
        int read_bytes = read(sock, recv_buf, sizeof(recv_buf) - 1); // 서버로부터 데이터 받기
        recv_buf[read_bytes] = '\0';
        
        if (read_bytes > 0) {
            printf("Received from server: %s\n", recv_buf);
            isSit = recv_buf[0] - '0'; 
            //writeTime 값 전달받기
            int index = 0;
            for (int i = 1; i < read_bytes; i++) { 
                writeTime_buf[index++] = recv_buf[i];
            }
            writeTime_buf[index] = '\0';
            writeTime = atoi(writeTime_buf);

            if(isSit==1){ // isSit==1일때는 정상 실행
                accelerationMain();
                cameraMain();
            }
            else{
                // 필기시간 정수형을 문자열로 변환
                sprintf(send_buf, "%d", writeTime);
                // 서버로 데이터 전송
                if (write(sock, send_buf, strlen(send_buf)) == -1) {
                    perror("write() error");
                    close(sock);
                    exit(1);
                }
            }
            if(isSit==2){ // isSit==2일때는 공부 완전 종료
                //camera 사진들 서버로 보내기
            }
        } else if (read_bytes == 0) {
            printf("Server closed the connection.\n");
            break;
        } else {
            perror("read() error");
            break;
        }

        sleep(1);  // 1s 대기        
    }
    close(sock);
    return 0;
}