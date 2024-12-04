int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in serv_addr;
    char msg[64];  // 서버로 보낼 데이터 버퍼
    float accel_x, accel_y, accel_z;

    if (argc != 3) {
        printf("Usage: %s <Server_IP> <port>\n", argv[0]);
        exit(1);
    }

    // I²C 센서 초기화
    if (I2CInit() == -1) {
        return -1;
    }
    initSensor();

    // 소켓 생성
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket() error");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    // 서버에 연결
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("connect() error");
        close(sock);
        exit(1);
    }

    printf("Connected to server.\n");

    while (1) {
        // 가속도 데이터 읽기
        readAccelXYZ(&accel_x, &accel_y, &accel_z);

        // 데이터를 문자열로 변환
        snprintf(msg, sizeof(msg), "X: %.2f, Y: %.2f, Z: %.2f\n", accel_x, accel_y, accel_z);

        // 서버로 데이터 전송
        if (write(sock, msg, strlen(msg)) == -1) {
            perror("write() error");
            close(sock);
            exit(1);
        }

        printf("Sent to server: %s", msg);

        usleep(500000);  // 500ms 대기
    }

    close(sock);
    close(i2c_fd);
    return 0;
}