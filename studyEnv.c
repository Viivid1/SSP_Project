// 필요한 헤더 파일 포함
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

// GPIO 및 PWM 매크로 정의
#define IN 0
#define OUT 1
#define PWM 0

#define LOW 0
#define HIGH 1

#define PIN 20        // 첫 번째 버튼의 GPIO 핀 번호
#define POUT2 21      // 첫 번째 LED의 GPIO 핀 번호
#define PPIN 23       // 두 번째 버튼의 GPIO 핀 번호
#define PPOUT 24      // 두 번째 LED의 GPIO 핀 번호
#define POUT 17       // LED 1 GPIO 핀
#define POUT3 27      // LED 2 GPIO 핀
#define VALUE_MAX 256
#define DIRECTION_MAX 256
#define BUFFER_MAX 3

// 글로벌 변수 선언
int toggle = 0;             // 스피커 ON/OFF 상태
int light = 0;              // LED 상태 (0: OFF, 1: LED1 ON, 2: LED2 ON)
int arr[2] = {0};           // LED 상태 저장 배열
int repeat = 1;             // 모든 코드를 제어하는 변수
int arr_bitmask = 0;        // LED 비트마스크 상태

// PWM 핀을 활성화
static int PWMExport(int pwmnum)
{
    char buffer[BUFFER_MAX];
    int fd, byte;

    fd = open("/sys/class/pwm/pwmchip0/export", O_WRONLY);
    if (-1 == fd)
    {
        fprintf(stderr, "Failed to open export for export!\n");
        return (-1);
    }

    byte = snprintf(buffer, BUFFER_MAX, "%d", pwmnum);
    write(fd, buffer, byte);
    close(fd);

    sleep(1);

    return (0);
}

// PWM 핀 사용 가능 설정
static int PWMEnable(int pwmnum)
{
    static const char s_enable_str[] = "1";

    char path[DIRECTION_MAX];
    int fd;

    snprintf(path, DIRECTION_MAX, "/sys/class/pwm/pwmchip0/pwm0/enable", pwmnum);
    fd = open(path, O_WRONLY);
    if (-1 == fd)
    {
        fprintf(stderr, "Failed to open in enable!\n");
        return -1;
    }

    write(fd, s_enable_str, strlen(s_enable_str));
    close(fd);

    return (0);
}

// PWM 주기(period) 설정
static int PWMWritePeriod(int pwmnum, int value)
{
    char s_value_str[VALUE_MAX];
    char path[VALUE_MAX];
    int fd, byte;

    snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm0/period", pwmnum);
    fd = open(path, O_WRONLY);
    if (-1 == fd)
    {
        fprintf(stderr, "Failed to open in period!\n");
        return (-1);
    }
    byte = snprintf(s_value_str, VALUE_MAX, "%d", value);

    if (-1 == write(fd, s_value_str, byte))
    {
        fprintf(stderr, "Failed to write value in period!\n");
        close(fd);
        return -1;
    }
    close(fd);

    return (0);
}

// PWM 듀티 사이클 설정
static int PWMWriteDutyCycle(int pwmnum, int value)
{
    char s_value_str[VALUE_MAX];
    char path[VALUE_MAX];
    int fd, byte;

    snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm0/duty_cycle", pwmnum);
    fd = open(path, O_WRONLY);
    if (-1 == fd)
    {
        fprintf(stderr, "Failed to open in duty cycle!\n");
        return (-1);
    }
    byte = snprintf(s_value_str, VALUE_MAX, "%d", value);

    if (-1 == write(fd, s_value_str, byte))
    {
        fprintf(stderr, "Failed to write value in duty cycle!\n");
        close(fd);
        return -1;
    }
    close(fd);

    return (0);
}

// PWM 비활성화
static int PWMDisable(int pwmnum)
{
    static const char s_disable_str[] = "0";
    char path[DIRECTION_MAX];
    int fd;

    snprintf(path, DIRECTION_MAX, "/sys/class/pwm/pwmchip0/pwm0/enable", pwmnum);
    fd = open(path, O_WRONLY);
    if (-1 == fd)
    {
        fprintf(stderr, "Failed to open in disable!\n");
        return -1;
    }

    write(fd, s_disable_str, strlen(s_disable_str));
    close(fd);

    return (0);
}

// PWM 주파수 설정
void setFrequency(int pwmnum, int frequency)
{
    int period = 1000000000 / frequency;
    PWMWritePeriod(pwmnum, period);
    PWMWriteDutyCycle(pwmnum, period / 2);
}

// PWM으로 소리 끄기
void stopSound(int pwmnum)
{
    PWMWriteDutyCycle(pwmnum, 0);
}

// GPIO 핀 초기화
static int GPIOExport(int pin)
{
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (-1 == fd)
    {
        fprintf(stderr, "Failed to open export for writing!\n");
        return (-1);
    }

    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return (0);
}

// GPIO 핀 해제
static int GPIOUnexport(int pin)
{
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (-1 == fd)
    {
        fprintf(stderr, "Failed to open unexport for writing!\n");
        return (-1);
    }

    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return (0);
}

// GPIO 핀 방향 설정
static int GPIODirection(int pin, int dir)
{
    static const char s_directions_str[] = "in\0out";

    char path[DIRECTION_MAX];
    int fd;

    snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
    fd = open(path, O_WRONLY);
    if (-1 == fd)
    {
        fprintf(stderr, "Failed to open gpio direction for writing!\n");
        return (-1);
    }

    if (-1 ==
        write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3))
    {
        fprintf(stderr, "Failed to set direction!\n");
        return (-1);
    }

    close(fd);
    return (0);
}

// GPIO 핀 읽기
static int GPIORead(int pin)
{
    char path[VALUE_MAX];
    char value_str[3];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_RDONLY);
    if (-1 == fd)
    {
        fprintf(stderr, "Failed to open gpio value for reading!\n");
        return (-1);
    }

    if (-1 == read(fd, value_str, 3))
    {
        fprintf(stderr, "Failed to read value!\n");
        return (-1);
    }

    close(fd);

    return (atoi(value_str));
}

// GPIO 핀 쓰기
static int GPIOWrite(int pin, int value)
{
    static const char s_values_str[] = "01";

    char path[VALUE_MAX];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_WRONLY);
    if (-1 == fd)
    {
        fprintf(stderr, "Failed to open gpio value for writing!\n");
        return (-1);
    }

    if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1))
    {
        fprintf(stderr, "Failed to write value!\n");
        return (-1);
    }

    close(fd);
    return (0);
}

// LED를 제어하는 스레드
void *t_function1(void *data)
{
    // LED GPIO 핀 초기화
    if (GPIOExport(POUT) == -1 || GPIOExport(POUT3) == -1)
    {
        printf("export error\n");
        return NULL;
    }

    // LED GPIO 핀 방향 설정
    if (GPIODirection(POUT, OUT) == -1 || GPIODirection(POUT3, OUT) == -1)
    {
        printf("direction error\n");
        return NULL;
    }

    // LED 상태를 arr_bitmask를 통해 제어
    while (repeat)
    {
        if (GPIOWrite(POUT, (arr_bitmask & (1 << 0)) ? 1 : 0) == -1)
        {
            printf("write error\n");
            return NULL;
        }
        
        if (GPIOWrite(POUT3, (arr_bitmask & 1) ? (1 << 1)) == -1)
        {
            printf("write error\n");
            return NULL;
        }
    }

    GPIOUnexport(POUT);
    GPIOUnexport(POUT3);
}

// 스피커를 제어하는 스레드
void *t_function2(void *data)
{
    PWMExport(PWM);
    sleep(1);
    stopSound(PWM);
    PWMEnable(PWM);

    while (repeat)
    {
        if (toggle)
        {
            setFrequency(PWM, 40); // 스피커 켜기
        }
        else
        {
            stopSound(PWM); // 스피커 끄기
        }
    }

    PWMDisable(PWM);
}

// 메인 함수: LED와 스피커 제어를 위한 스레드 생성 및 버튼 입력 처리
int main(int argc, char *argv[])
{
    pthread_t p_thread[2];
    int thr_id;
    int status;
    char p1[] = "thread_1";
    char p2[] = "thread_2";
    int state = 1;        //LED 버튼 인식
    int prev_state = 1;   //LED 버튼 인식
    int _state = 1;       //피에조 스피커
    int _prev_state = 1;  //피에조 스피커

    // LED 제어 스레드 생성
    thr_id = pthread_create(&p_thread[0], NULL, t_function1, (void *)p1);
    if (thr_id < 0)
    {
        perror("thread create error : ");
        exit(0);
    }

    // 스피커 제어 스레드 생성
    thr_id = pthread_create(&p_thread[1], NULL, t_function2, (void *)p2);
    if (thr_id < 0)
    {
        perror("thread create error : ");
        exit(0);
    }

    // GPIO 초기화
    if (GPIOExport(POUT2) == -1 || GPIOExport(PIN) || GPIOExport(PPOUT) == -1 || GPIOExport(PPIN))
    {
        return 1;
    }

    if (GPIODirection(POUT2, OUT) == -1 || GPIODirection(PIN, IN) == -1 ||
        GPIODirection(PPOUT, OUT) == -1 || GPIODirection(PPIN, IN) == -1)
    {
        return 2;
    }

    // 버튼 상태 확인 및 제어 반복
    do
    {
        if (GPIOWrite(POUT2, 1) == -1 || GPIOWrite(PPOUT, 1) == -1)
        {
            return 3;
        }

        state = GPIORead(PIN);
        _state = GPIORead(PPIN);

        if (state == 1 && prev_state == 0)
        {
            light = (light + 1) % 3;
            switch (light)
            {
            case 0:
                arr_bitmask &= 0;  // 0
                break;
            case 1:
                arr_bitmask |= 1;  // 1
                break;
            case 2:
                arr_bitmask |= 3;  // 11
                break;
            default:
                break;
            }
        }

        if (_state == 1 && _prev_state == 0)
        {
            toggle = (toggle + 1) % 2;
        }

        prev_state = state;
        _prev_state = _state;

        printf("GPIORead: bitmask state: 0x%x (from pin %d)\n", arr_bitmask, PIN);
        printf("GPIORead: toggle state: %d (from pin %d)\n", toggle, PPIN);
        usleep(1000 * 100);
    } while (repeat);

    // GPIO 핀 해제
    if (GPIOUnexport(POUT2) == -1 || GPIOUnexport(PIN) == -1 || GPIOUnexport(PPOUT) == -1 || GPIOUnexport(PPIN) == -1)
    {
        return 4;

    }

    // 스레드 종료 대기
    pthread_join(p_thread[0], (void **)&status);
    pthread_join(p_thread[1], (void **)&status);

    return 0;
}
