/**************************************
 * Network Systems Project 1
 * Server Code
 * Ben Heberlein
 * 
 * This file implements the server-side
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
    uint8_t  data[DATA_SIZE];
} msg_t;

/* Usage message */
char usage[128] = "server <port>\n";

/* Socket parameters */
int sock = 0;
struct sockaddr_in serv_addr;
struct sockaddr_in client_addr;
int client_len = 0;

/* Error handler */
void error(char *msg) {
    perror(msg);
    exit(2);
}

/* Warning handler */
void warn(char *msg) {
    perror(msg);
}

/* Get operation server side */
void get(msg_t *rec) {
    int ret = 0;
    msg_t init;
    msg_t d;
    msg_t done;
    int file_len = 0;
    FILE *f;   
    char *fbuf;
    int num_dpkt = 0;
    int curr_dpkt = 0;
 
    /* Create init response */
    init.oper = OPER_GET;
    init.func = GET_INIT;
    init.data[0] = 0;

    /* Create data response */
    d.oper = OPER_GET;
    d.func = GET_DATA;
    d.data[0] = 0;

    /* Create done packet */
    done.oper = OPER_GET;
    done.func = GET_DONE;  
    done.data[0] = 0;

    while(1) { 

        /* Send init response  with file size */
        if (rec->oper == OPER_GET  && rec->func == GET_INIT) {
            printf("Filename is %s\n",rec->data);

            /* Open file buffer */
            f = fopen(rec->data, "r");
            if (f == NULL) {
                warn("Couldn't open file");
                file_len = 0;
                init.data[0] = file_len >> 24;
                init.data[1] = file_len >> 16;
                init.data[2] = file_len >> 8;
                init.data[3] = file_len >> 0;

                ret = sendto(sock, &init, MSG_SIZE, 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
                break;
            }

            /* Get file size */
            fseek(f, 0, SEEK_END);
            file_len = ftell(f);
            fseek(f, 0, SEEK_SET);
           
            /* Load file (round up to a frame) */ 
            fbuf = malloc(file_len - (file_len % FRAME_SIZE) + FRAME_SIZE);
            fread(fbuf, file_len, 1, f);
            fclose(f);

            printf("data is %s\n", fbuf);

            /* Calculate number of packets */
            num_dpkt = (file_len + (FRAME_SIZE - 1)) / FRAME_SIZE;
            curr_dpkt = 0;
    
            /* Set file size */
            init.data[0] = file_len >> 24;
            init.data[1] = file_len >> 16;
            init.data[2] = file_len >> 8;
            init.data[3] = file_len >> 0;

            /* Send init response */
            ret = sendto(sock, &init, MSG_SIZE, 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
            if (ret < 0) {
                warn("Init response failure in GET");
                free(fbuf);
                continue;
            }
        }

        /* Request for missing packet */
        if (rec->oper == OPER_GET  && rec->func == GET_DATA) {
            curr_dpkt = rec->data[0] << 8 | rec->data[1] << 0;
            printf("packet is %d\n", curr_dpkt);

            /* Send next ten packets */
            for (int i = curr_dpkt; i < curr_dpkt + 10; i++) {
                if (i < num_dpkt) {
                    d.data[0] = i >> 8;
                    d.data[1] = i >> 0;
                    memcpy(d.data + 2, fbuf + FRAME_SIZE*i, FRAME_SIZE);
                    ret = sendto(sock, &d, MSG_SIZE, 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
                    if (ret < 0) {
                        warn("Data response failure in GET");
                    }
                }
            }
        }

        /* Agree that we are done */
        if  (rec->oper == OPER_GET && rec->func == GET_DONE) {
            ret = sendto(sock, &done, MSG_SIZE, 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
            if (ret < 0) {
                warn("Done response failure in GET");
            }

            /* Can break out of loop since another GET DONE from client puts us back in loop */
            break;
        }

        /* Get packet from client */
        ret = recvfrom(sock, rec, MSG_SIZE, 0 , (struct sockaddr *) &client_addr, &client_len);
        if (ret < 0) {
            warn("Recieve failure in GET");
        }
    }
}

void put(msg_t *rec) {

}

void del(msg_t *rec) {
    msg_t init;
    msg_t done;
    char file_name[64];
    int success = 0;
    int ret = 0;
    FILE *f;

    /* Create init response */
    init.oper = OPER_DEL;
    init.func = DEL_INIT;
    init.data[0] = 0;

    /* Create done packet */
    done.oper = OPER_GET;
    done.func = GET_DONE;
    done.data[0] = 0;

    while(1) {
        
        /* Send init response */
        if (rec->oper == OPER_DEL && rec->func == DEL_INIT) {
            printf("Filename is %s\n", rec->data);
            strcpy(file_name, rec->data);

            /* Send init response */
            ret = sendto(sock, &init, MSG_SIZE, 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
            if (ret < 0) {
                warn("Init response failure in DEL");
                continue;
            }

            /* Try to delete file, set success (default 0) */
            f = fopen(file_name, "rb");   
            if (f != NULL) {
                fclose(f);
                remove(file_name);
                f = fopen(file_name, "rb");
                if (f == NULL) {
                    success = 1;
                } else {
                    fclose(f);
                }
            }
        }    

        /* Send done with success value */
        if (rec->oper == OPER_DEL && rec->func == DEL_DONE) {
            done.data[0] = success;
            ret = sendto(sock, &done, MSG_SIZE, 0, (struct sockaddr *) &client_addr, sizeof(client_addr));

            if (ret < 0) {
                warn("Done response failure in DEL");
            }

            /* Can break out of loop since another GET DONE from client puts us back in loop */
            break;
        }

        /* Get packet from client */
        ret = recvfrom(sock, rec, MSG_SIZE, 0 , (struct sockaddr *) &client_addr, &client_len);
        if (ret < 0) {
            warn("Recieve failure in DEL");
        }
    } 
}

void ls(msg_t *rec) {

}

void ex(msg_t *rec) {
    msg_t init;
    int ret;
    
    /* Create init response */
    init.oper = OPER_EXIT;
    init.func = EXIT_INIT;
    init.data[0] = 0;

    while(1) {

        /* Send init response */
        if (rec->oper == OPER_EXIT && rec->func == EXIT_INIT) {
            printf("Shutting down server...\n");

            /* Send init response multiple times since we are shutting down */
            for (int i = 0; i < 10; i++) {
                ret = sendto(sock, &init, MSG_SIZE, 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
                if (ret < 0) {
                    warn("Init response failure in DEL");
                    continue;
                }
            }

            /* Shutdown socket and exit */
            ret = close(sock);
            if (ret < 0) {
                warn("Couldn't shut down socket");
                printf("Forcefully quitting - Goodbye!\n");
                exit(0);
            } else {
                printf("Successfully shut down socket\n");
                printf("Goodbye!\n");
                exit(0);
            }
        }

        /* Get packet from client */
        ret = recvfrom(sock, rec, MSG_SIZE, 0 , (struct sockaddr *) &client_addr, &client_len);
        if (ret < 0) {
            warn("Recieve failure in EXIT");
        }
    }
}

int main(int argc, char **argv) {
    int serv_port = 0;
    int optval = 0; 
    msg_t rec;
    int ret = 0;

    /* Parse server IP and port */
    if (argc != 2) {
        printf("%s", usage);
        exit(1);
    }
    serv_port = atoi(argv[1]);

    /* Create socket */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        error("Error initializing socket");
    }

    /* Allow quick release of socket */
    optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval, sizeof(int));

    /* Create server IP and port */
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons((unsigned short) serv_port);

    /* Bind to port */
    if (bind(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("Error binding socket");
    }

    printf("Waiting for command...\n");

    client_len = sizeof(client_addr);
    while(1) {
        ret = recvfrom(sock, &rec, MSG_SIZE, 0, (struct sockaddr *) &client_addr, &client_len);
        if (ret < 0) {
            warn("Issue recieving packet");
            continue;
        }
#if 0        
        if (rec.oper == OPER_GET  && rec.func == GET_INIT) {
            ret = sendto(sock, &rec, MSG_SIZE, 0, (struct sockaddr *) &client_addr, client_len);
            if (ret < 0) {
                warn("Init response failure in GET");
                continue;
            }
        }

#endif
        switch(rec.oper) {
            case OPER_GET:
                get(&rec);
                break;
            case OPER_PUT:
                put(&rec);
                break;
            case OPER_DEL:
                del(&rec);
                break;
            case OPER_LS:
                ls(&rec);
                break;
            case OPER_EXIT:
                ex(&rec);
                break;
            default:
                warn("Recieved packet with invalid operation\n");
                break;
        }
    }

    return 0;
}
