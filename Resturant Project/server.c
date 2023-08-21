#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <math.h>

#define RESERVATIONS_FILE "reservations.bin" // File used to store reservation data
#define ORDERS_FILE "orders.bin"             // File used to store order data
#define MENU_FILE "menu.txt"                 // File used to store menu data
#define STATUS_WAITING "waiting"
#define STATUS_PREPARING "preparing"
#define STATUS_SERVED "served"
#define MAX_COMMAND_SIZE 6         // Maximum size of a command
#define MAX_SERVER_COMMAND_SIZE 20 // Maximum size of a command for server
#define MAX_BUFFER_SIZE 1024       // Maximum size of a buffer
#define MAX_ORDER_SIZE 30          // Maximum size of an order
#define MAX_RESERVATIONS 30        // Maximum number of reservations allowed
#define MAX_ORDERS_PER_TABLE 5     // Maximum number of orders allowed
#define MAX_TABLES 6               // Maximum number of tables in the restaurant
#define MAX_KITCHEN_DEVICES 10     // Maximum number of kitchen devices
#define MAX_MENU_ITEMS 8           // Maximum number of menu items
#define MAX_CODE_LENGTH 3          // Maximum length of a dish code
#define MAX_NAME_LENGTH 30         // Maximum length of a dish name

// Struct for making a reservation request
typedef struct FindRequest
{
    char surname[20]; // Surname of the person making the reservation
    int people;       // Number of people to be seated
    char date[20];    // Date of the reservation
    char hour[20];    // Time of the reservation
} FindRequest;

// Struct for detailed table information
typedef struct Table
{
    char id[5];          // Unique identifier for each table
    char room[6];        // Room in which the table is placed
    int nr_seats;        // Maximum number of people that can be seated at this table
    char place_desc[30]; // Short description of where the table is placed
} Table;

// Struct for creating a list of matching tables
typedef struct MatchingTable
{
    Table *table; // Pointer to a table that matches a search criterion
} MatchingTable;

// Struct for reservation information
typedef struct Reservation
{
    int code;         // Unique reservation code
    char surname[30]; // Surname of the person who made the reservation
    int nr_people;    // Number of people to be seated
    char date[20];    // Date of the reservation
    char hour[20];    // Time of the reservation
    Table *table;     // Pointer to the reserved table
} Reservation;

// Struct for order handling
typedef struct Order
{
    int rsrv_code;    // Reservation code associated with the order
    char table_id[5]; // Identifier of the table the order is placed from
    char course[5];   // Course code for the ordered item
    char order[30];   // Description of the ordered item
    char status[20];  // Current status of the order
    int value;        // Price of the ordered item
    time_t time;      // Time when the order was placed
} Order;

typedef struct
{
    char code[MAX_CODE_LENGTH + 1]; // Code representing a menu item
    char name[MAX_NAME_LENGTH + 1]; // Name of the menu item
    int price;                      // Price of the menu item
} MenuItem;

struct ThreadArgs
{
    int port;
    int server_sock;
};

// List of all available tables in restaurant
Table ALL_TABLES[MAX_TABLES] = {{"T12", "ROOM1", 2, "WINDOW"},
                                {"T22", "ROOM2", 2, "ENTRANCE"},
                                {"T14", "ROOM1", 4, "FIREPLACE"},
                                {"T24", "ROOM2", 4, "ENTRANCE"},
                                {"T16", "ROOM1", 6, "WINDOW"},
                                {"T26", "ROOM2", 6, "FIREPLACE"}};

// Methods handling threads
void *scan_function(void *arg);
void *socket_communication(void *arg);

// Methods handling Socket Connections
void prepareServerForConnections(struct sockaddr_in *server_addr, int *server_sock, const char *ip, int *port, int *n);
void createSocket(int *server_sock);
void initializeServerAddress(struct sockaddr_in *server_addr, const char *ip, int *port);
void bindServer(struct sockaddr_in *server_addr, int *server_sock, int *port, int *n);
void listenForIncomingConnections(int *server_sock);
void establishNewConnection(struct sockaddr_in *client_addr, socklen_t *addr_size, int *server_sock, int *client_sock);

// Methods handling Reservations
int findAvailableTables(MatchingTable matching_tab[], FindRequest *rsrv_params);
int isTableReserved(const char *table_id, const char *date, const char *time);
int addReservation(FindRequest *rsrv_params, Table *table, Reservation *reservation);
int generateReservationCode();
int findReservation(const char *surname, int code, Reservation *reservation);

// Methods handling Orders
int saveOrder(Order *order);
void printOrderStatusByTable(const char *table_id);
void printOrderStatusByStatus(const char *status);
void sendLongestWaitingOrder(int client_sock);
int changeOrderStatus(int rsrv_code, const char *course, const char *new_status);
void sendAllOrdersInPreparingStatus(int client_sock);
int allOrdersAreServed();
int countReceipt(const char *order);

// Supporting methods
bool startsWith(const char *pre, const char *str);
int roundToEven(int num);

int main(int argc, const char *argv[])
{
    fprintf(stdout, "-------------------------------------------SERVER-------------------------------------------\n");
    pthread_t scan_thread, socket_communication_thread;
    int port = atoi(argv[1]);
    int server_sock;

    createSocket(&server_sock);

    struct ThreadArgs args;
    args.port = port;
    args.server_sock = server_sock;

    pthread_create(&scan_thread, NULL, scan_function, &server_sock);
    pthread_create(&socket_communication_thread, NULL, socket_communication, &args);

    // Wait for the scan_thread and socket_communication_thread to complete
    pthread_join(scan_thread, NULL);
    pthread_join(socket_communication_thread, NULL);

    return 0;
}

void *scan_function(void *arg)
{
    int server_sock = *(int *)arg;
    fprintf(stdout, "\n------------------------------------------WELCOME!------------------------------------------\n");
    fprintf(stdout, "1)  stat {table_nr} or {status} ---> display table status or dishes that are in given status\n");
    fprintf(stdout, "2)  stop                        ---> stop the server if there are bo other meals to prepare\n\n");

    while (1)
    {
        char command[MAX_SERVER_COMMAND_SIZE];
        fgets(command, sizeof(command), stdin);

        if (startsWith("stop", command))
        {
            fprintf(stdout, "[SERVER STOP] Checking if all orders are served...\n");
            if (allOrdersAreServed() == 1)
            {
                fprintf(stdout, "[SERVER STOP] All orders are served. Closing the server...\n");
                close(server_sock);
                break;
            }
        }
        else if (startsWith("stat table", command))
        {
            char table_id[5];
            sscanf(command, "stat table %s", table_id);
            fprintf(stdout, "[SERVER STAT] Printing orders for table %s...\n", table_id);
            printOrderStatusByTable(table_id);
        }
        else if (startsWith("stat status", command))
        {
            char status[10];
            sscanf(command, "stat status %s", status);
            fprintf(stdout, "[SERVER STAT] Printing orders with status: %s...\n", status);
            printOrderStatusByStatus(status);
        }
    }
}

void *socket_communication(void *arg)
{
    struct ThreadArgs *args = (struct ThreadArgs *)arg;
    int port = args->port;
    int server_sock = args->server_sock;
    // Declare all important variables for establishing a connection
    char *ip = "127.0.0.1";                      // IP address of the server
    int n, client_sock, td_sock, kd_sock;        // Socket descriptors for various connections
    struct sockaddr_in server_addr, client_addr; // Server and client address structures
    socklen_t addr_size;                         // Size of the address structure
    pid_t childpid;                              // Process ID for child processes

    // Prepare server for incoming connections
    prepareServerForConnections(&server_addr, &server_sock, ip, &port, &n);

    while (1)
    {
        // Handle new connection
        // Establish new incoming connection
        establishNewConnection(&client_addr, &addr_size, &server_sock, &client_sock);

        // Create a new child process
        if ((childpid = fork()) == 0)
        {
            int total = 0; // total order value for table
            close(server_sock);

            // Handle for sever-child communication
            while (1)
            {
                // Declare variables for communication
                char command[MAX_COMMAND_SIZE]; // Array for receiving commands
                char buffer[MAX_BUFFER_SIZE];   // Array for receiving/sending messages
                // Recive command
                bzero(command, MAX_COMMAND_SIZE);
                if (recv(client_sock, command, MAX_COMMAND_SIZE, 0) < 0)
                    fprintf(stdout, "[ERROR] Cannot recive command\n");
                else
                {
                    // Handle reviced command
                    fprintf(stdout, "[COMMAND] %s\n", command);

                    // Handle client commands
                    if (startsWith("find", command) || startsWith("book", command))
                    {
                        FindRequest reserv_params;
                        MatchingTable matching_tab[MAX_TABLES];

                        // Find available tables options for recived parameters
                        if (startsWith("find", command) == true)
                        {
                            // Recive detailed reservation request from client
                            bzero(buffer, MAX_BUFFER_SIZE);
                            recv(client_sock, buffer, MAX_BUFFER_SIZE, 0);
                            fprintf(stdout, "[CLIENT] %s\n", buffer);

                            // Write infromation from buffer to the FindRequest struct
                            sscanf(buffer, "%s %d %s %s", &reserv_params.surname, &reserv_params.people, &reserv_params.date, &reserv_params.hour);

                            // Find avaible tables
                            int result = findAvailableTables(matching_tab, &reserv_params);

                            send(client_sock, &result, sizeof(int), 0);
                            // Error occurred while finding available tables
                            if (result <= 0)
                            {
                                bzero(buffer, MAX_BUFFER_SIZE);
                                if (result < 0)
                                {
                                    char error_msg[] = "[ERROR] Could not open file";
                                    strcpy(buffer, error_msg);
                                }
                                else
                                {
                                    char no_found_msg[] = "Sorry! All tables are reserved. Please try different data/hour.";
                                    strcpy(buffer, no_found_msg);
                                }
                                send(client_sock, buffer, MAX_BUFFER_SIZE, 0);
                                fprintf(stdout, "[SERVER]%s\n", buffer);
                            }
                            // Send found available tables
                            else
                            {
                                for (int k = 0; k < result; k++)
                                {
                                    bzero(buffer, MAX_BUFFER_SIZE);
                                    sprintf(buffer, "%s %s %s", matching_tab[k].table->id, matching_tab[k].table->room, matching_tab[k].table->place_desc);
                                    send(client_sock, buffer, MAX_BUFFER_SIZE, 0);
                                }
                                fprintf(stdout, "[SERVER] Available tables send to client\n");
                            }
                        }
                        else if (startsWith("book", command) == true)
                        {
                            // Recive client reservation choice
                            int choice;
                            recv(client_sock, &choice, sizeof(int), 0);
                            fprintf(stdout, "[CLIENT] %d\n", choice);

                            // Book table for given client choice
                            Reservation reservation;
                            int result = addReservation(&reserv_params, matching_tab[choice - 1].table, &reservation);

                            send(client_sock, &result, sizeof(int), 0);
                            bzero(buffer, MAX_BUFFER_SIZE);
                            // Error occurred while finding available tables
                            if (result < 0)
                            {
                                char error_msg[] = "[ERROR] Could not open file";
                                strcpy(buffer, error_msg);
                                fprintf(stdout, "[SERVER]%s\n", error_msg);
                            }
                            // Send reservation confirmation to client
                            else
                            {
                                sprintf(buffer, "%d %s %s", reservation.code, reservation.table->room, reservation.table->id);
                                fprintf(stdout, "[SERVER]Reservation details: %s\n", buffer);
                            }

                            if (send(client_sock, buffer, MAX_BUFFER_SIZE, 0) < 0)
                                fprintf(stdout, "[-]Error with sending\n");
                        }
                    }
                    else if (startsWith("check", command) || startsWith("order", command) || startsWith("bill", command))
                    {
                        Reservation reservation;
                        if (startsWith("check", command) == true)
                        {
                            char surname[20];
                            int code;

                            // Recive surname and code from client to login to table
                            bzero(buffer, MAX_BUFFER_SIZE);
                            recv(client_sock, buffer, MAX_BUFFER_SIZE, 0);
                            sscanf(buffer, "%s %d", surname, &code);
                            fprintf(stdout, "[TABLE] Surname: %s code:%d\n", surname, code);

                            // Check if there is reservation for given surname and code
                            Reservation reservation;
                            int result;
                            result = findReservation(surname, code, &reservation);

                            send(client_sock, &result, sizeof(int), 0);
                            bzero(buffer, MAX_BUFFER_SIZE);
                            // Error occurred while finding available tables
                            if (result < 0)
                            {
                                char error_msg[] = "[[ERROR] Could not open file";
                                strcpy(buffer, error_msg);
                            }
                            else if (reservation.code == 0)
                            {
                                char wrong_creds[] = "[ERROR] No reservation found.";
                                strcpy(buffer, wrong_creds);
                            }
                            else
                            {
                                sprintf(buffer, "%s %s %s", reservation.table->id, reservation.date, reservation.hour);
                            }
                            send(client_sock, buffer, MAX_BUFFER_SIZE, 0);
                            fprintf(stdout, "[SERVER] %s\n", buffer);
                        }
                        else if (startsWith("order", command) == true)
                        {
                            Order order;
                            // Recive order from Table
                            recv(client_sock, &buffer, MAX_BUFFER_SIZE, 0);
                            sscanf(buffer, "Course: %s Order: %[^\n]", order.course, order.order);
                            fprintf(stdout, "[TABLE]Course: %s Order: %s\n", order.course, order.order);

                            // Fill missing Order information
                            order.rsrv_code = reservation.code;
                            strcpy(order.table_id, reservation.table->id);
                            strcpy(order.status, STATUS_WAITING);
                            order.time = time(NULL);

                            // Count value of the order
                            order.value = countReceipt(order.order);
                            total += order.value;

                            // Save order
                            int result;
                            result = saveOrder(&order);
                            bzero(buffer, MAX_BUFFER_SIZE);
                            if (result < 0)
                            {
                                char error_msg[] = "[ERROR] Could not open file";
                                strcpy(buffer, error_msg);
                            }
                            else
                            {
                                char success_msg[] = "Order was successfully saved!";
                                strcpy(buffer, success_msg);
                            }
                            send(client_sock, buffer, MAX_BUFFER_SIZE, 0);
                            fprintf(stdout, "[SERVER] %s\n", buffer);
                        }
                        else if (startsWith("bill", command) == true)
                        {
                            // Get total value and send it to Table
                            send(client_sock, &total, sizeof(int), 0);
                            fprintf(stdout, "[SERVER SEND] Total bill value: %d\n", total);
                        }
                    }
                    else if (startsWith("take", command) || startsWith("ready", command) || startsWith("show", command))
                    {
                        if (startsWith("take", command) == true)
                        {
                            // Take longest waiting order and change it status
                            sendLongestWaitingOrder(client_sock);
                        }
                        else if (startsWith("ready", command) == true)
                        {
                            int rsrv_code;
                            char course[5];

                            // Get rsrv_code and course
                            bzero(buffer, MAX_BUFFER_SIZE);
                            recv(client_sock, buffer, MAX_BUFFER_SIZE, 0);
                            sscanf(buffer, "%d %s", &rsrv_code, course);
                            fprintf(stdout, "[KD] Rsrv Code: %d Course: %s\n", rsrv_code, course);

                            // Change order status
                            int result = changeOrderStatus(rsrv_code, course, STATUS_SERVED);
                            bzero(buffer, MAX_BUFFER_SIZE);
                            if (result < 0)
                            {
                                char file_error_msg[] = "[ERROR] Could not open file";
                                strcpy(buffer, file_error_msg);
                            }
                            else if (result == 0)
                            {
                                char change_error_msg[] = "[ERROR] Status was not change";
                                strcpy(buffer, change_error_msg);
                            }
                            else
                            {
                                char success_msg[] = "Status was succesfully changed to \"served\"";
                                strcpy(buffer, success_msg);
                            }
                            send(client_sock, buffer, MAX_BUFFER_SIZE, 0);
                            fprintf(stdout, "[SERVER] %s\n", buffer);
                        }
                        else if (startsWith("show", command) == true)
                        {
                            sendAllOrdersInPreparingStatus(client_sock);
                        }
                    }
                    else if (startsWith("esc", command) == true)
                    {
                        fprintf(stdout, "[+]Disconnected from: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                        break;
                    }
                    else
                        fprintf(stdout, "[-] Wrong command recived!\n");
                }
            }
        }
    }
    fprintf(stdout, "Poza pętlą\n");
    close(server_sock);
}

int isTableReserved(const char *table_id, const char *date, const char *time)
{
    FILE *file = fopen(RESERVATIONS_FILE, "rb");

    if (file == NULL)
        return -1;
    else
    {
        Reservation reservation;
        while (fread(&reservation, sizeof(Reservation), 1, file))
        {
            if (strcmp(reservation.table->id, table_id) == 0 &&
                strcmp(reservation.date, date) == 0 &&
                strcmp(reservation.hour, time) == 0)
            {
                fclose(file);
                return 1;
            }
        }
        fclose(file);
    }

    return 0;
}

int findReservation(const char *surname, int code, Reservation *reservation)
{
    FILE *file = fopen(RESERVATIONS_FILE, "rb");

    if (file == NULL)
        return -1;
    else
    {
        Reservation foundReservation;
        while (fread(&foundReservation, sizeof(Reservation), 1, file))
        {
            if (foundReservation.code == code && strcmp(foundReservation.surname, surname) == 0)
            {
                reservation->code = foundReservation.code;
                strcpy(reservation->date, foundReservation.date);
                strcpy(reservation->hour, foundReservation.hour);
                reservation->nr_people = foundReservation.nr_people;
                strcpy(reservation->surname, foundReservation.surname);
                reservation->table = foundReservation.table;
                fclose(file);
                return 1;
            }
        }
        fclose(file);
    }

    return 0;
}

int addReservation(FindRequest *rsrv_params, Table *table, Reservation *reservation)
{
    FILE *file = fopen(RESERVATIONS_FILE, "ab");

    if (file == NULL)
        return -1;
    else
    {
        reservation->code = generateReservationCode();
        strncpy(reservation->surname, rsrv_params->surname, sizeof(reservation->surname));
        reservation->nr_people = rsrv_params->people;
        strncpy(reservation->date, rsrv_params->date, sizeof(reservation->date));
        strncpy(reservation->hour, rsrv_params->hour, sizeof(reservation->hour));
        reservation->table = table;

        fwrite(reservation, sizeof(Reservation), 1, file);
        fclose(file);
        return 1;
    }
}

int findAvailableTables(MatchingTable matching_tab[], FindRequest *rsrv_params)
{
    int found_tab_nr = 0; // number of found matching tables for reservation request
    int nr_people = roundToEven(rsrv_params->people);

    for (int i = 0; i < sizeof(ALL_TABLES) / sizeof(ALL_TABLES[0]); i++)
    {
        if (ALL_TABLES[i].nr_seats == nr_people)
        {
            int result = isTableReserved(ALL_TABLES[i].id, rsrv_params->date, rsrv_params->hour);
            if (result == -1)
            {
                return -1; // return error
            }
            else if (result == 0)
            {
                matching_tab[found_tab_nr].table = &ALL_TABLES[i];
                found_tab_nr++;
            }
        }
    }
    return found_tab_nr;
}

int saveOrder(Order *order)
{
    FILE *file = fopen(ORDERS_FILE, "ab");
    if (file == NULL)
        return -1;

    fwrite(order, sizeof(Order), 1, file);
    fclose(file);
    return 1;
}

void printOrderStatusByTable(const char *table_id)
{
    FILE *file = fopen(ORDERS_FILE, "rb");
    if (file == NULL)
        fprintf(stdout, "[ERROR] Cannot read the file\n");

    Order order;
    int nr = 1;
    while (fread(&order, sizeof(Order), 1, file) == 1)
    {
        if (strcmp(order.table_id, table_id) == 0)
        {
            fprintf(stdout, "%d) Order: %s, Status: %s\n", nr, order.order, order.status);
            nr++;
        }
    }

    fclose(file);
}

void printOrderStatusByStatus(const char *status)
{
    FILE *file = fopen(ORDERS_FILE, "rb");
    if (file == NULL)
        fprintf(stdout, "[ERROR] Cannot read the file\n");

    Order order;
    int nr = 1;
    while (fread(&order, sizeof(Order), 1, file) == 1)
    {
        if (strcmp(order.status, status) == 0)
        {
            fprintf(stdout, "%d) Table: %s Course: %s Order: %s\n", nr, order.table_id, order.course, order.order);
            nr++;
        }
    }

    fclose(file);
}

void sendLongestWaitingOrder(int client_sock)
{
    int found = 0;
    char buffer[MAX_BUFFER_SIZE];
    FILE *file = fopen(ORDERS_FILE, "rb");
    if (file == NULL)
    {
        found = -1;
        send(client_sock, &found, sizeof(int), 0);

        char error_msg[] = "[ERROR] Could not open file";
        bzero(buffer, MAX_BUFFER_SIZE);
        strcpy(buffer, error_msg);
        send(client_sock, buffer, MAX_BUFFER_SIZE, 0);
        fprintf(stdout, "[SERVER] %s\n", buffer);
    }
    else
    {
        Order order;
        Order longest_waiting_order;
        time_t longest_waiting_time = time(NULL);

        while (fread(&order, sizeof(Order), 1, file) == 1)
        {
            if (strcmp(order.status, "waiting") == 0)
            {
                fprintf(stdout, "Order in waiting status: %s %s\n", order.table_id, order.course);
                if (order.time < longest_waiting_time)
                {
                    longest_waiting_time = order.time;
                    longest_waiting_order = order;
                    found = 1;
                }
            }
        }

        fclose(file);

        send(client_sock, &found, sizeof(int), 0);
        bzero(buffer, MAX_BUFFER_SIZE);
        if (found == 0)
        {
            char no_order_msg[] = "There are no orders in \"waiting\" status";
            strcpy(buffer, no_order_msg);
        }
        else if (found == 1)
        {
            // Send order to kitchen device
            changeOrderStatus(longest_waiting_order.rsrv_code, longest_waiting_order.course, STATUS_PREPARING);
            sprintf(buffer, "%d %s %s %s",
                    longest_waiting_order.rsrv_code, longest_waiting_order.table_id,
                    longest_waiting_order.course, longest_waiting_order.order);
        }
        send(client_sock, buffer, MAX_BUFFER_SIZE, 0);
        fprintf(stdout, "[SERVER] %s\n", buffer);
    }
}

int changeOrderStatus(int rsrv_code, const char *course, const char *new_status)
{
    FILE *file = fopen("orders.bin", "rb+");
    if (file == NULL)
        return -1;

    Order order;
    int changed = 0;

    while (fread(&order, sizeof(Order), 1, file) == 1)
    {
        if (order.rsrv_code == rsrv_code && strcmp(order.course, course) == 0)
        {
            strcpy(order.status, new_status);
            fseek(file, -sizeof(Order), SEEK_CUR);
            fwrite(&order, sizeof(Order), 1, file);
            changed = 1;
            break;
        }
    }

    fclose(file);
    return changed ? 1 : 0;
}

void sendAllOrdersInPreparingStatus(int client_sock)
{
    int found = 0;
    char buffer[MAX_BUFFER_SIZE];
    FILE *file = fopen("orders.bin", "rb");
    if (file == NULL)
    {
        found = -1;
        send(client_sock, &found, sizeof(int), 0);

        char error_msg[] = "[ERROR] Could not open file";
        bzero(buffer, MAX_BUFFER_SIZE);
        strcpy(buffer, error_msg);
        send(client_sock, buffer, MAX_BUFFER_SIZE, 0);
        fprintf(stdout, "[SERVER] %s\n", buffer);
    }
    else
    {
        // Calculate the number of orders in the file
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        int num_all_orders = file_size / sizeof(Order);
        rewind(file);

        Order order;
        Order inPreparationOrders[num_all_orders];
        int last_order = 0;

        while (fread(&order, sizeof(Order), 1, file) == 1)
        {
            if (strcmp(order.status, STATUS_PREPARING) == 0)
            {
                fprintf(stdout, "Order in preparing status: %s %s\n", order.table_id, order.course);
                inPreparationOrders[last_order] = order;
                last_order++;
                found = 1;
            }
        }

        fclose(file);

        send(client_sock, &found, sizeof(int), 0);
        if (found == 0)
        {
            char no_found_msg[] = "There are no orders in \"in preparation\" status right now.";
            bzero(buffer, MAX_BUFFER_SIZE);
            strcpy(buffer, no_found_msg);
            send(client_sock, buffer, MAX_BUFFER_SIZE, 0);
            fprintf(stdout, "[SERVER] %s\n", buffer);
        }
        else if (found == 1)
        {
            send(client_sock, &last_order, sizeof(int), 0);
            for (int k = 0; k < last_order; k++)
            {
                bzero(buffer, MAX_BUFFER_SIZE);
                sprintf(buffer, "%s %s %s", inPreparationOrders[k].table_id, inPreparationOrders[k].course, inPreparationOrders[k].order);
                send(client_sock, buffer, MAX_BUFFER_SIZE, 0);
            }
            fprintf(stdout, "[SERVER]Orders in preparation send to kitchen device\n");
        }
    }
}

int countReceipt(const char *order)
{
    MenuItem menu[MAX_MENU_ITEMS];

    // Load the menu from the file
    FILE *menuFile = fopen("menu.txt", "r");
    if (menuFile == NULL)
    {
        printf("Error opening menu file.\n");
        return -1;
    }

    // Skip the header and separator lines
    char line[100];
    fgets(line, sizeof(line), menuFile); // Skip the header
    fgets(line, sizeof(line), menuFile); // Skip the separator

    // Read the menu items
    int i = 0;
    while (fgets(line, sizeof(line), menuFile) != NULL && i < MAX_MENU_ITEMS)
    {
        if (line[0] == '|' && line[1] != '=')
        {
            sscanf(line, "| %3s | %30[^|] | %d", menu[i].code, menu[i].name, &menu[i].price);
            i++;
        }
    }

    fclose(menuFile);

    // Parse the order string and calculate the total price
    char orderCopy[100];
    strcpy(orderCopy, order);

    char *token = strtok(orderCopy, " ");
    int totalPrice = 0;

    while (token != NULL)
    {
        char code[MAX_CODE_LENGTH + 1];
        int quantity;
        sscanf(token, "%[^-]-%d", code, &quantity);

        // Find the code in the menu and update the total price
        for (int i = 0; i < MAX_MENU_ITEMS; i++)
        {
            if (strcmp(menu[i].code, code) == 0)
            {
                totalPrice += menu[i].price * quantity;
                break;
            }
        }

        token = strtok(NULL, " ");
    }
    fprintf(stdout, "Total price: %d\n", totalPrice);

    return totalPrice;
}

int allOrdersAreServed()
{
    FILE *file = fopen(ORDERS_FILE, "rb");
    if (file == NULL)
    {
        fprintf(stdout, "[SERVER STOP] Cannot open a file\n");
        return -1;
    }
    Order order;
    while (fread(&order, sizeof(Order), 1, file) == 1)
    {
        if (strcmp(order.status, STATUS_SERVED) != 0)
        {
            fprintf(stdout, "[SERVER STOP] Not all orders are served, server cannot be closed now\n");
            return 0;
        }
    }

    fclose(file);
    fprintf(stdout, "[SERVER STOP] All orders are served...\n");
    return 1;
}

void prepareServerForConnections(struct sockaddr_in *server_addr, int *server_sock, const char *ip, int *port, int *n)
{
    initializeServerAddress(server_addr, ip, port); // Initialize the server address
    bindServer(server_addr, server_sock, port, n);  // Bind server descriptor to server address
    listenForIncomingConnections(server_sock);      // Turn on the socket to listen for incoming connections
}

void createSocket(int *server_sock)
{
    *server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*server_sock < 0)
    {
        perror("[-] Socket error.\n");
        exit(1);
    }
    fprintf(stdout, "[+] TCP server socket created.\n");
}

void initializeServerAddress(struct sockaddr_in *server_addr, const char *ip, int *port)
{
    memset(server_addr, '\0', sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = *port;
    server_addr->sin_addr.s_addr = inet_addr(ip);
}

void bindServer(struct sockaddr_in *server_addr, int *server_sock, int *port, int *n)
{
    *n = bind(*server_sock, (struct sockaddr *)server_addr, sizeof(*server_addr));
    if (*n < 0)
    {
        perror("[-] Bind error.\n");
        exit(1);
    }
    fprintf(stdout, "[+] Bind to the port number: %d.\n", *port);
}

void listenForIncomingConnections(int *server_sock)
{
    if (listen(*server_sock, 30) == 0)
    {
        fprintf(stdout, "[+] Listening...\n");
    }
    else
    {
        fprintf(stdout, "[-] Bind error.\n");
    }
}

void establishNewConnection(struct sockaddr_in *client_addr, socklen_t *addr_size, int *server_sock, int *client_sock)
{
    *client_sock = accept(*server_sock, (struct sockaddr *)client_addr, addr_size);
    if (*client_sock < 0)
    {
        exit(1);
    }
    fprintf(stdout, "[+] New connection accepted from: %s:%d.\n", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
}

int generateReservationCode()
{
    int min = 1000;
    int max = 9999;

    static int initialized = 0;
    if (!initialized)
    {
        srand(time(NULL));
        initialized = 1;
    }
    return (rand() % max) + min;
}

bool startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre);
    int result = strncmp(str, pre, lenpre);
    if (result == 0)
        return true;
    else
        return false;
}

int roundToEven(int num)
{
    // Check if the number is odd
    if (num % 2 != 0)
    {
        return num + 1;
    }
    return num;
}
