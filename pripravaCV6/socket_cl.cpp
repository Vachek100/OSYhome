// ***************************************************************************
//
// Program example for subject Operating Systems
//
// Petr Olivka, Dept. of Computer Science, petr.olivka@vsb.cz, 2026
//
// Example of socket server/client.
//
// This program is example of socket client.
// The mandatory arguments of program is IP address or name of server and
// a port number.
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
#include <netdb.h>
#include <ctime>
#include <fcntl.h>

#define STR_CLOSE               "close"

// ***************************************************************************
// log messages

#define LOG_ERROR               0       // errors
#define LOG_INFO                1       // information and notifications
#define LOG_DEBUG               2       // debug messages

// debug flag
int g_debug = LOG_INFO;

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
    "  Socket client example.\n"
    "\n"
    "  Use: %s [-h] [-d] ip_or_name port_number\n"
    "\n"
    "    -d  debug mode \n"
    "    -h  this help\n"
    "\n"
    ;

// ***************************************************************************

int main( int t_argc, char **t_argv )
{
    int l_port = 0;
    char *l_host = nullptr;
    char *resolution = nullptr;


    // parsing arguments
    // Prochazi argumenty zadane pri spusteni klienta.
    // Napr. ./socket_cl 127.0.0.1 12345
    // Prvni argument bez pomlcky je host a druhy je port.
    // inx = 1 je proste i = 1
    // t_argc je to same jako argc
    // t_argv je to same jako argv
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
            if ( l_host == nullptr  )
            {
                l_host = t_argv[ inx ];
            }
            else if ( l_port == 0 )
            {
                l_port = atoi( t_argv[ inx ] );
            }
            else if (resolution == nullptr)
            {
                resolution = t_argv[inx];
            }
        }


    }

    if ( l_host == nullptr || l_port <= 0 )
    {
        log_msg( LOG_INFO, "Host or port is missing!" );
        printf( g_help, t_argv[ 0 ] );
        exit( EXIT_FAILURE );
    }

    log_msg( LOG_INFO, "Connection to '%s':%d.", l_host, l_port );

    addrinfo l_ai_req, *l_ai_ans;
    bzero( &l_ai_req, sizeof( l_ai_req ) );
    l_ai_req.ai_family = AF_INET;
    l_ai_req.ai_socktype = SOCK_STREAM;

    int l_get_ai = getaddrinfo( l_host, nullptr, &l_ai_req, &l_ai_ans );
    if ( l_get_ai )
    {
        log_msg( LOG_ERROR, "Unknown host name!" );
        exit( EXIT_FAILURE );
    }

    // get the first address
    sockaddr_in l_cl_addr =  *( sockaddr_in * ) l_ai_ans->ai_addr;
    l_cl_addr.sin_port = htons( l_port );
    freeaddrinfo( l_ai_ans );

    // socket creation
    int l_sock_server = socket( AF_INET, SOCK_STREAM, 0 );
    if ( l_sock_server < 0 )
    {
        log_msg( LOG_ERROR, "Unable to create socket.");
        exit( EXIT_FAILURE );
    }

    // connect to server
    if ( connect( l_sock_server, ( sockaddr * ) &l_cl_addr, sizeof( l_cl_addr ) ) < 0 )
    {
        log_msg( LOG_ERROR, "Unable to connect server." );
        exit( EXIT_FAILURE );
    }

    uint l_lsa = sizeof( l_cl_addr );
    // my IP
    getsockname( l_sock_server, ( sockaddr * ) &l_cl_addr, &l_lsa );
    log_msg( LOG_INFO, "My IP: '%s'  port: %d",
             inet_ntoa( l_cl_addr.sin_addr ), ntohs( l_cl_addr.sin_port ) );
    // server IP
    getpeername( l_sock_server, ( sockaddr * ) &l_cl_addr, &l_lsa );
    log_msg( LOG_INFO, "Server IP: '%s'  port: %d",
             inet_ntoa( l_cl_addr.sin_addr ), ntohs( l_cl_addr.sin_port ) );

    log_msg( LOG_INFO, "Enter 'close' to close application." );

    char l_request[128];

    snprintf(l_request, sizeof(l_request), "%s\n", resolution);

    if (write(l_sock_server, l_request, strlen(l_request)) < 0)
    {
        perror("write resolution");
        close(l_sock_server);
        return EXIT_FAILURE;
    }
    
    int image_fd = open("image.img", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (image_fd < 0) {
        perror("open image.jpg");
        close(l_sock_server);
        return EXIT_FAILURE;
    }

    // go!
    while ( 1 )
    {

        char l_buf[ 128 ];


        ///////////////////////////////////////////////////////////////////////////////////////////////////
        ///     MY PLAYGROUND
        ///////////////////////////////////////////////////////////////////////////////////////////////////


        // data from server?


            // read data from server
            int l_serverlen = read( l_sock_server, l_buf, sizeof( l_buf ) );
            if ( l_serverlen == 0 )
            {
                log_msg( LOG_DEBUG, "Server closed socket." );
                break;
            }
            else if ( l_serverlen < 0 )
            {
                log_msg( LOG_ERROR, "Unable to read data from server." );
                break;
            }
            else
            {
                log_msg( LOG_DEBUG, "Read %d bytes from server.", l_serverlen );
            }

            ///ZDE MOHU ZPRACOVAVAT DATA ZE SERVERU

            // Ulozeni prijatych bajtu do obrazku
            int written = 0;

            while (written < l_serverlen)
            {
                int ret = write(
                    image_fd,
                    l_buf + written,
                    l_serverlen - written
                );

                if (ret <= 0)
                {
                    perror("write image");
                    break;
                }

                written += ret;
            }

            printf("Prijato %d bajtu obrazku.\n", l_serverlen);



            // request to close?
            if ( strncasecmp( l_buf, STR_CLOSE, strlen( STR_CLOSE ) ) == 0 )
            {
                log_msg( LOG_INFO, "Connection will be closed..." );
                break;
            }

    }

    close(image_fd);

    close(l_sock_server);
    // close socket
    close( l_sock_server );

    char *args[] = {
        (char *)"display",
        (char *)"image.img",
        nullptr
    };

    execvp("display", args);
    perror("execvp display");
    exit(EXIT_FAILURE);

    return 0;
} // main
