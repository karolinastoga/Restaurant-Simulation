#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_BUFFER_SIZE 1024
#define MAX_COMMAND_SIZE 6
#define SERVER_PORT 4242

// Struct for orders handling
typedef struct Order
{
    int rsrv_code;
    char table_id[5];
    char course[5];
    char order[30];
} Order;

void prepareClientConnection(char *ip, int *client_socket, struct sockaddr_in *addr);
void createClientSocket(int *client_socket);
void initializeServerAddress(char *ip, struct sockaddr_in *addr);
void connectToServer(int *client_socket, struct sockaddr_in *addr);
void displayMenuAction();
bool startsWith(const char *pre, const char *str);
void cleanOrder(Order *order);

int main(int argc, const char *argv[])
{
    fprintf(stdout, "--------------------------------KITCHEN DEVICE--------------------------------\n");
    Order order = {0, "", "", ""};
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
        bzero(command, MAX_COMMAND_SIZE);
        scanf("%s", &command);
        if (strcmp("take", command) == 0 || strcmp("ready", command) == 0 || strcmp("show", command) == 0 || strcmp("esc", command) == 0)
        {
            if (strcmp("take", command) == 0)
            {
                if (order.rsrv_code == 0)
                {
                    if (send(client_socket, command, MAX_COMMAND_SIZE, 0) < 0)
                        printf("[SENDING ERROR]\n");
                    else
                    {
                        // Recive information about taken order
                        int result;
                        recv(client_socket, &result, sizeof(int), 0);
                        bzero(buffer, MAX_BUFFER_SIZE);
                        recv(client_socket, buffer, MAX_BUFFER_SIZE, 0);
                        if (result > 0)
                        {
                            sscanf(buffer, "%d %s %s %[^/n]", &order.rsrv_code, order.table_id, order.course, order.order);
                            fprintf(stdout, "[SERVER] Rsrv code %d Order for table %s course: %s order: %s\n", order.rsrv_code, order.table_id, order.course, order.order);
                        }
                        else
                        {
                            fprintf(stdout, "[SERVER]%s\n", buffer);
                        }
                    }
                }
                else
                {
                    fprintf(stdout, "Order already taken by kitchen device. Please finish order first!\n");
                }
            }
            else if (strcmp("ready", command) == 0)
            {
                if (order.rsrv_code != 0)
                {
                    if (send(client_socket, command, MAX_COMMAND_SIZE, 0) < 0)
                        printf("[SENDING ERROR]\n");
                    else
                    {
                        bzero(buffer, MAX_BUFFER_SIZE);
                        sprintf(buffer, "%d %s", order.rsrv_code, order.course);
                        send(client_socket, buffer, MAX_BUFFER_SIZE, 0);
                        fprintf(stdout, "[KD] %s\n", buffer);

                        cleanOrder(&order);

                        bzero(buffer, MAX_BUFFER_SIZE);
                        recv(client_socket, buffer, MAX_BUFFER_SIZE, 0);
                        fprintf(stdout, "%s\n", buffer);
                    }
                }
                else
                {
                    fprintf(stdout, "No order taken by kitchen device. Please take order first!\n");
                }
            }
            else if (strcmp("show", command) == 0)
            {
                if (send(client_socket, command, MAX_COMMAND_SIZE, 0) < 0)
                    printf("[SENDING ERROR]\n");
                else
                {
                    int result = 0;
                    recv(client_socket, &result, sizeof(int), 0);
                    if (result <= 0)
                    {
                        recv(client_socket, buffer, MAX_BUFFER_SIZE, 0);
                        fprintf(stdout, "%s\n", buffer);
                    }
                    else
                    {
                        int orders_nr = 0;
                        fprintf(stdout, "Orders in reparation:\n");
                        recv(client_socket, &orders_nr, sizeof(int), 0);
                        for (int i = 0; i < orders_nr; i++)
                        {
                            Order order;
                            bzero(buffer, MAX_BUFFER_SIZE);
                            recv(client_socket, buffer, MAX_BUFFER_SIZE, 0);
                            sscanf(buffer, "%s %s %[^/n]", &order.table_id, &order.course, &order.order);
                            fprintf(stdout, "%d)Table %s course %s order details: %s\n", i + 1, order.table_id, order.course, order.order);
                        }
                    }
                }
            }
            else if (strcmp("esc", command) == 0)
            {
                if (send(client_socket, command, MAX_COMMAND_SIZE, 0) < 0)
                    printf("[SENDING ERROR]\n");
                else
                {
                    // send esc command to server and disconect from server
                    char buffer[MAX_BUFFER_SIZE] = {0};
                    strcpy(buffer, command);
                    send(client_socket, buffer, strlen(buffer), 0);
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

void displayMenuAction()
{
    fprintf(stdout, "\n------------------WELCOME!-----------------\n");
    fprintf(stdout, "Type a command:\n");
    fprintf(stdout, "1)   take    ---> accept command\n");
    fprintf(stdout, "2)   ready   ---> set the status of the command\n");
    fprintf(stdout, "3)   show    ---> show the accepted commands\n");
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

void cleanOrder(Order *order)
{
    strncpy(order->course, "", sizeof(order->course));
    strncpy(order->order, "", sizeof(order->order));
    order->rsrv_code = 0;
    strncpy(order->table_id, "", sizeof(order->table_id));
}