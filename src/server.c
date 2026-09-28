#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define USERNAME_SIZE 50
#define MAX_CLIENTS 10
#define RECEIVE_BUFFER_SIZE 4096

typedef struct
{
    int socket_fd;
    char username[USERNAME_SIZE];
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast_message(const char *message, int sender_fd)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].socket_fd != sender_fd)
        {
            send(clients[i].socket_fd,
                 message,
                 strlen(message),
                 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

int send_private_message(const char *target_username,
                         const char *message)
{
    int target_fd = -1;

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++)
    {
        if (strcmp(clients[i].username,
                   target_username) == 0)
        {
            target_fd = clients[i].socket_fd;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    if (target_fd == -1)
    {
        return -1;
    }

    if (send(target_fd,
             message,
             strlen(message),
             0) < 0)
    {
        return -2;
    }

    return 0;
}

int username_exists(const char *username,
                    int current_fd)
{
    int exists = 0;

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].socket_fd != current_fd &&
            strcmp(clients[i].username,
                   username) == 0)
        {
            exists = 1;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    return exists;
}

void send_online_users(int client_fd)
{
    char users_message[BUFFER_SIZE];

    size_t used =
        snprintf(users_message,
                 sizeof(users_message),
                 "Online users:\n");

    if (used >= sizeof(users_message))
    {
        return;
    }

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].username[0] == '\0')
        {
            continue;
        }

        int written =
            snprintf(users_message + used,
                     sizeof(users_message) - used,
                     "%d. %s\n",
                     i + 1,
                     clients[i].username);

        if (written < 0)
        {
            break;
        }

        if ((size_t)written >=
            sizeof(users_message) - used)
        {
            used = sizeof(users_message) - 1;
            break;
        }

        used += (size_t)written;
    }

    pthread_mutex_unlock(&clients_mutex);

    send(client_fd,
         users_message,
         strlen(users_message),
         0);
}

void remove_client(int client_fd)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].socket_fd == client_fd)
        {
            for (int j = i; j < client_count - 1; j++)
            {
                clients[j] = clients[j + 1];
            }

            client_count--;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

int set_client_username(int client_fd,
                        const char *username)
{
    int result = 0;

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].socket_fd == client_fd)
        {
            strncpy(clients[i].username,
                    username,
                    USERNAME_SIZE - 1);

            clients[i].username[USERNAME_SIZE - 1] =
                '\0';

            result = 1;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    return result;
}

void process_message(int client_fd,
                     const char *username,
                     const char *message)
{
    if (strcmp(message, "/quit") == 0)
    {
        char quit_message[] =
            "Peer has left the chat.\n";

        broadcast_message(quit_message,
                          client_fd);

        printf("%s disconnected.\n",
               username);

        return;
    }

    if (strcmp(message, "/users") == 0)
    {
        send_online_users(client_fd);
        return;
    }

    if (strncmp(message, "/msg ", 5) == 0)
    {
        char private_command[BUFFER_SIZE];

        strncpy(private_command,
                message + 5,
                sizeof(private_command) - 1);

        private_command[sizeof(private_command) - 1] =
            '\0';

        char *space =
            strchr(private_command, ' ');

        if (space == NULL ||
            space == private_command ||
            *(space + 1) == '\0')
        {
            char error_message[] =
                "Usage: /msg <username> <message>\n";

            send(client_fd,
                 error_message,
                 strlen(error_message),
                 0);

            return;
        }

        *space = '\0';

        char *target_username =
            private_command;

        char *private_text =
            space + 1;

        char private_message[BUFFER_SIZE];

        snprintf(private_message,
                 sizeof(private_message),
                 "[Private] %s: %s\n",
                 username,
                 private_text);

        int result =
            send_private_message(target_username,
                                 private_message);

        if (result == -1)
        {
            char error_message[BUFFER_SIZE];

            size_t target_length =
                strlen(target_username);

            if (target_length >
                sizeof(error_message) - 30)
            {
                target_length =
                    sizeof(error_message) - 30;
            }

            snprintf(error_message,
                     sizeof(error_message),
                     "User '%.*s' is not connected.\n",
                     (int)target_length,
                     target_username);

            send(client_fd,
                 error_message,
                 strlen(error_message),
                 0);
        }
        else if (result == -2)
        {
            char error_message[] =
                "Failed to send private message.\n";

            send(client_fd,
                 error_message,
                 strlen(error_message),
                 0);
        }
        else
        {
            printf("%s",
                   private_message);
        }

        return;
    }

    if (strlen(message) > 0)
    {
        char formatted_message[BUFFER_SIZE];

        snprintf(formatted_message,
                 sizeof(formatted_message),
                 "%s: %.*s\n",
                 username,
                 (int)(sizeof(formatted_message) -
                       strlen(username) -
                       4),
                 message);

        printf("%s",
               formatted_message);

        broadcast_message(formatted_message,
                          client_fd);
    }
}

void *handle_client(void *arg)
{
    int client_fd = *(int *)arg;

    free(arg);

    char buffer[BUFFER_SIZE];
    char username[USERNAME_SIZE];

    char receive_buffer[RECEIVE_BUFFER_SIZE];

    size_t receive_length = 0;

    memset(username,
           0,
           sizeof(username));

    memset(receive_buffer,
           0,
           sizeof(receive_buffer));

    int bytes_received =
        recv(client_fd,
             buffer,
             sizeof(buffer) - 1,
             0);

    if (bytes_received <= 0)
    {
        close(client_fd);
        remove_client(client_fd);
        return NULL;
    }

    buffer[bytes_received] = '\0';

    char *newline =
        strchr(buffer, '\n');

    if (newline == NULL ||
        strncmp(buffer, "USERNAME:", 9) != 0)
    {
        char error_message[] =
            "Invalid username format.\n";

        send(client_fd,
             error_message,
             strlen(error_message),
             0);

        close(client_fd);
        remove_client(client_fd);

        return NULL;
    }

    *newline = '\0';

    strncpy(username,
            buffer + 9,
            sizeof(username) - 1);

    username[sizeof(username) - 1] =
        '\0';

    if (strlen(username) == 0)
    {
        char error_message[] =
            "Username cannot be empty.\n";

        send(client_fd,
             error_message,
             strlen(error_message),
             0);

        close(client_fd);
        remove_client(client_fd);

        return NULL;
    }

    if (username_exists(username,
                        client_fd))
    {
        char error_message[] =
            "Username already exists. Please choose another username.\n";

        send(client_fd,
             error_message,
             strlen(error_message),
             0);

        close(client_fd);
        remove_client(client_fd);

        printf("Duplicate username rejected: %s\n",
               username);

        return NULL;
    }

    set_client_username(client_fd,
                         username);

    printf("%s connected.\n",
           username);

    /*
     * If additional bytes arrived together with
     * the username, preserve them for processing.
     */
    size_t username_message_length =
        (size_t)(bytes_received -
                 ((newline + 1) - buffer));

    if (username_message_length > 0)
    {
        if (username_message_length >
            sizeof(receive_buffer) - 1)
        {
            username_message_length =
                sizeof(receive_buffer) - 1;
        }

        memcpy(receive_buffer,
               newline + 1,
               username_message_length);

        receive_length =
            username_message_length;

        receive_buffer[receive_length] =
            '\0';
    }

    while (1)
    {
        /*
         * Process every complete newline-delimited
         * message currently in the receive buffer.
         */
        while (1)
        {
            char *message_end =
                memchr(receive_buffer,
                       '\n',
                       receive_length);

            if (message_end == NULL)
            {
                break;
            }

            size_t message_length =
                (size_t)(message_end -
                         receive_buffer);

            char message[BUFFER_SIZE];

            if (message_length >=
                sizeof(message))
            {
                char error_message[] =
                    "Message too long.\n";

                send(client_fd,
                     error_message,
                     strlen(error_message),
                     0);

                size_t remaining =
                    receive_length -
                    (message_length + 1);

                memmove(receive_buffer,
                        message_end + 1,
                        remaining);

                receive_length =
                    remaining;

                continue;
            }

            memcpy(message,
                   receive_buffer,
                   message_length);

            message[message_length] =
                '\0';

            size_t remaining =
                receive_length -
                (message_length + 1);

            memmove(receive_buffer,
                    message_end + 1,
                    remaining);

            receive_length =
                remaining;

            if (strcmp(message,
                       "/quit") == 0)
            {
                process_message(client_fd,
                                username,
                                message);

                goto client_exit;
            }

            process_message(client_fd,
                            username,
                            message);
        }

        if (receive_length >=
            sizeof(receive_buffer) - 1)
        {
            char error_message[] =
                "Receive buffer full. Message discarded.\n";

            send(client_fd,
                 error_message,
                 strlen(error_message),
                 0);

            receive_length = 0;
        }

        bytes_received =
            recv(client_fd,
                 buffer,
                 sizeof(buffer),
                 0);

        if (bytes_received <= 0)
        {
            printf("%s disconnected.\n",
                   username);

            break;
        }

        if (receive_length +
            (size_t)bytes_received >=
            sizeof(receive_buffer))
        {
            char error_message[] =
                "Receive buffer overflow. Data discarded.\n";

            send(client_fd,
                 error_message,
                 strlen(error_message),
                 0);

            receive_length = 0;
            continue;
        }

        memcpy(receive_buffer + receive_length,
               buffer,
               (size_t)bytes_received);

        receive_length +=
            (size_t)bytes_received;
    }

client_exit:

    close(client_fd);
    remove_client(client_fd);

    return NULL;
}

int main(void)
{
    int server_fd;

    struct sockaddr_in server_address;

    server_fd =
        socket(AF_INET,
               SOCK_STREAM,
               0);

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    printf("Server socket created successfully.\n");

    int option = 1;

    if (setsockopt(server_fd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &option,
                   sizeof(option)) < 0)
    {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    memset(&server_address,
           0,
           sizeof(server_address));

    server_address.sin_family =
        AF_INET;

    server_address.sin_addr.s_addr =
        INADDR_ANY;

    server_address.sin_port =
        htons(PORT);

    if (bind(server_fd,
             (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    printf("Server bound to port %d.\n",
           PORT);

    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Server is listening...\n");

    while (1)
    {
        struct sockaddr_in client_address;

        socklen_t client_length =
            sizeof(client_address);

        int client_fd =
            accept(server_fd,
                   (struct sockaddr *)&client_address,
                   &client_length);

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);

        if (client_count >= MAX_CLIENTS)
        {
            pthread_mutex_unlock(&clients_mutex);

            char error_message[] =
                "Server is full. Try again later.\n";

            send(client_fd,
                 error_message,
                 strlen(error_message),
                 0);

            close(client_fd);

            continue;
        }

        clients[client_count].socket_fd =
            client_fd;

        clients[client_count].username[0] =
            '\0';

        client_count++;

        pthread_mutex_unlock(&clients_mutex);

        printf("Client connected successfully.\n");

        int *client_socket =
            malloc(sizeof(int));

        if (client_socket == NULL)
        {
            perror("malloc");

            close(client_fd);
            remove_client(client_fd);

            continue;
        }

        *client_socket =
            client_fd;

        pthread_t thread;

        if (pthread_create(&thread,
                           NULL,
                           handle_client,
                           client_socket) != 0)
        {
            perror("pthread_create");

            free(client_socket);
            close(client_fd);
            remove_client(client_fd);

            continue;
        }

        pthread_detach(thread);
    }

    close(server_fd);

    return 0;
}
