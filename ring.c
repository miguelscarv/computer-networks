#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_SIZE 128
#define STDIN 0

typedef struct node {
    char key[5];
    char ip[20];
    char port[10]; 
} node;

typedef struct file_descriptors {
    int tcp_succ;
    int tcp_pred;
    int tcp_listen;
    int tcp_temp;
    int udp_server;
    int udp_shortcut;
} file_descriptors;

typedef struct search {
    int current_search_n;
    int my_search_n;
    int my_search_key;
    int search_key;
    int ans_key;
} search;


int TCPserver(node own_node);
int TCPclient(node other_node);
int UDPserver(node own_node);
int dist(int k, int l);
int max(int arr[], size_t n);


int main(int argc, char* argv[]){

    if (argc != 4) {

        printf("Wrong number of command line arguments!\n");
    
    } else {

        int err_code, max_fd, search_n = 0;
        char cmd_buffer[MAX_SIZE], msg_buffer[MAX_SIZE], command[10], trash[100];
        node own_node, pred_node, succ_node, shortcut_node, temp_node;
        file_descriptors all_fds;
        struct sockaddr_in addr;
        int addrlen = sizeof(addr);
        fd_set rset;
        bool has_shortcut = false;
        bool can_exit = false;
        search search;
        struct addrinfo hints, *res;

        search.current_search_n = -1;
        search.my_search_n = -1; //This means this node has still not made any find calls

        strcpy(own_node.key, argv[1]);
        strcpy(own_node.ip, argv[2]);
        strcpy(own_node.port,argv[3]);

        printf("My keys is: %s\n", own_node.key);
        printf("My ip is: %s\n", own_node.ip);
        printf("My port is: %s\n", own_node.port);

        printf("\nInsert a command:\n");
        fgets(cmd_buffer, MAX_SIZE, stdin);
        sscanf(cmd_buffer, "%s", command);
        
        memset(msg_buffer, 0, sizeof(msg_buffer));

        if (strcmp(command,"new") == 0){

            //Creating the TCP Server
            all_fds.tcp_listen = TCPserver(own_node);

            //When starting the pred and succ should be this node itself
            pred_node = own_node;
            succ_node = own_node;

            //Wait for client message
            printf("Waiting for messages...\n\n");
            if ((all_fds.tcp_temp = accept(all_fds.tcp_listen, (struct sockaddr*) &addr, (socklen_t *)&addrlen)) < 0) exit(1);

            //Setting previous and successor node to the new entering node
            err_code = read(all_fds.tcp_temp, msg_buffer, sizeof(msg_buffer));
            if (err_code == -1) exit(1);
            if (sscanf(msg_buffer, "%s %s %s %s", command, succ_node.key, succ_node.ip, succ_node.port) != 4) {
                printf("Ring with a single node received wrong arguments!\n");
                exit(1);
            }

            pred_node = succ_node;
            all_fds.tcp_succ = all_fds.tcp_temp;

            if (strcmp(command, "SELF") == 0) {
                printf("My successor\'s info (Key/IP/Port): %s %s %s\n", succ_node.key, succ_node.ip, succ_node.port);
                printf("My predecessor\'s info (Key/IP/Port): %s %s %s\n\n", pred_node.key, pred_node.ip, pred_node.port);
            } else {
                printf("Did not receive a SELF message as I was expecting...\n");
                exit(1);
            }
            
            //Sending the new node my key, ip and port
            all_fds.tcp_pred = TCPclient(pred_node);

            sprintf(msg_buffer, "SELF %s %s %s", own_node.key, own_node.ip, own_node.port);
            err_code = write(all_fds.tcp_pred, msg_buffer, strlen(msg_buffer));
            if (err_code == -1) exit(1);

            //Creating a UDP server file descriptor
            all_fds.udp_server = UDPserver(own_node);


        } else if (strcmp(command, "pentry") == 0) {

            sscanf(cmd_buffer, "%s %s %s %s", command, pred_node.key, pred_node.ip, pred_node.port);

            //Creating the TCP client file descriptor
            all_fds.tcp_pred = TCPclient(pred_node);

            //Send a SELF message to the predecessor
            sprintf(msg_buffer, "SELF %s %s %s", own_node.key, own_node.ip, own_node.port);
            err_code = write(all_fds.tcp_pred, msg_buffer, strlen(msg_buffer));
            if (err_code == -1) exit(1);

            //Wait for the succesor SELF message
            all_fds.tcp_listen = TCPserver(own_node);
            printf("Waiting for messages...\n\n");
            if ((all_fds.tcp_temp = accept(all_fds.tcp_listen, (struct sockaddr*) &addr, (socklen_t *)&addrlen)) < 0) exit(1);

            memset(msg_buffer, 0, sizeof(msg_buffer));
            err_code = read(all_fds.tcp_temp, msg_buffer, sizeof(msg_buffer));
            if (err_code == -1) exit(1);
            sscanf(msg_buffer, "%s %s %s %s", command, succ_node.key, succ_node.ip, succ_node.port);

            if (strcmp(command, "SELF") == 0) {
                    printf("My successor\'s info (Key/IP/Port): %s %s %s\n", succ_node.key, succ_node.ip, succ_node.port);
                    printf("My predecessor\'s info (Key/IP/Port): %s %s %s\n\n", pred_node.key, pred_node.ip, pred_node.port);
            } else {
                printf("Did not receive a SELF message as I was expecting...\n");
                exit(1);
            }

            all_fds.tcp_succ = all_fds.tcp_temp;
            
            //Creating a UDP server file descriptor
            all_fds.udp_server = UDPserver(own_node);

        } else if (strcmp(command, "exit") == 0) {
            
            printf("\nExiting program...\n");
            exit(0);

        } else {
            printf("Unknown command...\n");
            exit(1);
        }
        

        while(1){
            
            //Getting max_fd needed for select
            int fd_arr[] = {all_fds.tcp_listen, all_fds.tcp_succ, all_fds.tcp_pred, all_fds.udp_server, STDIN};
            max_fd = max(fd_arr, 5) + 1;
            
            FD_ZERO(&rset);
            FD_SET(all_fds.tcp_listen, &rset);
            FD_SET(all_fds.udp_server, &rset);
            if (!can_exit) {
                FD_SET(all_fds.tcp_pred, &rset);
                FD_SET(all_fds.tcp_succ, &rset);
            }
            FD_SET(STDIN, &rset);

            select(max_fd, &rset, NULL, NULL, NULL);
            
            if (FD_ISSET(all_fds.tcp_listen, &rset)){
                
                //printf("\nA new node is trying to enter the ring\n");
                if ((all_fds.tcp_temp = accept(all_fds.tcp_listen, (struct sockaddr*) &addr, (socklen_t *)&addrlen)) < 0) exit(1);

                //Read from new node
                memset(msg_buffer, 0, sizeof(msg_buffer));
                memset(command, 0, sizeof(command));

                memset(temp_node.key,0,sizeof(temp_node.key));
                memset(temp_node.ip,0,sizeof(temp_node.ip));
                memset(temp_node.port,0,sizeof(temp_node.port));

                err_code = read(all_fds.tcp_temp, msg_buffer, sizeof(msg_buffer));
                if (err_code == -1) exit(1);
                if (sscanf(msg_buffer, "%s %s %s %s", command, temp_node.key, temp_node.ip, temp_node.port) != 4) exit(1);
                
                //printf("\nI received a message: %s from key %s", msg_buffer, temp_node.key);

                if (strcmp(command, "SELF") == 0) {

                    //Tell successor about his new predecessor 
                    if (dist(atoi(own_node.key), atoi(succ_node.key)) > dist(atoi(own_node.key), atoi(temp_node.key))) {
                        
                        memset(msg_buffer, 0, sizeof(msg_buffer));
                        sprintf(msg_buffer, "PRED %s %s %s", temp_node.key, temp_node.ip, temp_node.port);
                        err_code = write(all_fds.tcp_succ, msg_buffer, strlen(msg_buffer));
                        if (err_code == -1) exit(1);

                    } else {
                        printf("My successor with key %s left the ring!\n\n", succ_node.key);
                    }

                    close(all_fds.tcp_succ);


                    //Update successor fd and info
                    all_fds.tcp_succ = all_fds.tcp_temp;
                    succ_node = temp_node;

                }

            }

            if (FD_ISSET(all_fds.tcp_pred, &rset)) {

                memset(msg_buffer, 0, sizeof(msg_buffer));
                memset(command, 0, sizeof(command));

                memset(temp_node.key,0,sizeof(temp_node.key));
                memset(temp_node.ip,0,sizeof(temp_node.ip));
                memset(temp_node.port,0,sizeof(temp_node.port));

                err_code = read(all_fds.tcp_pred, msg_buffer, sizeof(msg_buffer));
                if (err_code == -1) exit(1);

                if (strstr(msg_buffer,"PRED") != NULL) {

                    sscanf(msg_buffer, "%s %s %s %s", command, temp_node.key, temp_node.ip, temp_node.port);
        
                    //Close connection with current predecessor and update it
                    close(all_fds.tcp_pred);
                    pred_node = temp_node;

                    //Sending a SELF message to my predecessor
                    all_fds.tcp_pred = TCPclient(pred_node);

                    memset(msg_buffer, 0, sizeof(msg_buffer));
                    sprintf(msg_buffer, "SELF %s %s %s", own_node.key, own_node.ip, own_node.port);
                    err_code = write(all_fds.tcp_pred, msg_buffer, strlen(msg_buffer));
                    if (err_code == -1) exit(1);

                } else if(strstr(msg_buffer,"FND") != NULL){

                    sscanf(msg_buffer, "%s %d %d %s %s %s", command, &search.search_key, &search.current_search_n, temp_node.key, temp_node.ip, temp_node.port);

                    if (dist(atoi(own_node.key), search.search_key) < dist(atoi(succ_node.key), search.search_key)){
                         
                        memset(msg_buffer, 0, sizeof(msg_buffer));

                        if (!has_shortcut) {

                            //Propagate the RSP message through successor
                            sprintf(msg_buffer, "RSP %s %d %s %s %s", temp_node.key, search.current_search_n, own_node.key, own_node.ip, own_node.port);
                            err_code = write(all_fds.tcp_succ, msg_buffer, strlen(msg_buffer));
                            if (err_code == -1) exit(1);

                        } else {

                            if ((dist(atoi(shortcut_node.key), search.search_key) < dist(atoi(succ_node.key), search.search_key)) && (strcmp(succ_node.key, temp_node.key) != 0)) {

                                //Propagate the RSP message through shortcut
                                sprintf(msg_buffer, "RSP %s %d %s %s %s", temp_node.key, search.current_search_n, own_node.key, own_node.ip, own_node.port);
                                err_code = sendto(all_fds.udp_shortcut, msg_buffer, strlen(msg_buffer), 0, res->ai_addr, res->ai_addrlen);
                                if (err_code == -1) exit(1);

                                memset(msg_buffer, 0, sizeof(msg_buffer));

                                //Wait for ACK
                                err_code = recvfrom(all_fds.udp_shortcut, msg_buffer, sizeof(msg_buffer), 0, (struct sockaddr*) &addr, (socklen_t *)&addrlen);
                                if (strcmp(msg_buffer, "ACK") == 0) {
                                    printf("\nSent a RSP message through my shortcut and received an ACK!\n\n");
                                } else {
                                    printf("\nSomething went wrong - did not receive an ACK...\n\n");
                                }

                            } else {

                                //Propagate the RSP message through successor
                                sprintf(msg_buffer, "RSP %s %d %s %s %s", temp_node.key, search.current_search_n, own_node.key, own_node.ip, own_node.port);
                                err_code = write(all_fds.tcp_succ, msg_buffer, strlen(msg_buffer));
                                if (err_code == -1) exit(1);

                            }

                        }

                    } else {
                        
                        if (!has_shortcut) {

                            //Propagate the FND message to the successor
                            err_code = write(all_fds.tcp_succ, msg_buffer, strlen(msg_buffer));
                            if (err_code == -1) exit(1);

                        } else {

                            if (dist(atoi(shortcut_node.key), search.search_key) < dist(atoi(succ_node.key), search.search_key)) {

                                //Propagate the FND message through shortcut
                                err_code = sendto(all_fds.udp_shortcut, msg_buffer, strlen(msg_buffer), 0, res->ai_addr, res->ai_addrlen);
                                if (err_code == -1) exit(1);

                                memset(msg_buffer, 0, sizeof(msg_buffer));

                                //Wait for ACK
                                err_code = recvfrom(all_fds.udp_shortcut, msg_buffer, sizeof(msg_buffer), 0, (struct sockaddr*) &addr, (socklen_t *)&addrlen);
                                if (strcmp(msg_buffer, "ACK") == 0) {
                                    printf("\nSent a FND message through my shortcut and received an ACK!\n\n");
                                } else {
                                    printf("\nSomething went wrong - did not receive an ACK...\n\n");
                                }

                            } else {

                                //Propagate the FND message to the successor
                                err_code = write(all_fds.tcp_succ, msg_buffer, strlen(msg_buffer));
                                if (err_code == -1) exit(1);

                            }

                        }
                    }
                    
                } else if(strstr(msg_buffer,"RSP") != NULL) {

                    sscanf(msg_buffer, "%s %d %d %s %s %s", command, &search.ans_key, &search.current_search_n, temp_node.key, temp_node.ip, temp_node.port);

                    if (search.ans_key == atoi(own_node.key)) {

                        printf("The answer is the node (Key/IP/Port): %s %s %s\n\n", temp_node.key, temp_node.ip, temp_node.port);
     
                    } else {

                        if (!has_shortcut){

                            err_code = write(all_fds.tcp_succ, msg_buffer, strlen(msg_buffer));
                            if (err_code == -1) exit(1);

                        } else {

                            if (dist(atoi(shortcut_node.key), search.ans_key) < dist(atoi(succ_node.key), search.ans_key)) {

                                //Propagate the FND message through shortcut
                                err_code = sendto(all_fds.udp_shortcut, msg_buffer, strlen(msg_buffer), 0, res->ai_addr, res->ai_addrlen);
                                if (err_code == -1) exit(1);

                                memset(msg_buffer, 0, sizeof(msg_buffer));

                                //Wait for ACK
                                err_code = recvfrom(all_fds.udp_shortcut, msg_buffer, sizeof(msg_buffer), 0, (struct sockaddr*) &addr, (socklen_t *)&addrlen);
                                if (strcmp(msg_buffer, "ACK") == 0) {
                                    printf("\nSent a RSP message through my shortcut and received an ACK!\n\n");
                                } 

                            } else {

                                //Propagate the FND message to the successor
                                err_code = write(all_fds.tcp_succ, msg_buffer, strlen(msg_buffer));
                                if (err_code == -1) exit(1);

                            }

                        }

                        
                    }

                }

            }
            
            if (FD_ISSET(all_fds.tcp_succ, &rset)) {
                
                //Finish code - may not even need this at all...
                memset(msg_buffer, 0, sizeof(msg_buffer));
                err_code = read(all_fds.tcp_succ, msg_buffer, sizeof(msg_buffer));
            
            }

            if (FD_ISSET(all_fds.udp_server, &rset)) {

                //Read message
                memset(msg_buffer, 0, sizeof(msg_buffer));
                err_code = recvfrom(all_fds.udp_server, msg_buffer, sizeof(msg_buffer), 0, (struct sockaddr*) &addr, (socklen_t *)&addrlen);
                if (err_code == -1) exit(1);

                //Sending back an ACK 
                err_code = sendto(all_fds.udp_server, "ACK", 4, 0, (struct sockaddr*) &addr, addrlen);
                if (err_code == -1) exit(1);

                if(strstr(msg_buffer,"FND") != NULL){

                    sscanf(msg_buffer, "%s %d %d %s %s %s", command, &search.search_key, &search.current_search_n, temp_node.key, temp_node.ip, temp_node.port);

                    if (dist(atoi(own_node.key), search.search_key) < dist(atoi(succ_node.key), search.search_key)){
                         
                        memset(msg_buffer, 0, sizeof(msg_buffer));

                        if (!has_shortcut) {

                            //Propagate the RSP message through successor
                            sprintf(msg_buffer, "RSP %s %d %s %s %s", temp_node.key, search.current_search_n, own_node.key, own_node.ip, own_node.port);
                            err_code = write(all_fds.tcp_succ, msg_buffer, strlen(msg_buffer));
                            if (err_code == -1) exit(1);

                        } else {

                            if (dist(atoi(shortcut_node.key), search.search_key) < dist(atoi(succ_node.key), search.search_key)) {

                                //Propagate the RSP message through shortcut
                                sprintf(msg_buffer, "RSP %s %d %s %s %s", temp_node.key, search.current_search_n, own_node.key, own_node.ip, own_node.port);
                                err_code = sendto(all_fds.udp_shortcut, msg_buffer, strlen(msg_buffer), 0, res->ai_addr, res->ai_addrlen);
                                if (err_code == -1) exit(1);

                                memset(msg_buffer, 0, sizeof(msg_buffer));

                                //Wait for ACK
                                err_code = recvfrom(all_fds.udp_shortcut, msg_buffer, sizeof(msg_buffer), 0, (struct sockaddr*) &addr, (socklen_t *)&addrlen);
                                if (strcmp(msg_buffer, "ACK") == 0) {
                                    printf("\nSent a RSP message through my shortcut and received an ACK!\n\n");
                                } else {
                                    printf("\nSomething went wrong - did not receive an ACK...\n\n");
                                }

                            } else {

                                //Propagate the RSP message through successor
                                sprintf(msg_buffer, "RSP %s %d %s %s %s", temp_node.key, search.current_search_n, own_node.key, own_node.ip, own_node.port);
                                err_code = write(all_fds.tcp_succ, msg_buffer, strlen(msg_buffer));
                                if (err_code == -1) exit(1);

                            }

                        }

                    } else {
                        
                        if (!has_shortcut) {

                            //Propagate the FND message to the successor
                            err_code = write(all_fds.tcp_succ, msg_buffer, strlen(msg_buffer));
                            if (err_code == -1) exit(1);

                        } else {

                            if (dist(atoi(shortcut_node.key), search.search_key) < dist(atoi(succ_node.key), search.search_key)) {

                                //Propagate the FND message through shortcut
                                err_code = sendto(all_fds.udp_shortcut, msg_buffer, strlen(msg_buffer), 0, res->ai_addr, res->ai_addrlen);
                                if (err_code == -1) exit(1);

                                memset(msg_buffer, 0, sizeof(msg_buffer));

                                //Wait for ACK
                                err_code = recvfrom(all_fds.udp_shortcut, msg_buffer, sizeof(msg_buffer), 0, (struct sockaddr*) &addr, (socklen_t *)&addrlen);
                                if (strcmp(msg_buffer, "ACK") == 0) {
                                    printf("\nSent a FND message through my shortcut and received an ACK!\n\n");
                                } else {
                                    printf("\nSomething went wrong - did not receive an ACK...\n\n");
                                }

                            } else {

                                //Propagate the FND message to the successor
                                err_code = write(all_fds.tcp_succ, msg_buffer, strlen(msg_buffer));
                                if (err_code == -1) exit(1);

                            }

                        }
                    }
                    
                } else if(strstr(msg_buffer,"RSP") != NULL) {

                    sscanf(msg_buffer, "%s %d %d %s %s %s", command, &search.ans_key, &search.current_search_n, temp_node.key, temp_node.ip, temp_node.port);

                    if (search.ans_key == atoi(own_node.key)) {

                        printf("The answer is the node (Key/IP/Port): %s %s %s\n\n", temp_node.key, temp_node.ip, temp_node.port);
     
                    } else {

                        if (!has_shortcut){

                            err_code = write(all_fds.tcp_succ, msg_buffer, strlen(msg_buffer));
                            if (err_code == -1) exit(1);

                        } else {

                            if (dist(atoi(shortcut_node.key), search.ans_key) < dist(atoi(succ_node.key), search.ans_key)) {

                                //Propagate the FND message through shortcut
                                err_code = sendto(all_fds.udp_shortcut, msg_buffer, strlen(msg_buffer), 0, res->ai_addr, res->ai_addrlen);
                                if (err_code == -1) exit(1);

                                memset(msg_buffer, 0, sizeof(msg_buffer));

                                //Wait for ACK
                                err_code = recvfrom(all_fds.udp_shortcut, msg_buffer, sizeof(msg_buffer), 0, (struct sockaddr*) &addr, (socklen_t *)&addrlen);
                                if (strcmp(msg_buffer, "ACK") == 0) {
                                    printf("\nSent a FND message through my shortcut and received an ACK!\n\n");
                                } else {
                                    printf("\nSomething went wrong - did not receive an ACK...\n\n");
                                }

                            } else {

                                //Propagate the FND message to the successor
                                err_code = write(all_fds.tcp_succ, msg_buffer, strlen(msg_buffer));
                                if (err_code == -1) exit(1);

                            }

                        }

                        
                    }

                }

            }

            if (FD_ISSET(STDIN, &rset)) {

                fgets(cmd_buffer, MAX_SIZE, stdin);
                
                if (strcmp(cmd_buffer,"show\n") == 0) {
                    printf("\nMy info (Key/IP/Port): %s %s %s\n", own_node.key, own_node.ip, own_node.port);
                    printf("My successor\'s info (Key/IP/Port): %s %s %s\n", succ_node.key, succ_node.ip, succ_node.port);
                    printf("My predecessor\'s info (Key/IP/Port): %s %s %s\n", pred_node.key, pred_node.ip, pred_node.port);
                    if (has_shortcut) {
                        printf("My shortcut\'s info (Key/IP/Port): %s %s %s\n\n", shortcut_node.key, shortcut_node.ip, shortcut_node.port);
                    } else {
                        printf("I don\'t have a shortcut...\n\n");
                    }
                }

                if (strcmp(cmd_buffer,"leave\n") == 0) {

                    //Send successor message about his new predecessor
                    memset(msg_buffer, 0, sizeof(msg_buffer));

                    sprintf(msg_buffer, "PRED %s %s %s", pred_node.key, pred_node.ip, pred_node.port);
                    err_code = write(all_fds.tcp_succ, msg_buffer, strlen(msg_buffer));
                    if (err_code == -1) exit(1);
                    

                    //Close all open connections
                    close(all_fds.tcp_succ);
                    close(all_fds.tcp_pred);
                    if (has_shortcut) {
                        close(all_fds.udp_shortcut);
                    }

                    can_exit = true;
                    printf("\nI left the ring...\n\n");

                }

                if (strcmp(cmd_buffer, "exit\n") == 0) {

                    if (can_exit) {
                        printf("\nExiting the program...\n");
                        exit(0);
                    } else {
                        printf("I am still in the ring - you first need to call \"leave\"!\n\n");
                    }

                }

                if (strstr(cmd_buffer, "find") != NULL) {

                    sscanf(cmd_buffer, "%s %d", trash, &search.my_search_key);

                    if (dist(atoi(own_node.key), search.my_search_key) < dist(atoi(succ_node.key), search.my_search_key)) {

                        printf("\nThat key belongs to this node...\n\n");
                    
                    } else {

                        if (search.current_search_n < 98) {
                            search.current_search_n++;
                        } else {
                            search.current_search_n = 0;
                        }

                        memset(msg_buffer, 0, sizeof(msg_buffer));
                        
                        printf("\nYou want to find key %d - search number %d\n", search.my_search_key, search.current_search_n);
                        sprintf(msg_buffer, "FND %d %d %s %s %s", search.my_search_key, search.current_search_n, own_node.key, own_node.ip, own_node.port);

                        if (!has_shortcut) {

                            //Send FND to successor
                            err_code = write(all_fds.tcp_succ, msg_buffer, strlen(msg_buffer));
                            if (err_code == -1) exit(1);

                        } else {    

                            if (dist(atoi(shortcut_node.key), search.search_key) < dist(atoi(succ_node.key), search.search_key)) {

                                //Propagate the FND message through shortcut
                                err_code = sendto(all_fds.udp_shortcut, msg_buffer, strlen(msg_buffer), 0, res->ai_addr, res->ai_addrlen);
                                if (err_code == -1) exit(1);

                                memset(msg_buffer, 0, sizeof(msg_buffer));

                                //Wait for ACK
                                err_code = recvfrom(all_fds.udp_shortcut, msg_buffer, sizeof(msg_buffer), 0, (struct sockaddr*) &addr, (socklen_t *)&addrlen);
                                if (strcmp(msg_buffer, "ACK") == 0) {
                                    printf("\nSent a FND message through my shortcut and received an ACK!\n\n");
                                } else {
                                    printf("\nSomething went wrong - did not receive an ACK...\n\n");
                                }

                            } else {

                                //Propagate the FND message to the successor
                                err_code = write(all_fds.tcp_succ, msg_buffer, strlen(msg_buffer));
                                if (err_code == -1) exit(1);

                            }

                        }

                    }
                    
                }

                if (strstr(cmd_buffer, "chord") != NULL && strstr(cmd_buffer, ".") != NULL) {

                    if (has_shortcut) {

                        printf("\nYou already have a shortcut - you can only have one!\n\n");

                    } else {

                        sscanf(cmd_buffer, "%s %s %s %s", trash, shortcut_node.key, shortcut_node.ip, shortcut_node.port);

                        all_fds.udp_shortcut = socket(AF_INET, SOCK_DGRAM,0);
                        if (all_fds.udp_shortcut == -1) exit(1);

                        memset(&hints, 0, sizeof(hints));
                        hints.ai_family = AF_INET;
                        hints.ai_socktype = SOCK_DGRAM;

                        err_code = getaddrinfo(shortcut_node.ip, shortcut_node.port, &hints, &res);
                        if (err_code != 0) exit(1);

                        has_shortcut = true;
                        printf("\nYou added a new shortcut!\n\n");

                    }

                }

                if (strcmp(cmd_buffer, "echord\n") == 0) {

                    freeaddrinfo(res);
                    close(all_fds.udp_shortcut);
                    has_shortcut = false;

                    printf("\nEliminated my shortcut...\n\n");
                }

            }

        }

    }

    return 0;
}

//Creates and returns the TCP Server File Descriptor
int TCPserver(node own_node) {

    int server_fd, err_code;
    struct addrinfo hints, *res;

    server_fd = socket(AF_INET,SOCK_STREAM,0);
    if (server_fd == -1) exit(1);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    err_code = getaddrinfo(own_node.ip, own_node.port, &hints, &res);
    if ((err_code) != 0) exit(1);

    if (bind(server_fd, res->ai_addr, res->ai_addrlen) < 0) exit(1);

    if (listen(server_fd, 32) == -1) exit(1);

    return server_fd;
}

//Creates and returns the TCP Client File Descriptor
int TCPclient(node other_node) {

    int client_fd, err_code;
    struct addrinfo hints, *res;

    client_fd = socket(AF_INET,SOCK_STREAM,0);
    if (client_fd == -1) exit(1);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    err_code = getaddrinfo(other_node.ip, other_node.port, &hints, &res);
    if ((err_code) != 0) exit(1);

    connect(client_fd, res->ai_addr, res->ai_addrlen);

    return client_fd;
}

//Creates and returns the UDP Server File Descriptor
int UDPserver(node own_node) {

    int server_fd, err_code;
    struct addrinfo hints, *res;

    server_fd = socket(AF_INET,SOCK_DGRAM,0);
    if (server_fd == -1) exit(1);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    err_code = getaddrinfo(own_node.ip, own_node.port, &hints, &res);
    if ((err_code) != 0) exit(1);

    if (bind(server_fd, res->ai_addr, res->ai_addrlen) < 0) exit(1);

    return server_fd;
}

//Distance for guiding the search
int dist(int k, int l){
    int N = 32;
    return ((l-k)%N + N)%N;
}


//Max function that accepts any number of File Descriptors to be used with select
int max(int arr[], size_t n) {

    int i;
    int max = arr[0];

    for (i = 0; i < n; i++){
        if (arr[i] > max)
            max = arr[i];
    }

    return max;
}