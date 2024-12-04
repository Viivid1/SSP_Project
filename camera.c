#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

void take_picture(int index) {
    char filename[50];
    snprintf(filename, 50, "photos/photo_%03d.jpg", index);

    char command[100];
    snprintf(command, 100, "libcamera-still -o %s", filename);
    system(command);

    printf("Photo saved: %s\n", filename);
}

void create_timelapse_video(const char *output_file) {
    char command[200];
    snprintf(command, 200, "ffmpeg -y -r 30 -i photos/photo_%%03d.jpg -vcodec libx264 -pix_fmt yuv420p %s", output_file);
    system(command);

    printf("Timelapse video created: %s\n", output_file);
}

int main() {
    const int num_photos = 100; 
    const int interval = 3;
    const char *output_video = "timelapse.mp4";

    system("mkdir -p photos");

    for (int i = 0; i < num_photos; i++) {
        take_picture(i);
        sleep(interval);
    }

    create_timelapse_video(output_video);

    return 0;
}
