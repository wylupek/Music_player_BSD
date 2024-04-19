# Music player BSD
Music player BSD is a client-server project employing BSD sockets for communication. The server is developed in C, while the client is built in Kotlin for Android devices. Communication is executed through combination of UDP protocol, utilized for music streaming, and TCP protocol, responsible for handling client requests and providing necessary data like the song queue.

## Song streaming
The song is streamed on a separate thread using a UDP multicast socket to a **`MULTICAST_GROUP`** on **`MULTICAST_PORT`**.
1. The first song from the **`queue.json`** is loaded into the appropriate structures of the mpg123 library.
1. Song fragments of size **`SONG_BUFF_SIZE`** bytes are written to the buffer using the mpg123 library.
1. At the beginning of each buffer, packet indices in range **`CLIENT_PACKET_BUFF_SIZE`** are added, necessary for buffering them by the client.
1. The buffer is sent to the client through a UDP multicast socket. 
1. After sending **`PACKET_SERIES_SIZE`** The server waits until a fragments of the song are played. If the song has not finished server reads next fragments of the song **(step 2)**.  
1. If the song has finished, the server loads the next item from the queue **(step 1)**.

## Handling clients
The rest of the communication between the server and the client is handled on a separate thread via TCP socket with **`TCP_ADDRESS`** on **`TCP_PORT`**. Command components are separated with **`DELIM`** sign.

### The server receives the following messages from client:
* `Pause` -  pauses the currently playing song.
* `Play` -  resumes the currently playing song.
* `Next` - changes the currently playing song to the next one in the queue.
* `Previous` - changes the currently playing song to the previous one in the queue.
* `AQ;filaname;title;artist` - Adds a song to the end of the queue.
* `DQ;queue_index` - Removes the song at the specified index from the queue.
* `AS;title;artist` - 
Adds a song to the server and updates the list.json.  Then, it receives from the client a sequence of **`CLIENT_MESSAGE_SIZE`** bytes representing the new song, which it saves to an .mp3 file. The server stops creating the file upon receiving the string `Finish`.

### The server sends to clients following messages:
* The list of available songs `list.json`:
    + to a new client,
    + when any client uploads a new song to the server.
* The list of available songs in queue `queue.json`:
    + to a new client,
    + when any client performs an action on the queue, modifying it.
* Index from the queue of the currently playing song:
    + to a new client,
    + when the song end,
    + when any client wants to play the next or previous song.
    + If any client modifies the queue before the currently playing song.

## Set up
Available commands:
- `make install` - to install all necessary packages
- `make serverRun` - to Compile and run server
- `make clientRun` - to Compile and run clien.
- `make clean` - to remove .out files

## Additional files
**client.c** - 
this is a client implementation in C. However, it is trimmed down and does not allow playing songs. However, it is possible to test TCP communication.

**UDP_test** - in this folder you will find a server and a client that allows testing music streaming via UDP. However, they do not have any additional functions implemented. Use the same commands as in the "Set up" section above.

## Example client application
https://github.com/WuzI38/GrooveGlider
