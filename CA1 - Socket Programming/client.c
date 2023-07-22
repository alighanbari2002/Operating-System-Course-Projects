#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <sys/time.h>

#define EMPTY_STR '\0'

#define LARG_BUF 5000
#define MAX_BUF 1204
#define ALARM_TIME 60

#define MAX_MSG_LEN 512
#define WRITE_FD 1
#define READ_FD 0
#define ERR_FD 2
#define NUM_OF_STUDENTS 3
#define SO_REUSEPORT	15
#define ANSWER_TIME 60

#define SO_REUSEPORT 15 

#define MEETING_TIMEOUT 60

#define STD_IN 0
#define STD_OUT 1
#define STD_ERR 2

#define SERVER_ADDRESS "127.0.0.1"
#define BROADCAST_IP "192.168.1.255"
// to find proper IP, use ifconfig in linux terminal. And put broadcast IP.

#define TA 1
#define STUDENT 2

#define ASK_QUESTION 1
#define GET_RUNNING_MEETINGS 2

int connect_to_server(int port) {
    int fd;
    struct sockaddr_in server_address;
    
    fd = socket(AF_INET, SOCK_STREAM, 0);
    
    server_address.sin_family = AF_INET; 
    server_address.sin_port = htons(port); 
    server_address.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);

    if (connect(fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        printf("Error in connecting to server\n");
        return 0;
    }

    return fd;
}

void print_menu(int ROLE) {
    char buffer[MAX_MSG_LEN] = {0};
    memset(buffer, 0, MAX_MSG_LEN);
    system("clear");
    if(ROLE == STUDENT) {
        sprintf(buffer, "hey, You are logged in as a (student), the things you can do are as"
        " follows:\n [1] ask a question\n [2] Get a list of running sessions\n");
        write(WRITE_FD, buffer, strlen(buffer));
    }
    else if(ROLE == TA) {
        sprintf(buffer, "hey, You are logged in as a (TA)\n");
        write(WRITE_FD, buffer, strlen(buffer));
        write(WRITE_FD, "type command <ls> to list all questions for you.\n", strlen("type command <ls> to list all questions for you.\n"));
        // write(WRITE_FD, " This is a list of all not answered questions here, you can select them by ID:\n\n",
        //         strlen(" This is a list of all not answered questions here, you can select them by ID:\n\n"));
    }
}

void STDIN_handler(int server_fd, int ROLE) {
    char buffer[MAX_MSG_LEN] = {0};
    memset(buffer, 0, MAX_MSG_LEN);
    if(ROLE == STUDENT) {
        read(READ_FD, buffer, MAX_MSG_LEN);
        int choice = atoi(buffer);
        if(choice == ASK_QUESTION) {
            write(WRITE_FD, "\nType your question below:\n", strlen("\nType your question below:\n"));
            char question[MAX_MSG_LEN - 20] = {0};
            memset(question, 0, MAX_MSG_LEN - 20);
            read(READ_FD, question, MAX_MSG_LEN - 20);
            memset(buffer, 0, MAX_MSG_LEN);
            sprintf(buffer, "new_question: %s", question);
            send(server_fd, buffer, strlen(buffer), 0);
        }
    }
    if(ROLE == TA) {
        read(READ_FD, buffer, MAX_MSG_LEN);
        if(strcmp(buffer, "ls\n") == 0) {
            printf("ls command received\n");
        }
        send(server_fd, "list_questions:", strlen("list_questions:"), 0);
    }
}

void start_conversation(int server_fd, int port) {
    printf("come to conversation\n");
    struct sockaddr_in sock_adr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket < 0)
    {
        write(ERR_FD, "error socket!\n", strlen("error socket!\n"));
    }
    // broadcast specs:
    sock_adr.sin_port = htons(port);
    sock_adr.sin_family = AF_INET;
    sock_adr.sin_addr.s_addr = inet_addr(BROADCAST_IP);
    int broadcast = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
         write(ERR_FD, "error setsocketopt broadcast!\n", strlen("error setsocketopt broadcast!\n")); 
    }
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &broadcast, sizeof(broadcast))) {
        write(ERR_FD, "error setsocket reuse!\n", strlen("error setsocket reuse!\n")); 
    }
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &broadcast, sizeof(broadcast))) {
        write(ERR_FD, "error setsocket reuse!\n", strlen("error setsocket reuse!\n")); 
    }
    if(bind(sock, (const struct sockaddr*) &sock_adr, (socklen_t)sizeof(sock_adr)) < 0) {
        write(ERR_FD, "error binding!\n", strlen("error binding!\n")); 
    }
    char buf[MAX_MSG_LEN] = {0};
    int sock_adr_len = sizeof(sock_adr);
    fd_set master_set, working_set;
    FD_ZERO(&master_set);
    FD_SET(STD_IN, &master_set);
    FD_SET(sock, &master_set);
    FD_SET(server_fd, &master_set);

    while(1)
    {
        working_set = master_set;
        select(sock + 1, &working_set, NULL, NULL, NULL);
        if (FD_ISSET(STD_IN, &working_set)) {
            memset(buf, 0, MAX_MSG_LEN);
            read(0, buf, MAX_MSG_LEN);
            //alarm(0);
            if(strcmp(buf, "exit\n") == 0) {
                sendto(sock, buf, strlen(buf), 0,(struct sockaddr *)&sock_adr, sizeof(sock_adr));
                send(server_fd, "exit_meeting:", strlen("exit_meeting:"), 0);
                return;
            }
            int is_valid = sendto(sock, buf, strlen(buf), 0,(struct sockaddr *)&sock_adr, sizeof(sock_adr));
            if (is_valid <= 0) {
                write(ERR_FD, "send ran to problem!\n", strlen("send ran to problem!\n"));;
            }
        }
        else if (FD_ISSET(sock, &working_set)) {
            // int rch = recvfrom(sock, buf, MAX_MSG_LEN, 0,
            // (struct sockaddr*) &sock_adr, (socklen_t*)&sock_adr_len);
            memset(buf, 0, MAX_MSG_LEN);
            int rch = recv(sock, buf, MAX_MSG_LEN, 0);
            if(rch <= 0)
            {
                write(ERR_FD, "recv ran to problem!\n", strlen("recv ran to problem!\n"));;
            }
            write(WRITE_FD, buf, strlen(buf));
        }
    }
}

void tokenizing_message(int server_fd, int ROLE, char buffer[]) {
    int to_broadcast_fd = 0, meeting_port = 0;
    char tokenizing_buffer[MAX_MSG_LEN] = {0};
    memset(tokenizing_buffer, 0, MAX_MSG_LEN);
    strncpy(tokenizing_buffer, buffer, MAX_MSG_LEN);
    char* stoken = strtok(tokenizing_buffer, " ");

    if(ROLE == TA && strcmp(stoken, "list_questions:") == 0)
    {
        int token_size = strlen(stoken) + 1, index = 0;
        for(index = 0; index < strlen(buffer) - token_size; index++) {
            buffer[index] = buffer[index + token_size];
        }
        buffer[index] = '\0';
        write(WRITE_FD, buffer, strlen(buffer));
        if(strcmp(buffer, "no questions to display!\n") != 0) {
            write(WRITE_FD, "\nthe ID you want to choose: ", strlen("\nthe ID you want to choose: "));
            memset(buffer, 0, LARG_BUF);
            read(READ_FD, buffer, LARG_BUF);
            int selected_ID = atoi(buffer);
            memset(buffer, 0, LARG_BUF);
            sprintf(buffer,"TA_ID_selection: %d", selected_ID);
            send(server_fd, buffer, strlen(buffer), 0);
        }
        else {
            write(WRITE_FD, "\nenter something to reload...\n",
             strlen("\nenter something to reload...\n"));
            memset(buffer, 0, LARG_BUF);
            read(READ_FD, buffer, LARG_BUF);
        }
    }
    if(strcmp(stoken, "start_meeting:") == 0)
    {
        stoken = strtok(NULL, " ");
        meeting_port = atoi(stoken);
        stoken = strtok(NULL, " ");
        to_broadcast_fd = atoi(stoken);
        //alarm(0);
        start_conversation(to_broadcast_fd, meeting_port);
    }
    if(strcmp(stoken, "exit_meeting:") == 0) {
        printf("come to exit\n");
        char answer[MAX_MSG_LEN] = {0};
        memset(answer, 0, MAX_MSG_LEN);
        read(READ_FD, buffer, MAX_MSG_LEN);
        memset(buffer, 0, LARG_BUF);
        sprintf(buffer,"exit_meeting: %d %s", meeting_port, answer);
        send(server_fd, buffer, strlen(buffer), 0);
    }
}

void my_handler(int sig)
{
    write(ERR_FD, "no response\n", strlen("no response\n"));
    exit(1);
}

int main(int argc, char* argv[]) {
    if(argc != 2) {
        write(ERR_FD, "bad argument\n", strlen("bad argument\n"));
        exit(1);
    }
    char buffer[MAX_MSG_LEN] = {0};
    int ROLE, SERVER_PORT = atoi(argv[1]);
    int server_fd = connect_to_server(SERVER_PORT);
        if(!server_fd) {
        return 0;
    }
    //alarm(0);
    //signal(SIGALRM, my_handler);

    while(1) {
        write(WRITE_FD, "Choose your role?\n [1] TA\n [2] Student\n", strlen("Choose your role?\n [1] TA\n [2] Student\n"));
        memset(buffer, 0, MAX_MSG_LEN);
        read(READ_FD, buffer, MAX_MSG_LEN);
        ROLE = (int)buffer[0] - 48;
        if(strlen(buffer) > 2 || (buffer[0] != '1' && buffer[0] != '2')) {
            system("clear");
            write(WRITE_FD, "Invalid Input, try again ...\n", strlen("Invalid Input, try again ...\n"));
            continue;
        }
        else {
            system("clear");
            memset(buffer, 0, MAX_MSG_LEN);
            sprintf(buffer,"role: %d", ROLE);
            send(server_fd, buffer, strlen(buffer), 0);
            break;
        }
    }

    fd_set master_set, working_set;
    FD_ZERO(&master_set);
    int max_sd = server_fd;
    FD_SET(server_fd, &master_set);
    FD_SET(STD_IN, &master_set);

    while(1)
    {
        print_menu(ROLE);
        // if(ROLE == TA) {
        //     send(server_fd, "list_questions:", strlen("list_questions:"), 0);
        // }
        working_set = master_set;
        select(max_sd + 1, &working_set, NULL, NULL, NULL);
        for (int i = 0; i <= max_sd; i++)
        {
            if (!FD_ISSET(i, &working_set)) {
                continue;
            }
            else if(i == STD_IN)
            { // user typed something
                STDIN_handler(server_fd, ROLE);
            }
            else if (i == server_fd)
            { // server sent something
                char recieved_buffer[LARG_BUF] = {0};
                memset(recieved_buffer, 0, LARG_BUF);
                int is_valid = recv(i, recieved_buffer, LARG_BUF, 0);
                tokenizing_message(server_fd, ROLE, recieved_buffer);
                if (!is_valid) {
                    continue;
                }
            }
        }
    }
    
    return 0;
}