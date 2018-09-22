/**************************************
 * Network Systems Project 1
 * Server Code
 * Ben Heberlein
 * 
 * This file implements the client-side
 * code for Network Systems Project 1.
 * This code will facilitate reliable 
 * UDP transfers.
 *************************************/

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Size of packet payload (for all packets, for simplicity) */
#define DATA_SIZE 1024
#define FRAME_SIZE 1022
#define MSG_SIZE  DATA_SIZE + 8

/* Codes for operations and packet functions for each operation */
enum oper_e {OPER_GET  = 0, OPER_PUT, OPER_DEL, OPER_LS, OPER_EXIT};
enum get_e  {GET_INIT  = 0, GET_DATA, GET_DONE};
enum put_e  {PUT_INIT  = 0, PUT_DATA, PUT_DONE};
enum del_e  {DEL_INIT  = 0, DEL_DONE};
enum ls_e   {LS_INIT   = 0, LS_DATA,  LS_DONE};
enum exit_e {EXIT_INIT = 0};

/* Message structure */
typedef struct msg_s {
    uint32_t oper;
    uint32_t func;
    uint8_t data[DATA_SIZE];
} msg_t;

/* Usage message */
char usage[128] = "client <server_ip> <port>\n";

/* Socket parameters */
int sock = 0;
struct sockaddr_in serv_addr;

/* Error handler */
void error(char *msg) {
  perror(msg);
  exit(1);
}

/* Warning harndler */
void warn(char *msg) {
  perror(msg);
}

/* Get operation for client side */
void get(char *file) {
    msg_t init;
    msg_t rec;
    msg_t d;
    msg_t done;
    int ret = 0;
    int serv_len = 0;
    int file_len = 0;
    char *fbuf;
    FILE *f;
    int curr_dpkt = 0;
    int pkt_id = 0;
    int num_dpkt = 0;

    /* Create init packet */
    init.oper = OPER_GET;
    init.func = GET_INIT;
    strcpy(init.data, file);

    /* Create data request packet */
    d.oper = OPER_GET;
    d.func = GET_DATA;
    d.data[0] = 0;
    d.data[1] = 0;

    /* Create done packet */
    done.oper = OPER_GET;
    done.func = GET_DONE;
    done.data[0] = 0;

    /* Send init packet and wait for response */
    while (1) {
        serv_len = sizeof(serv_addr);
        ret = sendto(sock, &init, MSG_SIZE, 0, (struct sockaddr *) &serv_addr, serv_len);
        if (ret < 0) {
            warn("Init packet failure in GET");
            continue;
        }
        ret = recvfrom(sock, &rec, MSG_SIZE, 0, (struct sockaddr *) &serv_addr, &serv_len);
        if (ret < 0) {
            warn("No init packet from server, retransmitting");
            continue;
        }

        file_len =  (int) (rec.data[0] << 24 | rec.data[1] << 16 |
                              rec.data[2] << 8  | rec.data[3] << 0);
        printf("File length is %d\n", file_len);
        break;
    }

    /* Creates data buffer (round up to a frame) */
    fbuf = (char *) malloc(file_len - (file_len % FRAME_SIZE) + FRAME_SIZE);
    if (fbuf == NULL) {
        error("Could not make memory for file");
    }

    /* Calculate total number of packets */
    num_dpkt = (file_len + (FRAME_SIZE - 1)) / FRAME_SIZE;    

    /* Data gathering loop */
    while(1) {
        /* Request packet */
        printf("Requesting pkt %d\n", curr_dpkt);
        d.data[0] = curr_dpkt >> 8;
        d.data[1] = curr_dpkt >> 0;
        ret = sendto(sock, &d, MSG_SIZE, 0, (struct sockaddr *) &serv_addr, serv_len);
        if (ret < 0) {
            warn("Data packet failure");
        }

        /* Recieve packet */
        ret = recvfrom(sock, &rec, MSG_SIZE, 0, (struct sockaddr *) &serv_addr, &serv_len);
        if (ret < 0) {
            warn("Data packet failure");
            continue;
        }

        if (rec.oper != OPER_GET || rec.func != GET_DATA) {
            printf("Recieved invalid packet\n");
            continue;
        }

        /* Decode packet ID */
        pkt_id = rec.data[0] << 8 | rec.data[1] << 0;
        printf("Pkt ID is %d\n", pkt_id);
        if (pkt_id < curr_dpkt) {
            continue;
        }

        /* copy into buffer and mark current packet TODO make smarter, keep track of recieved*/
        memcpy(fbuf + FRAME_SIZE*pkt_id, rec.data + 2, FRAME_SIZE);
        if (pkt_id == curr_dpkt) {
            curr_dpkt++;
        }

        if (curr_dpkt == num_dpkt) {
            break;
        }
    }

    printf("data is %s\n", fbuf);

    /* Save file */
    f = fopen("rec.txt", "wb");
    fwrite(fbuf, 1, file_len, f);
    fclose(f);
    free(fbuf);

    /* Send done */
    while(1) {
        ret = sendto(sock, &done, MSG_SIZE, 0, (struct sockaddr *) &serv_addr, serv_len);
        if (ret < 0) {
            warn("Done packet failure");
            continue;
        }

        /* Recieve done ack packet */
        ret = recvfrom(sock, &rec, MSG_SIZE, 0, (struct sockaddr *) &serv_addr, &serv_len);
        if (ret < 0) {
            warn("Didn't recieve done ack");
            continue;
        }

        if (rec.oper == OPER_GET && rec.func == GET_DONE) {
            printf("Completed get operation\n");
            break;
        }

    }
}

void put(char *file) {

}

void del(char *file) {
    msg_t init;
    msg_t done;
    msg_t rec;
    int serv_len = 0;
    int ret = 0;

    /* Create init packet */
    init.oper = OPER_DEL;
    init.func = DEL_INIT;
    strcpy(init.data, file);

    /* Create done packet */
    done.oper = OPER_DEL;
    done.func = DEL_DONE;
    done.data[0] = 0;

    /* Send init packet and wait for response */
    while (1) {
        serv_len = sizeof(serv_addr);
        ret = sendto(sock, &init, MSG_SIZE, 0, (struct sockaddr *) &serv_addr, serv_len);
        if (ret < 0) {
            warn("Init packet failure in DEL");
            continue;
        }
        ret = recvfrom(sock, &rec, MSG_SIZE, 0, (struct sockaddr *) &serv_addr, &serv_len);
        if (ret < 0) {
            warn("No init packet from server, retransmitting");
            continue;
        }

        break;
    }

    /* Send done */
    while(1) {
        ret = sendto(sock, &done, MSG_SIZE, 0, (struct sockaddr *) &serv_addr, serv_len);
        if (ret < 0) {
            warn("Done packet failure");
            continue;
        }

        /* Recieve done ack packet */
        ret = recvfrom(sock, &rec, MSG_SIZE, 0, (struct sockaddr *) &serv_addr, &serv_len);
        if (ret < 0) {
            warn("Didn't recieve done ack");
            continue;
        }

        if (rec.oper == OPER_GET && rec.func == GET_DONE) {
            if (rec.data[0] == 0) {
                printf("Delete operation failed\n");
            } else {
                printf("Successfully deleted\n");
            }
            break;
        }

    }
}

void ls() {

}

void ex() {
    msg_t init;
    msg_t rec;
    int count = 0;
    int ret = 0;
    int serv_len = 0; 

    /* Create init packet */
    init.oper = OPER_EXIT;
    init.func = EXIT_INIT;

    /* Send init packet and wait for response (try 5 times since there's no done) */
    while (count < 5) {
        count++;
        serv_len = sizeof(serv_addr);
        ret = sendto(sock, &init, MSG_SIZE, 0, (struct sockaddr *) &serv_addr, serv_len);
        if (ret < 0) {
            warn("Init packet failure in EXIT");
            continue;
        }
        ret = recvfrom(sock, &rec, MSG_SIZE, 0, (struct sockaddr *) &serv_addr, &serv_len);
        if (ret < 0) {
            warn("No init packet from server, retransmitting");
            continue;
        }

        break;
    }

    if (count == 5) {
        printf("Exit operation timed out\n");
    } else {
        printf("Server successfully shut down\n");
    }
}

int main(int argc, char **argv) {
    int serv_port = 0;
    char *serv_host;
    char *user_oper;
    char *user_arg;
    char user_temp[64];

    /* Parse server IP and port */
    if (argc != 3) {
        printf("%s", usage);
        exit(1);
    }
    serv_host = argv[1];
    serv_port = atoi(argv[2]);

    /* Create socket */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        error("Error initializing socket\n");
    }

    /* Set socket recieve timeout (200ms) */
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200000;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        error("Error setting socket timeout");
    }

    /* Build server address */
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(serv_port);
    if (inet_pton(AF_INET, serv_host, &serv_addr.sin_addr) <= 0) {
        error("Invalid host address\n");
    }

    /* Send test msg */
#if 0
    char buf[32] = "hello world\n";
    while (1) {
        printf("Sending message\n");
        sendto(sock, buf, 32, 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
        buf[0] = buf[0]+1;
    }
#endif
    /* Get operation from user */
    while (1) {
        fgets(user_temp, 32, stdin);
        user_oper = strtok(user_temp, " \n\t\r");

        /* Select appropriate operation or send invalid message*/
        if (strcmp("get", user_oper) == 0) {
            user_arg = strtok(NULL, " \n\t\r");
            if (user_arg == NULL) {
                printf("Needs an argument for file to get\n");
                continue;
            }
            printf("Sending 'get' command with file %s\n", user_arg);
            get(user_arg);
        } else if (strcmp("put", user_oper) == 0) {
            user_arg = strtok(NULL, " \n\t\r");
            if (user_arg == NULL) {
                printf("Needs an argument for file to put\n");
                continue;
            }
            printf("Sending 'put' command with file %s\n", user_arg);
            put(user_arg);
        } else if (strcmp("del", user_oper) == 0) {
            user_arg = strtok(NULL, " \n\t\r");
            if (user_arg == NULL) {
                printf("Needs an argument for file to delete\n");
                continue;
            }
            printf("Sending 'del' command with file %s\n", user_arg);
            del(user_arg);
        } else if (strcmp("ls", user_oper) == 0) {
            printf("Sending 'ls' command\n");
            ls();
        } else if (strcmp("exit", user_oper) == 0) {
            printf("Sending 'exit' command\n");
            ex();
        } else {
            printf("Invalid option. Options are:\n\tget\n\tput\n\tdel\n\tls\n\texit\n");
            continue;
        }
        
        printf("Completed command\n");

    }   

    return 0;
}
