#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define BUF_SIZE 1024

void take_picture(int index) {
    char filename[50];
    snprintf(filename, 50, "photos/photo_%03d.jpg", index);

    char command[100];
    snprintf(command, 100, "libcamera-still -o %s", filename);
    system(command);

    printf("Photo saved: %s\n", filename);
}

void create_photo_zip(const char *zip_name) {
    char command[200];
    snprintf(command, 200, "zip -r %s photos", zip_name);
    system(command);
    printf("Photos compressed into: %s\n", zip_name);
}

int cameraMain(int argc, char *argv[]) {
    char message[BUF_SIZE];
    int photo_index = 0;

    // 작업 디렉토리 생성
    system("mkdir -p photos");

    while (1) {
        printf("Photo capturing started...\n");
        while (1) {
            take_picture(photo_index++);
            sleep(5); 
        }
    }
    return 0;
}
