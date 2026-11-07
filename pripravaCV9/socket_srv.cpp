// ***************************************************************************
//
// Program example for labs in subject Operating Systems
//
// Petr Olivka, Dept. of Computer Science, petr.olivka@vsb.cz, 2026
//
// Example of socket server.
//
// This program is example of socket server and it allows to connect and serve
// the only one client.
// The mandatory argument of program is port number for listening.
//
// ***************************************************************************

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cerrno>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <vector>
#include <algorithm>
#include <mqueue.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>

#define STR_CLOSE   "close"
#define STR_QUIT    "quit"

// ***************************************************************************
// log messages

#define LOG_ERROR               0       // errors
#define LOG_INFO                1       // information and notifications
#define LOG_DEBUG               2       // debug messages

// debug flag
int g_debug = LOG_INFO;

//MOJE GLOBALNI PROMENNE

struct Client
{
    pid_t pid;
    int client_socket_number;
    char nickname[64];
};

enum MessageType {
    MSG_TEXT,
    MSG_JOIN,
    MSG_LOCK,
    MSG_UNLOCK,
};

struct Message {
    pid_t sender_pid;
    MessageType type;
    char nickname[64];
    char msg_text[256];
};

std::vector<Client> clients;

bool messages_locked = false;
pid_t lock_owner_pid = -1;

//KONEC MYCH GLOBALNICH PROMENNYCH

void log_msg( int t_log_level, const char *t_form, ... )
{
    const char *out_fmt[] = {
            "ERR: (%d-%s) %s\n",
            "INF: %s\n",
            "DEB: %s\n" };

    if ( t_log_level && t_log_level > g_debug )
    {
        return;
    }

    char l_buf[ 4096 ];
    va_list l_arg;
    va_start( l_arg, t_form );

    vsnprintf( l_buf, sizeof( l_buf ), t_form, l_arg );

    va_end( l_arg );

    switch ( t_log_level )
    {
        case LOG_INFO:
        case LOG_DEBUG:
            fprintf( stdout, out_fmt[ t_log_level ], l_buf );
            break;

        case LOG_ERROR:
            fprintf( stderr, out_fmt[ t_log_level ], errno, strerror( errno ), l_buf );
            break;
    }
}

// ***************************************************************************
// help

const char g_help[] =
    "\n"
    "  Socket server example.\n"
    "\n"
    "  Use: %s [-h] [-d] port_number\n"
    "\n"
    "    -d  debug mode \n"
    "    -h  this help\n"
    "\n"
    ;

// ***************************************************************************


void handleClient(int client_socket, mqd_t message_queue)
{
    char buffer[256];
    char nickname[64];

    // Pozadame klienta o prezdivku
    const char* prompt = "Zadej svou prezdivku: ";
    write(client_socket, prompt, strlen(prompt));

    // Precteme prezdivku
    int ret = read(client_socket, nickname, sizeof(nickname) - 1);

    if (ret <= 0)
    {
        close(client_socket);
        exit(EXIT_SUCCESS);
    }

    nickname[ret] = '\0';

    // Odstraneni konce radku
    nickname[strcspn(nickname, "\r\n")] = '\0';

    // Pokud klient nezadal prezdivku
    if (strlen(nickname) == 0)
    {
        strcpy(nickname, "Unknown");
    }

    Message message = {};

    message.sender_pid = getpid();
    message.type = MSG_JOIN;

    strncpy(
        message.nickname,
        nickname,
        sizeof(message.nickname) - 1
    );

    if (mq_send(
            message_queue,
            (const char*)&message,
            sizeof(message),
            0
        ) < 0)
    {
        perror("mq_send");
    }

    while (1)
    {
        int ret = read(client_socket, buffer, sizeof(buffer) - 1);

        if (ret <= 0)
        {
            break;
        }

        buffer[ret] = '\0';

        Message message = {};

        message.sender_pid = getpid();

        if (strncmp(buffer, "lock:", 5) == 0)
        {
            message.type = MSG_LOCK;
        }
        else if (strncmp(buffer, "unlock:", 7) == 0)
        {
            message.type = MSG_UNLOCK;
        }
        else
        {
            message.type = MSG_TEXT;
        }

        strncpy(
            message.nickname,
            nickname,
            sizeof(message.nickname) - 1
        );

        message.nickname[sizeof(message.nickname) - 1] = '\0';

        int copy_length = ret;

        if (copy_length >= (int)sizeof(message.msg_text))
        {
            copy_length = sizeof(message.msg_text) - 1;
        }

        memcpy(message.msg_text, buffer, copy_length);
        message.msg_text[copy_length] = '\0';

        if (mq_send(
                message_queue,
                (const char*)&message,
                sizeof(message),
                0
            ) < 0)
        {
            perror("mq_send");
        }
    }

    close(client_socket);
    exit(EXIT_SUCCESS);
}


int main( int t_argc, char **t_argv )
{
    int l_port = 0;



    // parsing arguments
    for ( int inx = 1; inx < t_argc; inx++ )
    {
        if ( strcmp( t_argv[ inx ], "-d" ) == 0 )
        {
            g_debug = LOG_DEBUG;
        }

        if ( strcmp( t_argv[ inx ], "-h" ) == 0 )
        {
            printf( g_help, t_argv[ 0 ] );
            exit( EXIT_SUCCESS );
        }

        if ( *t_argv[ inx ] != '-' )
        {
            if ( l_port == 0 )
            {
                l_port = atoi( t_argv[ inx ] );
            }
        }
    }


    if ( l_port <= 0 )
    {
        log_msg( LOG_INFO, "Bad or missing port number %d!", l_port );
        printf( g_help, t_argv[ 0 ] );
        exit( EXIT_FAILURE );
    }

    log_msg( LOG_INFO, "Server will listen on port: %d.", l_port );

    mq_unlink("/osys_chat_queue");

    // VYTVORENI FRONTY ZPRAV

    struct mq_attr attr;

    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(Message);
    attr.mq_curmsgs = 0;

    mqd_t message_queue = mq_open(
        "/osys_chat_queue",
        O_CREAT | O_RDWR | O_NONBLOCK,
        0600,
        &attr
    );

    if (message_queue == (mqd_t)-1)
    {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }


    // socket creation
    int l_sock_listen = socket( AF_INET, SOCK_STREAM, 0 );
    if ( l_sock_listen < 0 )
    {
        log_msg( LOG_ERROR, "Unable to create socket.");
        exit( EXIT_FAILURE );
    }

    in_addr l_addr_any = { INADDR_ANY };
    sockaddr_in l_srv_addr;
    l_srv_addr.sin_family = AF_INET;
    l_srv_addr.sin_port = htons( l_port );
    l_srv_addr.sin_addr = l_addr_any;

    // Enable the port number reusing
    int l_opt = 1;
    if ( setsockopt( l_sock_listen, SOL_SOCKET, SO_REUSEADDR, &l_opt, sizeof( l_opt ) ) < 0 )
    {
      log_msg( LOG_ERROR, "Unable to set socket option!" );
    }

    // assign port number to socket
    if ( bind( l_sock_listen, (const sockaddr * ) &l_srv_addr, sizeof( l_srv_addr ) ) < 0 )
    {
        log_msg( LOG_ERROR, "Bind failed!" );
        close( l_sock_listen );
        exit( EXIT_FAILURE );
    }

    // listenig on set port
    if ( listen( l_sock_listen, 10 ) < 0 )
    {
        log_msg( LOG_ERROR, "Unable to listen on given port!" );
        close( l_sock_listen );
        exit( EXIT_FAILURE );
    }

    log_msg( LOG_INFO, "Enter 'quit' to quit server." );

        int l_sock_client = -1;

        // list of fd sources
        pollfd l_read_poll[3];

        l_read_poll[0].fd = STDIN_FILENO;
        l_read_poll[0].events = POLLIN;

        l_read_poll[1].fd = l_sock_listen;
        l_read_poll[1].events = POLLIN;

        l_read_poll[2].fd = (int)message_queue;
        l_read_poll[2].events = POLLIN;

        while ( 1 ) // wait for new client
        {
            // VYNULOVANI REVENTS
            for (int i = 0; i < 3; i++)
            {
                l_read_poll[i].revents = 0;
            }

            // select from fds
            int l_poll = poll(l_read_poll, 3, 500);

            if ( l_poll < 0 )
            {
                log_msg( LOG_ERROR, "Function poll failed!" );
                exit( EXIT_FAILURE );
            }

            // KONTROLA UKONCENYCH POTOMKU

            int status;
            pid_t finished_pid;

            while ((finished_pid = waitpid(-1, &status, WNOHANG)) > 0)
            {
                //KDYZ SE KLIENT ODPOJI A PRED TIM ZADAL LOCK TAK SE ZPRAVY UNLOCKNOU
                if (finished_pid == lock_owner_pid)
                {
                    messages_locked = false;
                    lock_owner_pid = -1;

                    printf("Klient odpojil, broadcast byl odemcen.\n");
                }

                // Najdeme klienta podle PID
                for (auto it = clients.begin(); it != clients.end(); ++it)
                {
                    if (it->pid == finished_pid)
                    {
                        // Zavreme socket klienta
                        close(it->client_socket_number);

                        // Odstranime klienta ze seznamu
                        clients.erase(it);

                        printf("Klient s PID %d se odpojil.\n", finished_pid);

                        break;
                    }
                }
            }

            //SERVER STDIN
            if ( l_read_poll[ 0 ].revents & POLLIN )
            { // data on stdinu SERVERU
                char server_stdin_buffer[ 128 ];
                char server_message[256];
                
                int ret = read( STDIN_FILENO, server_stdin_buffer, sizeof( server_stdin_buffer) );
                if ( ret == 0 )
                {
                    log_msg( LOG_DEBUG, "Stdin closed." );
                    exit( EXIT_SUCCESS );
                }
                if ( ret < 0 )
                {
                    log_msg( LOG_DEBUG, "Unable to read from stdin!" );
                    exit( EXIT_FAILURE );
                }

                log_msg( LOG_DEBUG, "Read %d bytes from stdin", ret );
                // request to quit?
                if ( !strncmp( server_stdin_buffer, STR_QUIT, strlen( STR_QUIT ) ) )
                {
                    log_msg( LOG_INFO, "Request to 'quit' entered.");
                    close( l_sock_listen );
                    exit( EXIT_SUCCESS );
                }

                // Vytvoření zprávy "nick: textova zprava"
                int server_message_length = snprintf(
                    server_message,
                    sizeof(server_message),
                    "SERVER: %.*s",
                    ret,
                    server_stdin_buffer
                );

                if (server_message_length < 0)
                    break;

                if (server_message_length >= (int)sizeof(server_message))
                    server_message_length = sizeof(server_message) - 1;

                // Rozeslani zpravy vsem pripojenym klientum
                for (const Client& client : clients)
                {
                    int sent = write(
                        client.client_socket_number,
                        server_message,
                        server_message_length
                    );

                    if (sent < 0)
                    {
                        perror("write");
                    }
                }

            }

            // MESSAGE QUEUE
            if (l_read_poll[2].revents & POLLIN)
            {
                Message received_message;

                int ret = mq_receive(
                    message_queue,
                    (char*)&received_message,
                    sizeof(received_message),
                    NULL
                );

                if (ret < 0)
                {
                    perror("mq_receive");
                }
                else
                {
                    if (received_message.type == MSG_JOIN)
                    {
                        // Najdeme klienta podle PID
                        for (Client& client : clients)
                        {
                            if (client.pid == received_message.sender_pid)
                            {
                                strncpy(
                                    client.nickname,
                                    received_message.nickname,
                                    sizeof(client.nickname) - 1
                                );

                                client.nickname[sizeof(client.nickname) - 1] = '\0';

                                printf("Klient %s se pripojil.\n", client.nickname);

                                break;
                            }
                        }
                    }
                    else if (received_message.type == MSG_LOCK)
                    {
                        if (!messages_locked)
                        {
                            messages_locked = true;
                            lock_owner_pid = received_message.sender_pid;

                            printf("Klient %s zamkl broadcast.\n",
                                   received_message.nickname);
                        }
                    }
                    else if (received_message.type == MSG_UNLOCK)
                    {
                        if (messages_locked &&
                            lock_owner_pid == received_message.sender_pid)
                        {
                            messages_locked = false;
                            lock_owner_pid = -1;

                            printf("Klient %s odemkl broadcast.\n",
                                   received_message.nickname);
                        }
                    }
                    else if (received_message.type == MSG_TEXT) {
                        if (!messages_locked ||
                            received_message.sender_pid == lock_owner_pid)
                        {
                            // Rozeslani zpravy vsem pripojenym klientum
                            char broadcast_message[320];

                            int message_length = snprintf(
                                broadcast_message,
                                sizeof(broadcast_message),
                                "%s: %s\n",
                                received_message.nickname,
                                received_message.msg_text
                            );

                            if (message_length > 0 &&
                                message_length < (int)sizeof(broadcast_message))
                            {
                                for (const Client& client : clients)
                                {
                                    int sent = write(
                                        client.client_socket_number,
                                        broadcast_message,
                                        message_length
                                    );

                                    if (sent < 0)
                                    {
                                        perror("write");
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if ( l_read_poll[ 1 ].revents & POLLIN )
            { // new client?
                sockaddr_in l_rsa;
                int l_rsa_size = sizeof( l_rsa );
                // new connection
                l_sock_client = accept( l_sock_listen, ( sockaddr * ) &l_rsa, ( socklen_t * ) &l_rsa_size );
                if ( l_sock_client < 0 )
                {
                    log_msg( LOG_ERROR, "Unable to accept new client." );
                    close( l_sock_listen );
                    exit( EXIT_FAILURE );
                }
                uint l_lsa = sizeof( l_srv_addr );
                // my IP
                getsockname( l_sock_client, ( sockaddr * ) &l_srv_addr, &l_lsa );
                log_msg( LOG_INFO, "My IP: '%s'  port: %d",
                                 inet_ntoa( l_srv_addr.sin_addr ), ntohs( l_srv_addr.sin_port ) );
                // client IP
                getpeername( l_sock_client, ( sockaddr * ) &l_srv_addr, &l_lsa );
                log_msg( LOG_INFO, "Client IP: '%s'  port: %d",
                                 inet_ntoa( l_srv_addr.sin_addr ), ntohs( l_srv_addr.sin_port ) );

                ///////////////////////////////////////////////////////////////////////////////////////////////////
                ///     MUJ KOD
                ///////////////////////////////////////////////////////////////////////////////////////////////////

                // CREATION OF FIRST CHILD
                pid_t client_child = fork();

                if ( client_child < 0 ) {
                    //ERROR
                    perror("Unable to create new child process.(client_child)");
                    close( l_sock_client );
                }
                else if (client_child > 0)
                {
                    // PARENT

                    Client new_client;
                    new_client.pid = client_child;
                    new_client.client_socket_number = l_sock_client;
                    new_client.nickname[0] = '\0';

                    clients.push_back(new_client);
                }
                else
                {
                    // CHILD

                    close(l_sock_listen);

                    // Zavření socketů ostatních klientů
                    for (const Client& client : clients)
                    {
                        close(client.client_socket_number);
                    }

                    // Obsluha aktuálního klienta
                    handleClient(l_sock_client, message_queue);
                }
                //PARENT
            }

        } // while wait for client



    return 0;
} // main

