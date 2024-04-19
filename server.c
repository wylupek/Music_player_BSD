#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#include <ao/ao.h>
#include <mpg123.h>
#include <cjson/cJSON.h>
#include <dirent.h> 

#include <fcntl.h> 
#include <pthread.h>
#include <stdbool.h>


#define MULTICAST_PORT 6000
#define MULTICAST_GROUP "239.0.0.1"
#define TCP_PORT 8888
#define TCP_ADDRESS "192.168.68.228"

#define BITS 8
#define SONG_BUFF_SIZE 420
#define PACKET_SERIES_SIZE 8
#define CLIENT_PACKET_BUFF_SIZE 3360 // SONG_BUFF_SIZE * PACKET_SERIES_SIZE

#define CLIENT_MESSAGE_SIZE 4096
#define MAX_JSON_SIZE 8192
#define DELIM ";"

pthread_mutex_t pause_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t curr_song_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t songs_in_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t send_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t send_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t send_curr_song_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t write_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t write_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t client_counter_mutex = PTHREAD_MUTEX_INITIALIZER;
bool is_paused = false, break_song = false;
int songs_in_queue = 0, curr_song = 0, client_counter = 0;
int send_queue = 0, send_list = 0, send_curr_song = 0;

char* jsonToString(char *path_json) {
    /* Init file */
    FILE *fp = fopen(path_json, "r"); 
    if (fp == NULL) { 
        printf("Error: Unable to open the file.\n"); 
        return ""; 
    } 
  
    /* Read the file content into a string */ 
    char *buffer = (char*) malloc((MAX_JSON_SIZE) * sizeof(char));
    fread(buffer, 1, (MAX_JSON_SIZE) * sizeof(char), fp); 
    fclose(fp);

    return buffer;
}

int addToJsonQ(char **command, char **ptr) {
    /* First step of validation */
    *ptr = strtok(NULL, DELIM);
    if (!*ptr) return 0;

    /* String generation */
    char *jsonString = (char*) malloc((MAX_JSON_SIZE) * sizeof(char));
    jsonString = jsonToString("songs/queue.json");
    
    for (int i = 0; i < MAX_JSON_SIZE; i++) {
        if (jsonString[i] == ']'){
            jsonString[i-2] = '\0';
            break;
        }
    }
    
    /* Generating output string */
    if (jsonString[3] != ']')
        strcat(jsonString, ",\r\n{\"q\": \"");
    else
        strcat(jsonString, "\r\n{\"q\": \"");

    strcat(jsonString, *ptr);
    strcat(jsonString, "\", \"title\": \"");

    *ptr = strtok(NULL, DELIM);
    if (!*ptr) return 0;
    strcat(jsonString, *ptr);
    strcat(jsonString, "\", \"artist\": \"");

    *ptr = strtok(NULL, DELIM);
    if (!*ptr) return 0;
    strcat(jsonString, *ptr);
    strcat(jsonString, "\"}\r\n]\0");

    /* Saving into .json */
    FILE *fp = fopen("songs/queue.json", "w"); 
    if (fp == NULL) { 
        printf("Error: Unable to open the file.\n");
        return 0; 
    }
    fputs(jsonString, fp);
    fclose(fp);
    pthread_mutex_lock(&songs_in_queue_mutex);
    songs_in_queue += 1;
    pthread_mutex_unlock(&songs_in_queue_mutex);
    return 1;
}

int delFromJsonQ(char *q_index) {
    /* Validation */
    if (!q_index) return 0;
    int index;
    if (!strcmp(q_index, "0") || !strcmp(q_index, "-0")) {
        index = 0;
    }
    else {
        index = atoi(q_index);
        if (index == 0) return 0;
    }
    if(index > songs_in_queue - 1 || index < 0) return 0;
    
    /* String generation */
    char *jsonString = (char*) malloc((MAX_JSON_SIZE) * sizeof(char));
    jsonString = jsonToString("songs/queue.json");
    
    int counter = 0, start_index = 0, end_index = 0, file_end = 0;
    for (int i = 0; i < MAX_JSON_SIZE; i++) {
        if (jsonString[i] == '\n'){
            counter += 1;
        }
        
        if (start_index == 0 && index == counter - 1) {
            start_index = i;
        }
        else if (index == counter - 2) {
            end_index = i;
            break;
        }
    }
    if (index == songs_in_queue - 1 && songs_in_queue > 1) {start_index -= 2; end_index -= 1;}
    memmove(&jsonString[start_index], &jsonString[end_index], strlen(jsonString) - start_index);

    /* Saving into .json */
    FILE *fp = fopen("songs/queue.json", "w"); 
    if (fp == NULL) { 
        printf("Error: Unable to open the file.\n");
        return 0; 
    }
    fputs(jsonString, fp);
    fclose(fp);
    pthread_mutex_lock(&songs_in_queue_mutex);
    songs_in_queue -= 1;
    pthread_mutex_unlock(&songs_in_queue_mutex);
    if (index < curr_song) {
        pthread_mutex_lock(&curr_song_mutex);
        curr_song -= 1;
        pthread_mutex_unlock(&curr_song_mutex);
    }

    pthread_mutex_lock(&curr_song_mutex);
    break_song = true;
    pthread_mutex_unlock(&curr_song_mutex);

    return 1;
}

int addToJsonS(char **command, char **ptr) {
    /* First step of validation */
    *ptr = strtok(NULL, DELIM);
    if (!*ptr) return -1;

    /* String generation */
    char *jsonString = (char*) malloc((MAX_JSON_SIZE) * sizeof(char));
    jsonString = jsonToString("songs/list.json");
    int songs_on_server = 0;
    for (int i = 0; i < MAX_JSON_SIZE; i++) {
        if (jsonString[i] == '\n') {
            songs_on_server += 1;
        }
        else if (jsonString[i] == ']'){
            if (jsonString[i-2] == '}') {
                jsonString[i-1] =='\0';
            }
            else {
                jsonString[i-2] = '\0';
            }
            break;
        }
    }
    
    /* Generating output string */
    if (jsonString[3] != ']')
        strcat(jsonString, ",\r\n{\"s\": \"");
    else
        strcat(jsonString, "\r\n{\"s\": \"");

    char songs_on_server_str[20];
    sprintf(songs_on_server_str, "%d", songs_on_server);
    strcat(jsonString, songs_on_server_str);
    strcat(jsonString, ".mp3\", \"title\": \"");

    strcat(jsonString, *ptr);
    strcat(jsonString, "\", \"artist\": \"");

    *ptr = strtok(NULL, DELIM);
    if (!*ptr) return -1;
    strcat(jsonString, *ptr);
    strcat(jsonString, "\"}\r\n]\0");

    /* Saving into .json */
    FILE *fp = fopen("songs/list.json", "w"); 
    if (fp == NULL) { 
        printf("Error: Unable to open the file.\n");
        return -1; 
    }
    fputs(jsonString, fp);
    fclose(fp);
    return songs_on_server;
}

void handleMessage(char *command, void *arg) {
    printf("\nMessage from client: %s\n", command);
    if (!strcmp(command, "Pause")) {
        pthread_mutex_lock(&pause_mutex);
        is_paused = true;
        pthread_mutex_unlock(&pause_mutex);
    }
    else if (!strcmp(command, "Play")) {
        pthread_mutex_lock(&pause_mutex);
        is_paused = false;
        pthread_mutex_unlock(&pause_mutex);
    }
    else if (!strcmp(command, "Next")) {
        pthread_mutex_lock(&curr_song_mutex);
        if (send_queue == 0) {
            break_song = true;
            if (curr_song < songs_in_queue) {
                curr_song += 1;
            }
        }
        pthread_mutex_unlock(&curr_song_mutex);
    }
    else if (!strcmp(command, "Previous")) {
        pthread_mutex_lock(&curr_song_mutex);
        if (send_queue == 0) {
            break_song = true;
            if (curr_song > 0) {
                curr_song -= 1;
            }
        }
        pthread_mutex_unlock(&curr_song_mutex);
    }
    else {
        char *ptr = strtok(command, DELIM);
        if (!strcmp(command, "AQ")) {
            pthread_mutex_lock(&write_queue_mutex);
            int fb = addToJsonQ(&command, &ptr);
            pthread_mutex_unlock(&write_queue_mutex);
            if (fb) {
                pthread_mutex_lock(&send_queue_mutex);
                send_queue = client_counter;
                pthread_mutex_unlock(&send_queue_mutex);
            }            
        }
        else if (!strcmp(command, "DQ")) {
            ptr = strtok(NULL, DELIM);
            pthread_mutex_lock(&write_queue_mutex);
            int fb = delFromJsonQ(ptr);
            pthread_mutex_unlock(&write_queue_mutex);
            if (fb) {
                pthread_mutex_lock(&send_queue_mutex);
                send_queue = client_counter;
                pthread_mutex_unlock(&send_queue_mutex);
            }            
        }
        else if (!strcmp(command, "AS")) {
            pthread_mutex_lock(&write_list_mutex);
            int newSocket = *((int *)arg);
            int fb = addToJsonS(&command, &ptr);
            if (fb > 0) {
                /* Filename is fb.mp3 */
                char filename[30];
                char fb_str[20];
                sprintf(fb_str, "%d", fb);
                strcat(filename, "songs/");
                strcat(filename, fb_str);
                strcat(filename, ".mp3");

                char buffer[CLIENT_MESSAGE_SIZE];
                FILE *fp = fopen(filename, "wb");
                if (fp == NULL) {
                    printf("Error in creating file.mp3");
                    return;
                }
                memset(filename, 0, sizeof(filename));
                memset(fb_str, 0, sizeof(fb_str));

                int bytes_received;
                while((bytes_received = recv(newSocket, buffer, CLIENT_MESSAGE_SIZE, 0)) > 0) {
                    if (!strcmp(buffer, "Finish")) {break;}
                    fwrite(buffer, 1, bytes_received, fp);
                    bzero(buffer, CLIENT_MESSAGE_SIZE);
                }
                fclose(fp);

                pthread_mutex_unlock(&write_list_mutex);
                pthread_mutex_lock(&send_list_mutex);
                send_list = client_counter;
                pthread_mutex_unlock(&send_list_mutex);
            }            
        }
    }
}

void *sockThreadRecv(void *arg) {
    /* Init */
    char client_message[CLIENT_MESSAGE_SIZE];
    int newSocket = *((int *)arg);

    /* Receiving TCP messages */
    for (;;) {
        if (recv(newSocket, client_message, CLIENT_MESSAGE_SIZE, 0) < 1) {
            break;
        }
        handleMessage(client_message, &newSocket);
        usleep(10);
        memset(client_message, 0, sizeof (client_message));    
    }

    /* When DC */
    pthread_mutex_lock(&client_counter_mutex);
    client_counter -= 1;
    pthread_mutex_unlock(&client_counter_mutex);
    printf("\nClient disconnected.\n");
    printf("Client counter: %d\n", client_counter);
    pthread_exit(NULL);
}

void *sockThreadSend(void *arg) {
    /* Init */
    int newSocket = *((int *)arg);

    /* Sending current list at connect */
    char *server_message = (char*) malloc((MAX_JSON_SIZE) * sizeof(char));
    server_message = jsonToString("songs/list.json");
    if (send(newSocket, server_message, strlen(server_message), 0) < 0) {
        printf("Failed to send init list.\n");
        pthread_exit(NULL);
    }
    memset(server_message, 0, MAX_JSON_SIZE * sizeof(char));

    /* Sending current queue at connect */
    server_message = jsonToString("songs/queue.json");
    if (send(newSocket, server_message, strlen(server_message), 0) < 0) {
        printf("Failed to send init queue.\n");
        pthread_exit(NULL);
    }
    memset(server_message, 0, MAX_JSON_SIZE * sizeof(char));

    /* Sending current_song at connect */
    char str_curr_song[20];
    sprintf(str_curr_song, "%d", curr_song);
    if (send(newSocket, str_curr_song, strlen(str_curr_song), 0) < 0) {
        printf("Failed to send current_song.\n");
        pthread_exit(NULL);
    }
    memset(str_curr_song, 0, sizeof(str_curr_song));

    for (;;) {
        /* Sending queue after receiving message "AQ" or "DQ" */
        if (send_queue > 0) {
            server_message = jsonToString("songs/queue.json");
            if (send(newSocket, server_message, strlen(server_message), 0) < 0) {
                printf("Failed to send queue.\n");
                pthread_exit(NULL);
            }
            memset(server_message, 0, MAX_JSON_SIZE * sizeof(char));
            pthread_mutex_lock(&send_queue_mutex);
            send_queue -= 1;
            pthread_mutex_unlock(&send_queue_mutex);
        }

        /* Sending queue after receiving message "AS" */
        if (send_list > 0) {
            server_message = jsonToString("songs/list.json");
            if (send(newSocket, server_message, strlen(server_message), 0) < 0) {
                printf("Failed to send queue.\n");
                pthread_exit(NULL);
            }
            memset(server_message, 0, MAX_JSON_SIZE * sizeof(char));
            pthread_mutex_lock(&send_list_mutex);
            send_list -= 1;
            pthread_mutex_unlock(&send_list_mutex);
        }

        /* Sending current_song after UDP next song iteration */
        if (send_curr_song > 0) {
            sprintf(str_curr_song, "%d", curr_song);
            if (send(newSocket, str_curr_song, strlen(str_curr_song), 0) < 0) {
                printf("Failed to send current_song.\n");
                pthread_exit(NULL);
            }
            memset(str_curr_song, 0, sizeof(str_curr_song));
            pthread_mutex_lock(&send_curr_song_mutex);
            send_curr_song -= 1;
            pthread_mutex_unlock(&send_curr_song_mutex);
        }

        usleep(500000);
    }
    pthread_exit(NULL);
}

void *tcpListener() {
    /* Init */
    int serverSocket, newSocket;
    struct sockaddr_in serverAddr;
    struct sockaddr_storage serverStorage;
    socklen_t addr_size;

    /* Config socket */
    serverSocket = socket(PF_INET, SOCK_STREAM, 0);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(TCP_PORT);
    serverAddr.sin_addr.s_addr = inet_addr(TCP_ADDRESS);
    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
    bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));
    if (listen(serverSocket, 50) != 0) {
        printf("Listen error\n");
        return 0;
    }

    /* Listening for new TCP connections */
    printf("TCP Listening\n");
    pthread_t thread_id1, thread_id2;
    while(1) {
        addr_size = sizeof serverStorage;
        newSocket = accept(serverSocket, (struct sockaddr *) &serverStorage, &addr_size);
        if (pthread_create(&thread_id1, NULL, sockThreadRecv, &newSocket) != 0) {
            printf("Failed to create recv thread\n");
        }
        else {
            if (pthread_create(&thread_id2, NULL, sockThreadSend, &newSocket) != 0) {
                printf("Failed to create send thread\n");
            }
            else {
                pthread_mutex_lock(&client_counter_mutex);
                client_counter += 1;
                pthread_mutex_unlock(&client_counter_mutex);
                printf("\nNew client connected.\n");
                printf("Client counter: %d\n", client_counter);

                pthread_detach(thread_id1);
                pthread_detach(thread_id2);
            }
        }
    }
}

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

void initJson(cJSON **json, char *path_json) {
    /* Open the file */
    FILE *fp = fopen(path_json, "r"); 
    if (fp == NULL) { 
        printf("Error: Unable to open the file.\n"); 
        return; 
    } 
  
    /* Read the file contents into a string */
    char buffer[MAX_JSON_SIZE]; 
    fread(buffer, 1, sizeof(buffer), fp); 
    fclose(fp); 
  
    /* Parse the JSON data */
    *json = cJSON_Parse(buffer); 
    if (*json == NULL) { 
        const char *error_ptr = cJSON_GetErrorPtr(); 
        if (error_ptr != NULL) { 
            printf("Error: %s\n", error_ptr); 
        } 
        cJSON_Delete(*json); 
        return; 
    } 
}

int main(int argc, char** argv) {
    /* Create thread for tcp listener */
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, tcpListener, NULL) != 0) {
        printf("Failed to create thread\n");
        return 0;
    }
    pthread_detach(thread_id);

    /* Set up socket */
    struct sockaddr_in addr;
    int addrlen, sock;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }
    bzero((char *)&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(MULTICAST_PORT);
    addr.sin_addr.s_addr = inet_addr(MULTICAST_GROUP);
    addrlen = sizeof(addr);

    /* Init ao and mpg123 */
    mpg123_init();
    ao_initialize();
    ao_device *dev;
    mpg123_handle *mh;

    /* Init Json*/
    cJSON *root_json, *song_json;
    initJson(&root_json, "songs/queue.json");
    songs_in_queue = cJSON_GetArraySize(root_json);

    /* Init for indexing packets */
    int tmpNum = CLIENT_PACKET_BUFF_SIZE - 1;
    int index_Bytes = 1;
    while (tmpNum = (tmpNum / 256)) {
        index_Bytes += 1;
    }

    /* Send song */
    printf("Streaming started\n");
    unsigned char blank[SONG_BUFF_SIZE] = {0};
    size_t done;
    long packet_counter = 0;
    unsigned char *buffer = (unsigned char*) malloc((SONG_BUFF_SIZE) * sizeof(unsigned char)) + index_Bytes;
    for(;;) {
        while(curr_song>=songs_in_queue) {sleep(1);}
        pthread_mutex_lock(&send_curr_song_mutex);
        send_curr_song = client_counter;
        pthread_mutex_unlock(&send_curr_song_mutex);
        pthread_mutex_lock(&curr_song_mutex);
        break_song = false;
        pthread_mutex_unlock(&curr_song_mutex);
        
        /* Song Init */
        initJson(&root_json, "songs/queue.json");
        song_json = cJSON_GetArrayItem(root_json, curr_song);
        char *song_name = cJSON_GetObjectItem(song_json, "q")->valuestring;
        char *fullpath = (char *) malloc(sizeof("songs/") + sizeof(song_name));
        bzero(fullpath, sizeof(fullpath));
        strcat(strcat(fullpath, "songs/"), song_name);
        initSong(&mh, &dev, fullpath);
        printf("\nPlaying song: %s", fullpath);
        while (mpg123_read(mh, &buffer[index_Bytes], SONG_BUFF_SIZE, &done) == MPG123_OK) {
            while(is_paused) {
                if(break_song)
                    break;
                usleep(500000);
            }
            
            /* Coding indices */
            for (int j = index_Bytes - 1; j >= 0; j--) {
                unsigned char byte = ((packet_counter % CLIENT_PACKET_BUFF_SIZE)>> (j * 8)) & 0xFF;
                buffer[index_Bytes - 1 - j] = byte;
            }

            /* Debug printing */
            // for (int i = 0; i < SONG_BUFF_SIZE + index_Bytes; i ++)
            //     printf("%d ", buffer[i]);
            // printf("\n%d, %d\n", buffer[0], buffer[1]);

            if (sendto(sock, buffer, SONG_BUFF_SIZE, 0, (struct sockaddr *) &addr, addrlen) < 0) {
                perror("sendto");
                exit(1);
            }

            packet_counter++;
            if(packet_counter % PACKET_SERIES_SIZE == PACKET_SERIES_SIZE - 1) {
                if(break_song)
                    break;

                for(int i = 0; i < PACKET_SERIES_SIZE; i++) {
                    ao_play(dev, blank, done);
                }
                printf(".");
                fflush(stdout);
            }
        }
        if (!break_song) {
            packet_counter = 0;
            pthread_mutex_lock(&curr_song_mutex);
            curr_song += 1;
            pthread_mutex_unlock(&curr_song_mutex);
        }
    }
    // { /* Clean up */
    // free(buffer);
    // ao_close(dev);
    // ao_shutdown();
    // mpg123_close(mh);
    // mpg123_delete(mh);
    // mpg123_exit();
    // close(sock);
    // cJSON_Delete(root_json);
    // }
}
