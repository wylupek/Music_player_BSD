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

int *initSong(mpg123_handle **mh, ao_device **dev, char *songName) {
    /* Init mpg123 and ao  */
    int driver = ao_default_driver_id();
    int err;
    *mh = mpg123_new(NULL, &err);
    
    /* Open the file and get the decoding format
       [  208     44100     2   ]
       [encoding, rate, channels]  */
    int * format_arr = malloc(sizeof(3));
    mpg123_open(*mh, songName);
    mpg123_getformat(*mh, (long*)&format_arr[1], &format_arr[2], &format_arr[0]);

    /* Set the output format and open the output device */
    ao_sample_format format;
    format.bits = mpg123_encsize(format_arr[0]) * BITS;
    format.rate = format_arr[1];
    format.channels = format_arr[2];
    format.byte_format = AO_FMT_NATIVE;
    format.matrix = 0;
    *dev = ao_open_live(driver, &format, NULL);
    return format_arr;
}

int main(int argc, char ** argv){
    // Init UDP
    struct sockaddr_in localAddress, clientAdress;
    localAddress.sin_family = AF_INET;
    localAddress.sin_port = htons(1100);
    localAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    int server_socket = socket(PF_INET, SOCK_DGRAM, 0);
    if (server_socket == -1) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }
    if (bind(server_socket, (struct sockaddr*) &localAddress, sizeof(localAddress)) == -1) {
        perror("Could not bind");
        close(server_socket);
    }
       
    // Receive song name from client
    int len = sizeof(clientAdress);
    char songName[128];
    bzero(songName, sizeof(songName));
    if (recvfrom(server_socket, songName, sizeof(songName), MSG_WAITALL, (struct sockaddr *) &clientAdress, &len) < 0) {
        perror("could not recive");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("Song name: %s\n", songName);

    int on = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));

    // Init mpg123 and ao
    mpg123_init();
    ao_initialize();
    ao_device *dev;
    mpg123_handle *mh;
    int *format_arr = initSong(&mh, &dev, songName);

    // Send decoding format to client
    if (sendto(server_socket, format_arr, 3 * sizeof(int), 0, (const struct sockaddr *) &clientAdress,len) < 0)  {
        perror("could not send");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    // printf("%d, %d, %d\n", format_arr[0], format_arr[1], format_arr[2]);

    // Sending bytes of song
    printf("Sending song...\n");
    unsigned char blank[4] = {0, 0, 0, 0};
    size_t done, buffer_size = 4; 
    unsigned char *buffer = (unsigned char*) malloc(buffer_size * sizeof(unsigned char));
    while (mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK) {
        if (sendto(server_socket, buffer, 4, 0, (const struct sockaddr *) &clientAdress,len) < 0)  {
            perror("could not send");
            close(server_socket);
            exit(EXIT_FAILURE);
        }
        ao_play(dev, blank, done);
    }
    
    // clean up
    free(buffer);
    ao_close(dev);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
    ao_shutdown();
    close (server_socket);
    
    return 0;
}

//gcc ServerUDP.c -o ServerUDP -lmpg123 -lao && ./ServerUDP