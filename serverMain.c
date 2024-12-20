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
int clnt_sock[2];


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

// pi3 작동 스레드
void *pi3_thread_func(void *arg){
  char pi3[1] = {'1'};       
  char threshold[1] = {0};
  //pi3 작동(소켓 값이 1이면 작동)
  while(1){
    write(clnt_sock[0], pi3, 1);
    //pi3로부터 압력 임계점 넘었는지 확인(넘었으면 pi3에서 소켓으로 
    //1 전송해서 threshold에 저장)
    read(clnt_sock[0], threshold, 1);

    // 문자열 -> 정수형
    isSit = threshold[0] - '0';
  }
  return NULL;
}
void *pi3_thread_end_func(void *arg){
  char pi3[1] = {'2'};
  char buf[BUF_SIZE];
  char *message = (char *)malloc(BUF_SIZE);  // 동적 메모리 할당

  write(clnt_sock[0], pi3, 1); //pi4 중지 & pi3 data 요청
  int bytes_read = read(clnt_sock[0], buf, sizeof(buf)-1);
  buf[bytes_read] = '\0'; // null-terminate
  strcpy(message, buf);  // 문자열 복사
  pthread_exit((void *)message);     
  return NULL;
}
void *pi4_thread_end_func(void *arg) {
  char pi4[1] = {'2'};
  char buf[BUF_SIZE];
  char *message = (char *)malloc(BUF_SIZE);  // 동적 메모리 할당
  char full_path[256];

  long file_size;
  long total_received = 0;

  //write_time 받아오기
  write(clnt_sock[1], pi4, 1);
  usleep(100*100);
  int bytes_read = read(clnt_sock[1], buf, sizeof(buf)-1);
  buf[bytes_read] = '\0'; // null-terminate
  strcpy(message, buf);  // 문자열 복사
  

  //photos.zip 받아오기
  pi4[0] = '3';
  
  const char *folder_name = "study_photos";
  mkdir(folder_name, 0777);
  getcwd(cwd, sizeof(cwd));
  snprintf(full_path, sizeof(full_path), "%s/%s", cwd, folder_name);
  FILE *fp = fopen(full_path, "wb");
  write(clnt_sock[1], pi4, 1);
  usleep(100*100);
  recv(clnt_sock[1], &file_size, sizeof(file_size), 0);
  while (total_received < file_size) {
        int bytes_received = recv(clnt_sock[1], buf, BUF_SIZE, 0);
        if (bytes_received <= 0) {
            perror("File receive error");
            fclose(fp);
            return NULL;
        }
        mkdir(folder_name, 0777);
        fwrite(buf, 1, bytes_received, fp);
        total_received += bytes_received;
        printf("File received successfully: %s\n", full_path);
        
  }
  fclose(fp);
  pthread_exit((void *)message);
  return NULL;
} 

void time_diff_func(double difference, int time_diff[3]){
    time_diff[0] = (int)difference / 3600;          // 시간
    time_diff[1] = ((int)difference % 3600) / 60;   // 분
    time_diff[2] = (int)difference % 60;            // 초
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
  time_t start_study, end_study, start_sit, end_sit;
  double study_time = 0;
  double sit_time = 0;
  int time_diff[3];  

  while (1) {
    if (-1 == GPIOWrite(POUT, 1)) return (3);
    state = GPIORead(PIN);
    if (prev_state == 0 && state == 1) { //버튼이 눌렸을 때
      if(pi3_unstart) {//공부 시작
        pthread_create(&pi3_thread, NULL, pi3_thread_func, NULL);
        start_study = time(NULL);
      }
      else{ //공부 종료
        end_study = time(NULL);
        study_time = difftime(end_study, start_study);
        //pi3 종료
        printf("pi3 종료");
        pthread_cancel(pi3_thread);
        pthread_join(pi3_thread, NULL);
        //pi3로부터 데이터 가져오기
        pthread_create(&pi3_thread_end, NULL, pi3_thread_end_func, NULL);
        pthread_join(pi3_thread_end, (void **)&pi3_data);
        free(pi3_data);
        //pi4로부터 필기 시간, 사진 압축 파일 가져오기
        pthread_create(&pi4_thread_end, NULL, pi4_thread_end_func, NULL);
        pthread_join(pi4_thread_end, (void **)&pi4_data);
        //필기 시간, 사진 압축 파일 분리
        strncpy(write_time, pi4_data, 6);  // 0~5 인덱스 복사
        write_time[6] = '\0';             // null-terminate for safety
        double write_time_double = strtod(write_time, &endptr); //str -> double
        strcpy(photo_zip, pi4_data + 6);  // 6부터 끝까지 복사      
        free(pi4_data);  // 동적 메모리 해제
        
        printf("pi3_data: %s\n", pi3_data);
        time_diff_func(study_time, time_diff);
        printf("공부 시간: %02d:%02d:%02d");
        time_diff_func(sit_time, time_diff);
        printf("순공 시간(앉은 시간): %02d:%02d:%02d");
        time_diff_func(write_time_double, time_diff);
        printf("필기 시간: %02d:%02d:%02d (hh:mm:ss)\n",hours, minutes, secs);
        printf("photo_zip 저장 경로: %s\n", photo_zip);

        break;
      }
      
      pi3_unstart = 0;
    }
    
    if(isSit && pi4_unstart){ //압력 센서 임계값 넘었을 때
      //pi4 실행
      pi4[0] = '1';
      write(clnt_sock[1], pi4, 1);
      pi4_unstart = 0;
      start_sit = time(NULL);
    }
    if((!isSit) && !(pi4_unstart)){//pi4 휴식
      pi4[0] = '0';
      write(clnt_sock[1], pi4, 1);
      pi4_unstart = 1;
      end_sit = time(NULL);
      sit_time += difftime(end_sit, start_sit);
    }
    prev_state = state;
    usleep(100000);                                              
  }
    
  close(clnt_sock[0]);
  close(clnt_sock[1]);
  close(serv_sock);

  return (0);
}