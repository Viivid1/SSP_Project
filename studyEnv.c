#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#define IN 0
#define OUT 1
#define PWM 0

#define LOW 0
#define HIGH 1

#define PIN 20
#define POUT2 21
#define PPIN 23
#define PPOUT 24
#define POUT 17
#define POUT3 27
#define VALUE_MAX 256
#define DIRECTION_MAX 256
#define BUFFER_MAX 3

int toggle = 0;
int light = 0;
int arr[2] = {0};
int repeat = 200;

static int PWMExport(int pwmnum)
{
    char buffer[BUFFER_MAX];
    int fd, byte;

    // TODO: Enter the export path.
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

static int PWMEnable(int pwmnum)
{
    static const char s_enable_str[] = "1";

    char path[DIRECTION_MAX];
    int fd;

    // TODO: Enter the enable path.
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

static int PWMWritePeriod(int pwmnum, int value)
{
    char s_value_str[VALUE_MAX];
    char path[VALUE_MAX];
    int fd, byte;

    // TODO: Enter the period path.
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

static int PWMWriteDutyCycle(int pwmnum, int value)
{
    char s_value_str[VALUE_MAX];
    char path[VALUE_MAX];
    int fd, byte;

    // TODO: Enter the duty_cycle path.
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

void setFrequency(int pwmnum, int frequency)
{
    int period = 1000000000 / frequency;
    PWMWritePeriod(pwmnum, period);
    PWMWriteDutyCycle(pwmnum, period / 2);
}

void stopSound(int pwmnum)
{
    PWMWriteDutyCycle(pwmnum, 0);
}

static int GPIOExport(int pin)
{
#define BUFFER_MAX 3
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
            setFrequency(PWM, 440);
        }
        else
        {
            stopSound(PWM);
        }
    }

    PWMDisable(PWM);
}

void *t_function1(void *data)
{
    if (GPIOExport(POUT) == -1 ||
        GPIOExport(POUT3) == -1)
    {
        printf("export error\n");
        return NULL;
    }

    if (GPIODirection(POUT, OUT) == -1 ||
        GPIODirection(POUT3, OUT) == -1)
    {
        printf("direction error\n");
        return NULL;
    }

    while (repeat)
    {
        if (GPIOWrite(POUT, arr[0]) == -1)
        {
            printf("write error\n");
            return NULL;
        }
        if (GPIOWrite(POUT3, arr[1]) == -1)
        {
            printf("write error\n");
            return NULL;
        }
    }

    if (GPIOUnexport(POUT) == -1 || GPIOUnexport(POUT3) == -1)
    {
        return NULL;
    }
}

int main(int argc, char *argv[])
{
    pthread_t p_thread[2];
    int thr_id;
    int status;
    char p1[] = "thread_1";
    char p2[] = "thread_2";
    int state = 1;
    int prev_state = 1;
    int _state = 1;
    int _prev_state = 1;

    thr_id = pthread_create(&p_thread[0], NULL, t_function1, (void *)p1);
    if (thr_id < 0)
    {
        perror("thread create error : ");
        exit(0);
    }

    thr_id = pthread_create(&p_thread[1], NULL, t_function2, (void *)p2);
    if (thr_id < 0)
    {
        perror("thread create error : ");
        exit(0);
    }

    if (GPIOExport(POUT2) == -1 ||
        GPIOExport(PIN) ||
        GPIOExport(PPOUT) == -1 ||
        GPIOExport(PPIN))
    {
        return 1;
    }

    if (GPIODirection(POUT2, OUT) == -1 ||
        GPIODirection(PIN, IN) == -1 ||
        GPIODirection(PPOUT, OUT) == -1 ||
        GPIODirection(PPIN, IN) == -1)
    {
        return 2;
    }

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
                arr[0] = 0;
                arr[1] = 0;
                break;
            case 1:
                arr[0] = 1;
                break;
            case 2:
                arr[1] = 1;
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

        printf("GPIORead: 1:%d, 2:%d, from pin %d\n", arr[0], arr[1], PIN);
        printf("GPIORead: 3:%d, from pin %d\n", toggle, PPIN);
        usleep(1000 * 100);
    } while (repeat--);

    if (GPIOUnexport(POUT2) == -1 || GPIOUnexport(PIN) == -1 || GPIOUnexport(PPOUT) == -1 || GPIOUnexport(PPIN) == -1)
    {
        return 4;
    }

    pthread_join(p_thread[0], (void **)&status);
    pthread_join(p_thread[1], (void **)&status);

    return 0;
}
