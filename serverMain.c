#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_MAX 3
#define DIRECTION_MAX 256
#define VALUE_MAX 256

#define IN 0
#define OUT 1

#define LOW 0
#define HIGH 1

#define PIN 20
#define POUT 21

int isSit = 0;
int pi4 = 0;
long write_time_long = 0;

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

// pi3&pi4 처리 스레드
void *pi3_thread_func(void){
  char pi3 = '1';
  char threshold = '0';
  //pi3 작동(소켓 값이 1이면 작동)
  while(1){
    write(clnt_sock[0], pi3, 1);
    //pi3로부터 압력 threshold 넘었는지 확인(넘었으면 pi3에서 소켓으로 1 전송)
    read(clnt_sock[0], threshold, 1);
    // 문자열 -> 정수형
    isSit = threshold - '0';
  }
  return NULL;
}
void *pi3_thread_end_func(void){
  char *buf = buf[100000];//버퍼 크기로 적당한 값으로 변경하기
  char *message = (char *)malloc(50);  // 동적 메모리 할당
  read(clnt_sock[0], buf, sizeof(buf));
  strcpy(message, buf);  // 문자열 복사
  pthread_exit((void *)message);     
  //여기다 반환
}
void *pi4_thread_end_func(void) {
  char pi4_end = '2';
  char write_time[6];
  //pi4 중지(소켓 값이 0이면 중지) & 필기시간(write_time) 요청
  write(clnt_sock[1], pi4_end, 1);
  read(clnt_sock[1], write_time, 1);
  write_time_long = strtol(write_time, NULL , 10);
  //여기다 반환
  return NULL;
} 

int main(int argc, char *argv[]) {
  int state = 1;
  int prev_state = 1;
  int serv_sock = -1; //얘도 고쳐야 하나?
  int clnt_sock[2] = {0};
  int client_count = 0;
  struct sockaddr_in serv_addr, clnt_addr;
  socklen_t clnt_addr_size;

  // Enable GPIO pins
  if (-1 == GPIOExport(PIN) || -1 == GPIOExport(POUT)) return (1);

  // Set GPIO directions
  if (-1 == GPIODirection(PIN, IN) || -1 == GPIODirection(POUT, OUT))
    return (2);

  if (-1 == GPIOWrite(POUT, 1)) return (3);

  // 서버 소켓 생성
  serv_sock = socket(PF_INET, SOCK_STREAM, 0);
  if (serv_sock == -1) error_handling("socket() error");

  // 서버 주소 설정
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  inet_pton(AF_INET, "192.168.0.1", &serv_addr.sin_addr);
  //serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(9999); //직접 설정
  

  // 소켓과 주소 바인딩
  if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    error_handling("bind() error");

  // 연결 대기 상태
  if (listen(serv_sock, 5) == -1) error_handling("listen() error");

  // 클라이언트 연결
  clnt_addr_size = sizeof(clnt_addr);
  for (int i = 0; i < 2; i++) {
    clnt_sock[i] = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);
    if (clnt_sock[i] == -1) {
        perror("accept() error");
        continue;
    }

      printf("Connected to client %d: %s:%d\n", i + 1,
              inet_ntoa(clnt_addr.sin_addr), ntohs(clnt_addr.sin_port));
      client_count++;
  }

  // 메인 로직
  int pi3_start = 0;
  int pi4_unstart = 1;
  pthread_t pi3_thread;
  pthread_t pi3_thread_end;
  pthread_t pi4_thread_end;
  void *pressure_data;
  void *video_data;
  char pi4 = '0';
  char *pi3_data;

  while (1) {

    state = GPIORead(PIN);

    if (prev_state == 0 && state == 1) { //버튼이 눌렸을 때
      //공부 시작
      //pi3 시작
      pthread_create(&pi3_thread_func, NULL, pi3_thread_func, NULL);

      if(pi3_start){ //공부 완전 종료
        //pi3 종료
        pthread_cancel(pi3_thread_func);
        pthread_join(pi3_thread_func, NULL);
        //pi3로부터 데이터 가져오기
        pthread_create(&pi3_thread_end, NULL, pi3_thread_end_func, *pi3_data);
        pthread_join(pi4_thread_end, NULL);
        //pi4로부터 필기 시간, 타임랩스 가져오기
        pthread_create(&pi4_thread_end, NULL, pi4_thread_end_func, NULL);
        pthread_join(pi4_thread_end);

        int hours = write_time_long / 3600;
        int minutes = (write_time_long % 3600) / 60;
        int secs = write_time_long % 60;
        printf("pi3_data: %s\n", pi3_data);// 공부자세? 출력
        printf("공부시간: %02d:%02d:%02d (hh:mm:ss)\n",hours, minutes, secs); //필기시간 출력
        printf(); //동영상 출력

        break;
      }
      pi3_start = 1;
    }
       
    if(isSit && pi4_unstart){ //압력 센서 임계값 넘었을 때
      //pi4 실행
      pi4 = '1';
      write(clnt_sock[1], pi4, 1);
      pi4_unstart = 0;
    }
    if((!isSit) && !(pi4_unstart)){//pi4 휴식
      pi4 = '0';
      write(clnt_sock[1], pi4, 1);
      pi4_unstart = 1;
    }    
    prev_state = state;
    usleep(500 * 100);
  }
    
  close(clnt_sock[0]);
  close(clnt_sock[1]);
  close(serv_sock);

  return (0);
}