#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ao/ao.h>
#include <mpg123.h>

#define BITS 8
#define SONG_BUFF_SIZE 420
#define PACKAGE_SERIES_SIZE 8



int main(int argc, char ** argv) {
    // Init UDP
    struct sockaddr_in serverAdress;
    serverAdress.sin_family = AF_INET;
    serverAdress.sin_port = htons(1100);
    serverAdress.sin_addr.s_addr = inet_addr("");
    int localSocket = socket(PF_INET, SOCK_DGRAM, 0);
    if (localSocket == -1) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }

    // Sending song name to server
    char* songName = (char *)argv[1];
    int on = 1;
    setsockopt(localSocket, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
    if (sendto(localSocket, (char *)songName, strlen(songName), 0, (const struct sockaddr *)&serverAdress, sizeof(serverAdress)) < 0) {
        perror("Could not send");
        close(localSocket);
        exit(EXIT_FAILURE);
    }
    //                                    [  208     44100     2   ]
    // Receive decoding format for device [encoding, rate, channels]
    int len = sizeof(serverAdress);
    size_t setup_size = 3 * sizeof(int); 
    int *format_arr = (int *) malloc(setup_size * sizeof(int));
    if (recvfrom(localSocket, format_arr, setup_size, MSG_WAITALL, (struct sockaddr *) &serverAdress, &len) < 0) {
        perror("could not recive");
        close(localSocket);
        exit(EXIT_FAILURE);
    }
    // printf("%d, %d, %d\n", format_arr[0], format_arr[1], format_arr[2]);

    // Set the output format and open the output device
    ao_initialize();
    int driver = ao_default_driver_id();
    ao_sample_format format;
    format.bits = mpg123_encsize(format_arr[0]) * BITS;
    format.rate = format_arr[1];
    format.channels = format_arr[2];
    // format.bits = mpg123_encsize(208) * BITS;
    // format.rate = 44100;
    // format.channels = 2;
    format.byte_format = AO_FMT_NATIVE;
    format.matrix = 0;
    ao_device *dev = ao_open_live(driver, &format, NULL);

    // Receive bytes of song
    size_t done = 4, buffer_size = 4;
    unsigned char *buffer = (unsigned char*) malloc(buffer_size * sizeof(unsigned char));
    for(;;) {
        while (recvfrom(localSocket, buffer, buffer_size, MSG_WAITALL, (struct sockaddr *) &serverAdress, &len) < 0) {
            perror("could not recive");
            close(localSocket);
            exit(EXIT_FAILURE);
        }
        ao_play(dev, buffer, done);
    }

    // clean up
    free(buffer);
    ao_close(dev);
    ao_shutdown();
    close(localSocket);

    return 0;
}

// gcc ClientUDP.c -o ClientUDP -lmpg123 -lao && ./ClientUDP test.mp3