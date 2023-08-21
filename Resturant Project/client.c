#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_BUFFER_SIZE 1024
#define MAX_COMMAND_SIZE 6
#define SERVER_PORT 4242

void prepareClientConnection(char *ip, int *client_socket, struct sockaddr_in *addr);
void createClientSocket(int *client_socket);
void initializeServerAddress(char *ip, struct sockaddr_in *addr);
void connectToServer(int *client_socket, struct sockaddr_in *addr);
void displayMenuAction();
bool startsWith(const char *pre, const char *str);

// Struct for information provided by client for find request
typedef struct ReservationParameters
{
    char surname[30];
    int people;
    char date[20];
    char hour[20];
} ReservationParameters;

int main(int argc, const char *argv[])
{
    fprintf(stdout, "--------------------------------CLIENT-------------------------------\n");
    char *ip = "127.0.0.1";
    int port = atoi(argv[1]);

    int client_socket, ret, n;

    struct sockaddr_in addr;
    socklen_t addr_size;

    prepareClientConnection(ip, &client_socket, &addr);
    displayMenuAction();

    while (1)
    {
        char command[MAX_COMMAND_SIZE];
        char buffer[MAX_BUFFER_SIZE];

        // Get command from user
        bzero(command, MAX_COMMAND_SIZE);
        scanf("%s", &command);
        if (startsWith("find", command) || startsWith("book", command) || startsWith("esc", command))
        {
            fprintf(stdout, "[SEND COMMAND] %s\n", command);
            if (send(client_socket, command, MAX_COMMAND_SIZE, 0) < 0)
                fprintf(stdout, "[SENDING ERROR]\n");
            else
            {
                if (startsWith("find", command) == true)
                {
                    // Get reseravtion parameteres from user
                    ReservationParameters res_param;
                    scanf("%s %d %s %s", &res_param.surname, &res_param.people, &res_param.date, &res_param.hour);

                    // Write parameters to buffer
                    bzero(buffer, MAX_BUFFER_SIZE);
                    sprintf(buffer, "%s %d %s %s", res_param.surname, res_param.people, res_param.date, res_param.hour);
                    fprintf(stdout, "[SEND BUFFER] %s\n", buffer);

                    // Send parameters to server
                    if (send(client_socket, buffer, MAX_BUFFER_SIZE, 0) < 0)
                        fprintf(stdout, "[ERROR] Cannot send to server socket\n");
                    else
                    {
                        // Recive searching for available tables result
                        int result;
                        recv(client_socket, &result, sizeof(int), 0);
                        if (result <= 0)
                        {
                            bzero(buffer, MAX_BUFFER_SIZE);
                            recv(client_socket, buffer, MAX_BUFFER_SIZE, 0);
                            fprintf(stdout, "%s\n", buffer);
                        }
                        else
                        {
                            fprintf(stdout, "We have available %d tables:\n", result);
                            for (int i = 0; i < result; i++)
                            {
                                bzero(buffer, MAX_BUFFER_SIZE);
                                recv(client_socket, buffer, MAX_BUFFER_SIZE, 0);
                                fprintf(stdout, "%d) %s\n", i + 1, buffer);
                            }
                            fprintf(stdout, "Please choose one option and enter: book {nr_of_choosen_table}.\n");
                        }
                    }
                }
                else if (startsWith("book", command) == true)
                {
                    // Get table choice from client
                    int choice = 0;
                    scanf("%d", &choice);

                    // Send client choice to the server
                    if (send(client_socket, &choice, sizeof(int), 0) < 0)
                        fprintf(stdout, "[ERROR] Cannot send to server socket\n");
                    else
                    {
                        fprintf(stdout, "[SEND BUFFER] %d\n", choice);

                        int result;
                        recv(client_socket, &result, sizeof(int), 0);
                        bzero(buffer, MAX_BUFFER_SIZE);
                        recv(client_socket, buffer, MAX_BUFFER_SIZE, 0);
                        if (result < 0)
                        {
                            fprintf(stdout, "%s\n", buffer);
                        }
                        else
                        {
                            fprintf(stdout, "BOOKIN MADE: %s\n", buffer);
                        }
                    }
                }
                else if (startsWith("esc", command) == true)
                {
                    // send esc command to server and disconect from server
                    bzero(buffer, MAX_BUFFER_SIZE);
                    strcpy(buffer, command);
                    send(client_socket, buffer, MAX_BUFFER_SIZE, 0);
                    close(client_socket);
                    fprintf(stdout, "[+]Disconnected from the server.\n");
                    return 0;
                }
            }
        }
        else
            fprintf(stdout, "Wrong command, please try again.\n");
    }
}

void prepareClientConnection(char *ip, int *client_socket, struct sockaddr_in *addr)
{
    createClientSocket(client_socket);    // Create a TCP socket
    initializeServerAddress(ip, addr);    // Initialize the server address
    connectToServer(client_socket, addr); // Connect to the server
}

void createClientSocket(int *client_socket)
{
    *client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0)
    {
        perror("[-]Socket error.\n");
        exit(1);
    }
    fprintf(stdout, "[+]TCP server socket created.\n");
}

void initializeServerAddress(char *ip, struct sockaddr_in *addr)
{
    memset(addr, '\0', sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = SERVER_PORT;
    addr->sin_addr.s_addr = inet_addr(ip);
}

void connectToServer(int *client_socket, struct sockaddr_in *addr)
{
    int ret = connect(*client_socket, (struct sockaddr *)addr, sizeof(*addr));
    if (ret < 0)
    {
        perror("[-]Connection error.\n");
        exit(1);
    }
    fprintf(stdout, "[+]Connected to the server.\n");
}

void displayMenuAction()
{
    fprintf(stdout, "\n----------------------WELCOME!----------------------\n");
    fprintf(stdout, "Type a command:\n");
    fprintf(stdout, "1)   find  ---> search availabilty for a reservation\n");
    fprintf(stdout, "2)   book  ---> seand a reservation\n");
    fprintf(stdout, "3)   esc   ---> terminate the client\n");
}

bool startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre) - 1;
    int wynik = strncmp(str, pre, lenpre);
    if (wynik == 0)
        return true;
    else
        return false;
}