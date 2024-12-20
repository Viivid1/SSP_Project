#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>

#define BUFFER_MAX 3
#define DIRECTION_MAX 256
#define VALUE_MAX 256

#define IN 0
#define OUT 1

#define LOW 0
#define HIGH 1

#define PIN 20
#define POUT 21

#define BUF_SIZE 1024 

int isSit = 0;
char cwd[PATH_MAX];
int clnt_sock;


void error_handling(char *message) {
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}
static int GPIOExport(int pin) {
  char buffer[BUFFER_MAX];
  ssize_t bytes_written;
  int fd;

  fd = open("/sys/class/gpio/export", O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open export for writing!\n");
    return (-1);
  }

  bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
  write(fd, buffer, bytes_written);
  close(fd);
  return (0);
}

static int GPIODirection(int pin, int dir) {
  static const char s_directions_str[] = "in\0out";

  char path[DIRECTION_MAX] = "/sys/class/gpio/gpio%d/direction";
  int fd;

  snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);

  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open gpio direction for writing!\n");
    return (-1);
  }

  if (-1 ==
      write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
    fprintf(stderr, "Failed to set direction!\n");
    return (-1);
  }

  close(fd);
  return (0);
}

static int GPIORead(int pin) {
  char path[VALUE_MAX];
  char value_str[3];
  int fd;

  snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
  fd = open(path, O_RDONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open gpio value for reading!\n");
    return (-1);
  }

  if (-1 == read(fd, value_str, 3)) {
    fprintf(stderr, "Failed to read value!\n");
    return (-1);
  }

  close(fd);

  return (atoi(value_str));
}

static int GPIOWrite(int pin, int value) {
  static const char s_values_str[] = "01";
  char path[VALUE_MAX];
  int fd;

  snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open gpio value for writing!\n");
    close(fd);
    return (-1);
  }

  if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
    fprintf(stderr, "Failed to write value!\n");
    close(fd);
    return (-1);
  }
  close(fd);
  return (0);
}


int main(int argc, char *argv[]) {
  int state = 1;
  int prev_state = 1;
  int serv_sock = -1; //얘도 고쳐야 하나?
  int client_count = 0;
  struct sockaddr_in serv_addr, clnt_addr;
  socklen_t clnt_addr_size;

  // Enable GPIO pins
  if (-1 == GPIOExport(PIN) || -1 == GPIOExport(POUT)) return (1);

  // Set GPIO directions
  if (-1 == GPIODirection(PIN, IN) || -1 == GPIODirection(POUT, OUT))
    return (2);

  

  // 서버 소켓 생성
  serv_sock = socket(PF_INET, SOCK_STREAM, 0);
  if (serv_sock == -1) error_handling("socket() error");

  // 서버 주소 설정
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
  //serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(9999); //직접 설정


  // 소켓과 주소 바인딩
  if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    error_handling("bind() error");

  // 연결 대기 상태
  if (listen(serv_sock, 5) == -1) error_handling("listen() error");

  // 클라이언트 연결
  clnt_addr_size = sizeof(clnt_addr);
  clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);
  printf("Connected to client: %s:%d\n",
              inet_ntoa(clnt_addr.sin_addr), ntohs(clnt_addr.sin_port));

  // 메인 로직
  int pi3_unstart = 1;
  int pi4_unstart = 1;
  pthread_t pi3_thread;
  pthread_t pi3_thread_end;
  pthread_t pi4_thread_end;
  char pi4[1];
  char *pi3_data;
  char *pi4_data;
  char write_time[6] = {0};  // 0~5 인덱스 저장
  char photo_zip[BUF_SIZE] = {0};  // 6부터 끝까지 저장

  
    pi4[0] = '1';
    write(clnt_sock, pi4, 1);
    usleep(100000);
    pi4[0] = '2';
    write(clnt_sock, pi4, 1);
    usleep(100000);
  
    
  close(clnt_sock);
  close(serv_sock);

  return (0);
}