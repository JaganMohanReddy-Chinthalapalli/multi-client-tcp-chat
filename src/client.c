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
#define RECEIVE_BUFFER_SIZE 4096

int sock_fd;
char username[USERNAME_SIZE];

pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Thread function:
 * Continuously receives newline-delimited messages
 * from the server.
 */
void *receive_messages(void *arg)
{
    char buffer[RECEIVE_BUFFER_SIZE];
    char message[BUFFER_SIZE];

    size_t buffer_length = 0;

    (void)arg;

    memset(buffer, 0, sizeof(buffer));

    while (1)
    {
        if (buffer_length >= sizeof(buffer) - 1)
        {
            fprintf(stderr,
                    "\nReceive buffer full. Messages discarded.\n");

            buffer_length = 0;
        }

        int bytes_received =
            recv(sock_fd,
                 buffer + buffer_length,
                 sizeof(buffer) - buffer_length - 1,
                 0);

        if (bytes_received <= 0)
        {
            pthread_mutex_lock(&print_mutex);

            printf("\nServer disconnected.\n");

            pthread_mutex_unlock(&print_mutex);

            break;
        }

        buffer_length += (size_t)bytes_received;
        buffer[buffer_length] = '\0';

        /*
         * Process every complete newline-delimited
         * message currently in the buffer.
         */
        while (1)
        {
            char *newline_position =
                memchr(buffer,
                       '\n',
                       buffer_length);

            if (newline_position == NULL)
            {
                break;
            }

            size_t message_length =
                (size_t)(newline_position - buffer);

            if (message_length >= sizeof(message))
            {
                fprintf(stderr,
                        "\nReceived message is too long.\n");

                size_t remaining_length =
                    buffer_length -
                    (message_length + 1);

                memmove(buffer,
                        newline_position + 1,
                        remaining_length);

                buffer_length = remaining_length;
                buffer[buffer_length] = '\0';

                continue;
            }

            memcpy(message,
                   buffer,
                   message_length);

            message[message_length] = '\0';

            pthread_mutex_lock(&print_mutex);

            printf("\r\033[K%s\n",
                   message);

            printf("%s: ",
                   username);

            fflush(stdout);

            pthread_mutex_unlock(&print_mutex);

            /*
             * Remove the processed message and
             * preserve any remaining bytes.
             */
            size_t remaining_length =
                buffer_length -
                (message_length + 1);

            memmove(buffer,
                    newline_position + 1,
                    remaining_length);

            buffer_length = remaining_length;
            buffer[buffer_length] = '\0';
        }
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    struct sockaddr_in server_addr;
    pthread_t receive_thread;

    char message[BUFFER_SIZE];
    char username_message[USERNAME_SIZE + 12];
    char formatted_message[BUFFER_SIZE];

    /*
     * Check command-line arguments.
     *
     * Usage:
     * ./client <server-ip> <port>
     */
    if (argc != 3)
    {
        printf("Usage: %s <server-ip> <port>\n",
               argv[0]);

        return 1;
    }

    /*
     * Ask for username.
     */
    printf("Enter your username: ");

    if (fgets(username,
              sizeof(username),
              stdin) == NULL)
    {
        printf("Failed to read username.\n");
        return 1;
    }

    username[strcspn(username, "\n")] = '\0';

    if (strlen(username) == 0)
    {
        printf("Username cannot be empty.\n");
        return 1;
    }

    /*
     * Create socket.
     */
    sock_fd =
        socket(AF_INET,
               SOCK_STREAM,
               0);

    if (sock_fd == -1)
    {
        perror("socket");
        return 1;
    }

    printf("Client socket created successfully.\n");

    /*
     * Configure server address.
     */
    memset(&server_addr,
           0,
           sizeof(server_addr));

    server_addr.sin_family =
        AF_INET;

    server_addr.sin_port =
        htons((uint16_t)atoi(argv[2]));

    if (inet_pton(AF_INET,
                  argv[1],
                  &server_addr.sin_addr) <= 0)
    {
        fprintf(stderr,
                "Invalid server IP address: %s\n",
                argv[1]);

        close(sock_fd);

        return 1;
    }

    /*
     * Connect to server.
     */
    if (connect(sock_fd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) == -1)
    {
        perror("connect");

        close(sock_fd);

        return 1;
    }

    printf("Connected to server %s:%s.\n",
           argv[1],
           argv[2]);

    /*
     * Send username to server.
     *
     * Protocol:
     * USERNAME:<username>\n
     *
     * Example:
     * USERNAME:Rahul\n
     */
    int username_length =
        snprintf(username_message,
                 sizeof(username_message),
                 "USERNAME:%s\n",
                 username);

    if (username_length < 0 ||
        (size_t)username_length >=
            sizeof(username_message))
    {
        fprintf(stderr,
                "Username is too long.\n");

        close(sock_fd);

        return 1;
    }

    if (send(sock_fd,
             username_message,
             (size_t)username_length,
             0) == -1)
    {
        perror("send");

        close(sock_fd);

        return 1;
    }

    /*
     * Create receive thread.
     */
    if (pthread_create(&receive_thread,
                       NULL,
                       receive_messages,
                       NULL) != 0)
    {
        perror("pthread_create");

        close(sock_fd);

        return 1;
    }

    /*
     * Main thread handles sending messages.
     */
    while (1)
    {
        printf("%s: ",
               username);

        fflush(stdout);

        if (fgets(message,
                  sizeof(message),
                  stdin) == NULL)
        {
            break;
        }

        /*
         * Remove newline character.
         */
        message[strcspn(message, "\n")] = '\0';

        /*
         * Exit command.
         */
        if (strcmp(message, "/quit") == 0)
        {
            const char *quit_message =
                "/quit\n";

            if (send(sock_fd,
                     quit_message,
                     strlen(quit_message),
                     0) == -1)
            {
                perror("send");
            }

            break;
        }

        /*
         * Send commands and normal messages
         * as newline-delimited frames.
         *
         * Examples:
         *
         * /users
         * /msg Jagan Hello
         * Hello Jagan
         */
        size_t message_length =
            strlen(message);

        if (message_length + 1 >=
            sizeof(formatted_message))
        {
            fprintf(stderr,
                    "Message is too long.\n");

            continue;
        }

        memcpy(formatted_message,
               message,
               message_length);

        formatted_message[message_length] =
            '\n';

        formatted_message[message_length + 1] =
            '\0';

        if (send(sock_fd,
                 formatted_message,
                 message_length + 1,
                 0) == -1)
        {
            perror("send");
            break;
        }
    }

    /*
     * Close socket.
     *
     * Closing the socket causes the receive
     * thread to finish if it is still blocked
     * inside recv().
     */
    shutdown(sock_fd,
             SHUT_RDWR);

    pthread_join(receive_thread,
                 NULL);

    close(sock_fd);

    printf("\nChat ended.\n");

    return 0;
}
