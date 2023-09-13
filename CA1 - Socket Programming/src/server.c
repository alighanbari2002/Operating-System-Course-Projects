#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>

#define STD_IN 0
#define STD_OUT 1
#define STD_ERR 2

#define BROADCAST_IP "192.168.1.255"

#define MAX_BACKLOG 1000000000

#define LARG_BUF 5000
#define MAX_BUF 1204
#define MAX_LESSON_LEN 128

#define NUM_OF_STUDENTS_IN_CLASS 3
#define IP_LEN 16
#define INITIAL_NUMBER_OF_CLASSES 128
#define INITIAL_NUMBER_OF_CLIENTS 384
#define MAX_NUM_OF_CLIENTS_IN_QUEUE 32
#define BUFFER_SIZE 128
#define WRITE_FD 1
#define READ_FD 0
#define ERR_FD 2
#define MAX_MSG_LEN 1024
#define COMPUTER 0
#define NUM_OF_CLASSES 4
#define PORT_ 4000

#define SO_REUSEPORT 15 
// grep -r SO_REUSEPORT /usr/include/asm-generic/socket.h

#define MAX_LISTENERS_COUNT 20

#define NO_QUESTION_FOUND 0
#define NO_TA_SELECTED -1
#define NO_LISTENERS -1

#define FALSE 0
#define TRUE 1

#define NOT_ANSWERED 0
#define ANSWERING 1
#define ANSWERED 2

#define TA 0
#define STUDENT 1

#define IN_PROGRESS 0
#define EXPIRED 1


typedef int Role;
typedef int Bool;
typedef int Status;
typedef int Meeting_status;


typedef struct {
    int id;
    int student_fd;
    int ta_fd;
    char question_text[MAX_MSG_LEN];
    char correct_answer[MAX_MSG_LEN];
    Status status;
} Question;

typedef struct {
    int questions_count;
    Question *questions;
} Questions;



typedef struct {
    int port;
    int student_fd;
    int ta_fd;
    int listeners[MAX_LISTENERS_COUNT];
    Question *question;
    Meeting_status meeting_status;
    // struct sockaddr_in* meeting_broadcast;
    // int socket_fd;
} Meeting;

typedef struct {
    int meetings_count;
    Meeting* meetings;
} Meetings;



typedef struct {
    int fd;
    Role role;
    Bool is_in_meeting;
} Client;

typedef struct {
    int clients_count;
    Client *clients;
} Clients;



int setup_server(int port) {
    struct sockaddr_in address;
    int server_fd;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        write(STD_ERR, "setupServer error: setsockopt\n", 30);
        // 30 is length of error msg
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0){
        write(STD_ERR, "setupServer error: bind\n", 24);
        // 24 is length of error msg
        return -1;
    }
    
    if(listen(server_fd, MAX_BACKLOG) < 0){
        write(STD_ERR, "setupServer error: listen\n", 26);
        // 26 is length of error msg
        return -1;
    }

    return server_fd;
}

void initialize_structs(Clients* clients_info, Questions* questions_info, Meetings* meetings_info) {
    clients_info->clients_count  = 0;
    clients_info->clients = (Client*)malloc(sizeof(Client));

    questions_info->questions_count  = 0;
    questions_info->questions = (Question*)malloc(sizeof(Question));

    meetings_info->meetings_count  = 0;
    meetings_info->meetings = (Meeting*)malloc(sizeof(Meeting));
}

int acceptClient(int server_fd) {
    int client_fd;
    struct sockaddr_in client_address;
    int address_len = sizeof(client_address);
    client_fd = accept(server_fd, (struct sockaddr *)&client_address, (socklen_t *)&address_len);

    if (client_fd < 0)
    {
        write(STD_ERR, "acceptClient error: accept\n", 27);
        // 27 is length of error msg
        return -1;
    }

    return client_fd;
}

void print_clients(Clients clients) {
    printf("\nall clients:\n");
    for (int i = 0; i < clients.clients_count; i++) {
        printf("fd=%d Role=%d\n", clients.clients[i].fd, clients.clients[i].role);
    }
    printf("\n");
}

void print_questions(Questions questions) {
    printf("\nall questions:\n");
    for (int i = 0; i < questions.questions_count; i++) {
        printf("id=%d student_fd=%d ta_fd=%d question=%s status=%d\n", questions.questions[i].id, questions.questions[i].student_fd, 
        questions.questions[i].ta_fd, questions.questions[i].question_text, questions.questions[i].status);
    }
    printf("\n");
}

void print_meetings(Meetings meetings) {
    printf("\nall meetings:\n");
    for (int i = 0; i < meetings.meetings_count; i++) {
        printf("port=%d student_fd=%d ta_fd=%d status=%d\n", meetings.meetings[i].port, meetings.meetings[i].student_fd,
         meetings.meetings[i].ta_fd, meetings.meetings[i].meeting_status);
    }
    printf("\n");
}




int add_client(int server_fd, Clients *clients_info) {
    int new_client_fd = acceptClient(server_fd);

    Client *new_client = (Client *)malloc(sizeof(Client));
    new_client->fd = new_client_fd;
    new_client->is_in_meeting = FALSE;

    Client *temp_clients = (Client *)realloc(clients_info->clients, ((clients_info->clients_count + 1) * sizeof(Client)));
    if (temp_clients == NULL) {
        return new_client_fd;
    }
    else {
        write(WRITE_FD, "New client connected.\n", strlen("New client connected.\n"));
        clients_info->clients = temp_clients;
        clients_info->clients[clients_info->clients_count] = *new_client;
    }
    clients_info->clients_count++;
    return new_client_fd;
}

int question_id_generator() {
    static int id = -1;
    id++;
    return id;
}

void add_question(Questions *questions_info, char *q_text, int stu_fd) {
    Question *new_question = (Question *)malloc(sizeof(Question));

    new_question->id = question_id_generator();
    strcpy(new_question->question_text, q_text);
    new_question->student_fd = stu_fd;
    new_question->ta_fd = NO_TA_SELECTED;
    new_question->status = NOT_ANSWERED;

    Question *temp_questions = (Question *)realloc(questions_info->questions, ((questions_info->questions_count + 1) * sizeof(Question)));
    if (temp_questions == NULL) {
        return;
    }
    else {
        questions_info->questions = temp_questions;
        questions_info->questions[questions_info->questions_count] = *new_question;
    }
    questions_info->questions_count++;
}

int update_question_status(Questions *questions_info, int question_ID, int ta_fd) {
    for(int i = 0; i < questions_info->questions_count; i++) {
        if(questions_info->questions[i].id == question_ID) {
            questions_info->questions[i].status = ANSWERING;
            questions_info->questions[i].ta_fd = ta_fd;
            return questions_info->questions[i].student_fd;
        }
    }
    write(ERR_FD, "invalid id!\n", strlen("invalid id!\n"));
    exit(1);
}

// void setup_meeting_broadcast(struct sockaddr_in* sockadr, int port, int sock)
// {
//     int broadcast = 1;
//     sockadr->sin_family = AF_INET;
//     sockadr->sin_port = htons(port);
//     sockadr->sin_addr.s_addr = inet_addr(BROADCAST_IP);
//     if(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast))) {
//         write(ERR_FD, "set setsocketopt problem!\n", strlen("set meeting socket problem!\n")); 
//     }
//     if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &broadcast, sizeof(broadcast))) {
//         write(ERR_FD, "set setsocket problem!\n", strlen("set setsocket problem!\n")); 
//     }
//     if(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &broadcast, sizeof(broadcast))) {
//         write(ERR_FD, "error setsocket reuse!\n", strlen("error setsocket reuse!\n")); 
//     }
//     if(bind(sock, (const struct sockaddr*)sockadr, sizeof(*sockadr))) {
//         write(ERR_FD, "error binding!\n", strlen("error binding!\n")); 
//     }
// }

Question* find_question(Questions* questions_info, int id) {
    for(int i = 0; i < questions_info->questions_count; i++) {
        if(questions_info->questions[i].id == id) {
            return &questions_info->questions[i];
        }
    }
    return NULL;
}

int add_meeting(Meetings *meetings_info, Questions* questions_info, int SERVER_PORT, int student_fd, int ta_fd, int q_id) {
    Meeting *new_meeting = (Meeting *)malloc(sizeof(Meeting));

    new_meeting->port = SERVER_PORT + meetings_info->meetings_count + 1;
    new_meeting->meeting_status = IN_PROGRESS;
    new_meeting->student_fd = student_fd;
    new_meeting->ta_fd = ta_fd;
    new_meeting->listeners[0] = NO_LISTENERS;
    new_meeting->question = find_question(questions_info, q_id);
    // new_meeting->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    // if(new_meeting->socket_fd <= 0) {
    //     write(ERR_FD, "room socket problem!\n", strlen("room socket problem!\n"));
    // }
    // new_meeting->meeting_broadcast = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
    // setup_meeting_broadcast(new_meeting->meeting_broadcast, new_meeting->port, new_meeting->socket_fd);

    Meeting *temp_meetings = (Meeting *)realloc(meetings_info->meetings, ((meetings_info->meetings_count + 1) * sizeof(Meeting)));

    if (temp_meetings == NULL) {
        return -1;
    }
    else {
        meetings_info->meetings = temp_meetings;
        meetings_info->meetings[meetings_info->meetings_count] = *new_meeting;
    }
    meetings_info->meetings_count++;
    return new_meeting->port;
}

void send_unanswerd_questions(Questions questions_info, int fd) {
    char buffer[LARG_BUF] = {0};
    int i = 0, counter = 0;
    memset(buffer, 0, LARG_BUF);
    strcpy(buffer, "list_questions: ");
    for(i = 0; i < questions_info.questions_count; i++) {
        if(questions_info.questions[i].status == NOT_ANSWERED) {
            char temp[MAX_BUF] = {0};
            memset(temp, 0, MAX_BUF);
            sprintf(temp, "[ID = %d] %s", questions_info.questions[i].id, questions_info.questions[i].question_text);
            strcat(buffer, temp);
            counter++;
        }
    }
    if (counter == NO_QUESTION_FOUND) {
        strcat(buffer, "no questions to display!\n");
    }

    send(fd, buffer, strlen(buffer), 0);
}

int dashboard(Clients *clients_info, Questions *questions_info, Meetings* meetings_info, int fd, int SERVER_PORT) {
    char buffer[MAX_MSG_LEN] = {0};
    memset(buffer, 0, MAX_MSG_LEN);
    int bytes_received = recv(fd, buffer, MAX_MSG_LEN, 0);

    printf("client request=%s\n", buffer);

    if (bytes_received != 0)
    {
        char tokenizing_buffer[MAX_MSG_LEN] = {0};
        memset(tokenizing_buffer, 0, MAX_MSG_LEN);
        strcpy(tokenizing_buffer, buffer);
        char* stoken = strtok(tokenizing_buffer, " ");

        if(strcmp(stoken, "role:") == 0)
        {
            stoken = strtok(NULL, " ");
            clients_info->clients[clients_info->clients_count - 1].role = atoi(stoken);
        }
        else if(strcmp(stoken, "new_question:") == 0)
        {
            int token_size = strlen(stoken) + 1, index = 0;
            for(index = 0; index < strlen(buffer) - token_size; index++) {
                buffer[index] = buffer[index + token_size];
            }
            buffer[index] = '\0';

            add_question(questions_info, buffer, fd);
        }
        else if(strcmp(stoken, "list_questions:") == 0)
        {
            send_unanswerd_questions(*questions_info, fd);
        }
        else if(strcmp(stoken, "TA_ID_selection:") == 0)
        {
            stoken = strtok(NULL, " ");
            int question_ID = atoi(stoken);

            int student_fd = update_question_status(questions_info, question_ID, fd);
            int port = add_meeting(meetings_info, questions_info, SERVER_PORT, student_fd, fd, question_ID);
            
            memset(buffer, 0, MAX_MSG_LEN);
            sprintf(buffer, "start_meeting: %d %d", port, fd);
            send(student_fd, buffer, strlen(buffer), 0);
            memset(buffer, 0, MAX_MSG_LEN);
            sprintf(buffer, "start_meeting: %d %d", port, student_fd);
            send(fd, buffer, strlen(buffer), 0);
        }
        else if(strcmp(stoken, "exit_meeting:") == 0)
        {
            int toral_token_size = strlen(stoken) + 1, index = 0;
            stoken = strtok(NULL, " ");
            int port = atoi(stoken);
            toral_token_size += (strlen(stoken) + 1);
            for(index = 0; index < strlen(buffer) - toral_token_size; index++) {
                buffer[index] = buffer[index + toral_token_size];
            }
            buffer[index] = '\0';
        
            printf("port=%d answer=%s\n", port, buffer);
        }
    }

    return bytes_received;
}

int main(int argc, char *argv[]) {
    if(argc != 2) {
        write(ERR_FD, "bad argument\n", strlen("bad argument\n"));
        exit(1);
    }
    char buffer[MAX_MSG_LEN] = {0};
    int server_fd, max_sd, SERVER_PORT;
    SERVER_PORT = atoi(argv[1]);
    server_fd = setup_server(SERVER_PORT);
    memset(buffer, 0, MAX_MSG_LEN);
    sprintf(buffer, "Server on port %d activated\n", SERVER_PORT);
    write(WRITE_FD, buffer, strlen(buffer));

    Clients clients_info;
    Questions questions_info;
    Meetings meetings_info;
    initialize_structs(&clients_info, &questions_info, &meetings_info);

    fd_set master_set, working_set;
    FD_ZERO(&master_set);
    max_sd = server_fd;
    FD_SET(server_fd, &master_set);

    while (1)
    {
        working_set = master_set;
        select(max_sd + 1, &working_set, NULL, NULL, NULL);

        write(WRITE_FD, "request received\n", strlen("request received\n"));

        for (int i = 0; i <= max_sd; i++)
        {
            if (!FD_ISSET(i, &working_set))
            {
                continue;
            }
            else if (i == server_fd)
            { // new clinet
                int new_socket = add_client(server_fd, &clients_info);

                FD_SET(new_socket, &master_set);
                if (new_socket > max_sd)
                    max_sd = new_socket;
            }
            else
            { // client sending msg
                int is_valid = dashboard(&clients_info, &questions_info, &meetings_info, i, SERVER_PORT);
                if (!is_valid)
                { // EOF
                    memset(buffer, 0, MAX_MSG_LEN);
                    sprintf(buffer, "client fd = %d closed\n", i);
                    write(WRITE_FD, buffer, strlen(buffer));
                    close(i);
                    FD_CLR(i, &master_set);
                    continue;
                }
                memset(buffer, 0, 1024);
            }
        }
    }

    return 0;
}