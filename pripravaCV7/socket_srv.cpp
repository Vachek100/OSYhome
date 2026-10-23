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
#include <pthread.h>
#include <vector>
#include <algorithm>

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
    int client_socket_number;
    char nickname[64];
};

std::vector<Client> global_list_of_clients;
pthread_mutex_t global_clients_mutex = PTHREAD_MUTEX_INITIALIZER;

bool messages_locked = false;
int lock_owner_socket = -1;

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


//TOTO JE ZASE FUNKCE KTERA FUNGUJE JAKO CISNIK, TAKOVEJ MIDDLEMAN
void* handleClient(void* arg)
{
    int client_socket = *(int*)arg;
    delete (int*)arg;

    // Vyžádání nicku od klienta
    //TOTO SE NAPISE KLIENTOVI DO TERMINALU
    const char* prompt = "Zadej nick: ";
    write(client_socket, prompt, strlen(prompt));

    char nickname[64];
    int nickname_ret = read(client_socket, nickname, sizeof(nickname) - 1);

    if (nickname_ret <= 0)
    {
        close(client_socket);
        return NULL;
    }

    nickname[nickname_ret] = '\0';

    // Odstranění konce řádku z nicku
    nickname[strcspn(nickname, "\r\n")] = '\0';

    // Pridani klienta do spolecneho seznamu ktery je na serveru
    // prirovnal bych to k pridani jmen a prijmeni lidi kteri jsou prave ted v restauraci
    Client new_client;
    new_client.client_socket_number = client_socket;
    snprintf(new_client.nickname, sizeof(new_client.nickname),
             "%s", nickname);

    pthread_mutex_lock(&global_clients_mutex);
    global_list_of_clients.push_back(new_client);
    pthread_mutex_unlock(&global_clients_mutex);

    char buffer[256];
    char message[256];

    while (1)
    {
        //ZDE CTE ZPRAVU OD KLIENTA(client_socket)
        int ret = read(client_socket, buffer, sizeof(buffer));

        if (ret <= 0)
            break;

        buffer[ret] = '\0';

        // Kopie zprávy pro kontrolu příkazů lock: a unlock:
        char command[256];
        snprintf(command, sizeof(command), "%s", buffer);
        command[strcspn(command, "\r\n")] = '\0';

        // LOCK / UNLOCK ZPRAV

        // Klient chce pozastavit rozesílání
        if (strcmp(command, "lock:") == 0)
        {
            pthread_mutex_lock(&global_clients_mutex);

            messages_locked = true;
            lock_owner_socket = client_socket;

            char notification[128];
            int notification_length = snprintf(
                notification,
                sizeof(notification),
                "SERVER: Klient %s pozastavil rozesilani zprav.\n",
                nickname
            );

            // Poslani potvrzeni vsem pripojenym klientum
            for (const Client& client : global_list_of_clients)
            {
                write(
                    client.client_socket_number,
                    notification,
                    notification_length
                );
            }

            pthread_mutex_unlock(&global_clients_mutex);

            log_msg(LOG_INFO, "Client %s locked message broadcasting.", nickname);
            continue;
        }

        // Klient chce znovu povolit rozesílání
        if (strcmp(command, "unlock:") == 0)
        {
            pthread_mutex_lock(&global_clients_mutex);

            if (lock_owner_socket == client_socket)
            {
                messages_locked = false;
                lock_owner_socket = -1;

                char notification[128];
                int notification_length = snprintf(
                    notification,
                    sizeof(notification),
                    "SERVER: Klient %s obnovil rozesilani zprav.\n",
                    nickname
                );

                // Poslani potvrzeni vsem pripojenym klientum
                for (const Client& client : global_list_of_clients)
                {
                    write(
                        client.client_socket_number,
                        notification,
                        notification_length
                    );
                }

                pthread_mutex_unlock(&global_clients_mutex);

                log_msg(LOG_INFO, "Client %s unlocked message broadcasting.", nickname);
            }
            else
            {
                pthread_mutex_unlock(&global_clients_mutex);
            }

            continue;
        }

        //NICK A TEXTOVA ZPRAVA
        // Vytvoření zprávy "nick: textova zprava"
        int message_length = snprintf(
            message,
            sizeof(message),
            "%s: %.*s",
            nickname,
            ret,
            buffer
        );

        if (message_length < 0)
            break;

        if (message_length >= (int)sizeof(message))
            message_length = sizeof(message) - 1;

        //STDOUT JE ZDE SERVER(takze co napise klient to se vypise hned na server)
        write(STDOUT_FILENO, message, message_length);

        // Send message to all connected clients - ROZESILA TO ZPRAVU VSEM KLIENTUM OBSAZENYCH V GLOBALNIM LISTU(global_list_of_clients)
        pthread_mutex_lock(&global_clients_mutex);

        // Pokud je rozesílání pozastavené,
        // zprávy může rozesílat pouze klient, který ho pozastavil.
        if (!messages_locked || client_socket == lock_owner_socket)
        {
            for (const Client& client : global_list_of_clients)
            {
                //ZPRAVA SE NEPOSILA ODESILATELI, takze kdo zpravu odeslal tomu jeho vlastni zprava neprijde
                if (client.client_socket_number == client_socket)
                    continue;

                int sent = write(client.client_socket_number, message, message_length);

                if (sent < 0)
                    log_msg(LOG_ERROR, "Unable to send message to client.");
            }
        }


        pthread_mutex_unlock(&global_clients_mutex);

        if (ret >= (int)strlen(STR_CLOSE) &&
            strncasecmp(buffer, STR_CLOSE, strlen(STR_CLOSE)) == 0)
        {
            break;
        }
    }

    // Odstraneni klienta ze seznamu
    pthread_mutex_lock(&global_clients_mutex);

    // Pokud se odpojil klient, který uzamkl rozesílání,
    // musíme rozesílání znovu povolit.
    if (lock_owner_socket == client_socket)
    {
        messages_locked = false;
        lock_owner_socket = -1;
    }

    auto it = std::find_if(
        global_list_of_clients.begin(),
        global_list_of_clients.end(),
        [client_socket](const Client& client)
        {
            return client.client_socket_number == client_socket;
        }
    );

    if (it != global_list_of_clients.end())
        global_list_of_clients.erase(it);

    pthread_mutex_unlock(&global_clients_mutex);

    close(client_socket);
    return NULL;
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
    if ( listen( l_sock_listen, 1 ) < 0 )
    {
        log_msg( LOG_ERROR, "Unable to listen on given port!" );
        close( l_sock_listen );
        exit( EXIT_FAILURE );
    }

    log_msg( LOG_INFO, "Enter 'quit' to quit server." );

    // go! Two loops inside.
    while ( 1 )
    {
        int l_sock_client = -1;

        // list of fd sources
        pollfd l_read_poll[ 2 ];

        l_read_poll[ 0 ].fd = STDIN_FILENO;
        l_read_poll[ 0 ].events = POLLIN;
        l_read_poll[ 1 ].fd = l_sock_listen;
        l_read_poll[ 1 ].events = POLLIN;

        while ( 1 ) // wait for new client
        {
            // select from fds
            int l_poll = poll( l_read_poll, 2, -1 );

            if ( l_poll < 0 )
            {
                log_msg( LOG_ERROR, "Function poll failed!" );
                exit( EXIT_FAILURE );
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

                // Send message to all connected clients - ROZESILA TO ZPRAVU VSEM KLIENTUM OBSAZENYCH V GLOBALNIM LISTU(global_list_of_clients)
                pthread_mutex_lock(&global_clients_mutex);

                for (const Client& client : global_list_of_clients)
                {
                    int sent = write(client.client_socket_number, server_message, server_message_length);

                    if (sent < 0)
                        log_msg(LOG_ERROR, "Unable to send message to clients.");
                }

                pthread_mutex_unlock(&global_clients_mutex);

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


                //ukazatel ktery ukazuje na adresu cisla socketu(l_sock_client)
                int* pointer_to_client_socket_num = new int(l_sock_client);

                //proste promenna do ktere se identifikator ulozi
                pthread_t thread;


                //vytvari se nove vlakno a nastavuje se mu kterou funkci bude vykonavat
                int rc = pthread_create(
                    &thread, //Adresa proměnné, do které se uloží identifikátor nového vlákna. Znak & znamená, že předáváš adresu proměnné.
                    NULL, //Použijí se výchozí atributy vlákna. Pro tuto úlohu nepotřebujeme žádné speciální nastavení.
                    handleClient, //Název funkce, kterou bude nové vlákno vykonávat. Nepíšeme takeCareOfClient(), protože funkci chceme předat, nikoliv ji tady sami zavolat.
                    pointer_to_client_socket_num //Ukazatel na argument, který dostane funkce vlákna. U nás ukazuje na dynamicky uložené číslo klientského socketu.
                );

                if (rc != 0)
                {
                    log_msg(LOG_INFO, "Unable to create client thread.");
                    close(l_sock_client);
                    delete pointer_to_client_socket_num;
                }
                else
                {
                    pthread_detach(thread);
                }

            }

        } // while wait for client


    } // while ( 1 )

    return 0;
} // main

