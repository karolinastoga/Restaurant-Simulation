#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_BUFFER_SIZE 1024
#define MAX_COMMAND_SIZE 6
#define MAX_ORDER_SIZE 50
#define SERVER_PORT 4242

char TABLE_ID;

void prepareClientConnection(char *ip, int *client_socket, struct sockaddr_in *addr);
void createClientSocket(int *client_socket);
void initializeServerAddress(char *ip, struct sockaddr_in *addr);
void connectToServer(int *client_socket, struct sockaddr_in *addr);
bool checkSurnameAndCode(int client_socket);
void displayMenuAction();
void printMenu();
bool startsWith(const char *pre, const char *str);
FILE *openFile(const char *file_name, const char *mode);

int main(int argc, const char *argv[])
{
    fprintf(stdout, "--------------------------------TABLE--------------------------------\n");
    char *ip = "127.0.0.1";
    int port = atoi(argv[1]);

    int client_socket, ret, n;

    struct sockaddr_in addr;
    socklen_t addr_size;

    prepareClientConnection(ip, &client_socket, &addr);

    if (checkSurnameAndCode(client_socket))
    {

        displayMenuAction();

        while (1)
        {
            char command[MAX_COMMAND_SIZE];
            char buffer[MAX_BUFFER_SIZE];
            bzero(command, MAX_COMMAND_SIZE);
            scanf("%s", &command);

            if (startsWith("help", command) || startsWith("menu", command))
            {
                if (startsWith("help", command) == true)
                {
                    // Print the menu options
                    fprintf(stdout, "Commands description:\n");
                    fprintf(stdout, "menu   --> will show detailed menu with dishes and prices\n");
                    fprintf(stdout, "order  --> will send order to the kitchen\n");
                    fprintf(stdout, "bill   --> will send request for reparing the final bill\n");
                }
                else if (startsWith("menu", command) == true)
                {
                    // Print restaurant menu
                    printMenu();
                }
            }
            else if (startsWith("order", command) || startsWith("bill", command) || startsWith("esc", command))
            {
                printf("[SEND COMMAND] %s\n", command);
                if (send(client_socket, command, MAX_COMMAND_SIZE, 0) < 0)
                    printf("[SENDING ERROR]\n");
                else
                {
                    if (startsWith("order", command) == true)
                    {
                        // Take order from client
                        char course[5], order[30];
                        scanf("%[^:]: %[^\n]", course, order);

                        // Send order to server
                        sprintf(buffer, "Course: %s Order: %s", course, order);
                        send(client_socket, buffer, MAX_BUFFER_SIZE, 0);
                        fprintf(stdout, "[TABLE] %s send to server!\n", buffer);

                        bzero(buffer, MAX_BUFFER_SIZE);
                        recv(client_socket, buffer, MAX_BUFFER_SIZE, 0);
                        fprintf(stdout, "[SERVER]%s\n", buffer);
                    }
                    else if (startsWith("bill", command) == true)
                    {
                        fprintf(stdout, "[TABLE] Bill request\n");
                        int result = 0;
                        recv(client_socket, &result, sizeof(int), 0);
                        fprintf(stdout, "[SERVER] Total value: %d\n", result);
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
    printf("[+]TCP server socket created.\n");
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
    printf("[+]Connected to the server.\n");
}

bool checkSurnameAndCode(int client_socket)
{
    char surname[20], buffer[MAX_BUFFER_SIZE], command[MAX_COMMAND_SIZE] = "check";
    char table_id[5], date[20], hour[20];
    int code = 0;

    while (1)
    {
        fprintf(stdout, "Please enter surname and reservation code\n");
        fprintf(stdout, "Enter your surname: ");
        scanf("%s", surname);
        fprintf(stdout, "Enter reservation code: ");
        scanf("%d", &code);
        if (send(client_socket, command, MAX_COMMAND_SIZE, 0) < 0)
            fprintf(stdout, "[SENDING ERROR]\n");
        else
        {
            sprintf(buffer, "%s %d", surname, code);
            fprintf(stdout, "[TABLE SEND] surname: %s, code: %d\n", surname, code);

            // Send parameters to server
            if (send(client_socket, buffer, MAX_BUFFER_SIZE, 0) < 0)
                fprintf(stdout, "[ERROR] Cannot send to server socket\n");
            else
            {
                // Recive checking result
                int result;
                recv(client_socket, &result, sizeof(int), 0);
                bzero(buffer, MAX_BUFFER_SIZE);
                recv(client_socket, buffer, MAX_BUFFER_SIZE, 0);
                if (result > 0)
                {
                    sscanf(buffer, "%s %s %s", table_id, date, hour);
                    fprintf(stdout, "======================================\n", table_id, date, hour, surname);
                    fprintf(stdout, "\nTable: %s %s %s\nWelcome Mr/Mrs %s! \n", table_id, date, hour, surname);
                    fprintf(stdout, "======================================\n", table_id, date, hour, surname);
                    return true;
                }
                else
                {
                    fprintf(stdout, "%s\n", buffer);
                }
            }
        }
    }
}

void displayMenuAction()
{
    fprintf(stdout, "\n--------------------------------------------------\n");
    fprintf(stdout, "Type a command:\n");
    fprintf(stdout, "1)   help       --> show the details of the commands\n");
    fprintf(stdout, "2)   menu       --> show the dishes menu\n");
    fprintf(stdout, "3)   order      --> send an order\n");
    fprintf(stdout, "4)   bill       --> ask for the bill\n");
}

void printMenu()
{
    // Open the file for reading
    FILE *file = openFile("menu.txt", "r");
    if (file == NULL)
    {
        fprintf(stdout, "[-] Error: Could not open file\n");
        return;
    }
    // Read the contents of the file and print a menu
    char line[100];
    int line_number = 1;
    while (fgets(line, 100, file) != NULL)
    {
        fprintf(stdout, "%s", line);
        line_number++;
    }
    fprintf(stdout, "\n");
    // Close the file
    fclose(file);
}

bool startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre) - 1;
    int result = strncmp(str, pre, lenpre);
    if (result == 0)
        return true;
    else
        return false;
}

FILE *openFile(const char *file_name, const char *mode)
{
    FILE *file = fopen(file_name, mode);
    if (file == NULL)
    {
        fprintf(stdout, "[-] Error: Could not open file\n");
    }
    return file;
}