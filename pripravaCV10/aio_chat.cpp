// ***************************************************************************
//
// Program example for labs in subject Operating Systems
//
// Petr Olivka, Dept. of Computer Science, petr.olivka@vsb.cz, 2026
//
// Example of socket server with asynchronous I/O (aio).
// See 'man aio'.
// Book: Richard Stevens, Advanced Programming in the Unix Environment.
//
// This program is implementation of chat between socket clients using asynchronous I/O.
// Beware: sending data to all connected clients is implemented very ugly! See below.
//
// The mandatory argument of program is port number for listening.
//
// ***************************************************************************

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <csignal>
#include <cerrno>
#include <vector>
#include <unistd.h>
#include <aio.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>

#define STR_CLOSE   "close"
#define STR_QUIT    "quit"

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
    "  Socket server example.\n"
    "\n"
    "  Use: %s [-h] [-d] port_number\n"
    "\n"
    "    -d  debug mode \n"
    "    -h  this help\n"
    "\n"
    ;

// ***************************************************************************

#define BUFLEN  512                     // buffer size
#define MAXFD   1024                    // limit of file descriptors



//STRUKTURA ZPRAVY, DELKA ZPRAVY A DATA O VELIKOSTI BUGLEN COZ JE 512
//Potřebujeme do fronty předat nejen samotný text, ale také jeho délku.
/*
 Například:
- length = 5
- data = "Ahoj"

Díky tomu ví vysílací vlákno, kolik bajtů má odeslat.
 */
struct Message
{
    int length;
    char data[BUFLEN];
};

/*
 Budeme mít dva deskriptory stejné fronty:
- g_mq_send – pro vkládání zpráv.
- g_mq_receive – pro vybírání zpráv.
*/

mqd_t g_mq_send;
mqd_t g_mq_receive;

pthread_mutex_t g_clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// vector of buffers for aiocb structures
std::vector< char[ BUFLEN ] > g_aio_buffers( MAXFD );


// aiocb znamená Asynchronous I/O Control Block.
// Je to struktura, která uchovává informace o jednom asynchronním požadavku.
// vector of aiocb structures
std::vector< aiocb > g_aio_cbs( MAXFD );

//VYSILACI VLAKNO
void *broadcast_thread(void *)
{
    Message msg;

    while (true)
    {
        //mq_receive(fronta, buffer, velikost_bufferu, priorita);
        /*
         g_mq_receive	Deskriptor fronty, ze které čteme (TAKZE TO JE FD NA CTENI)
        (char *)&msg	Adresa proměnné, do které uložíme zprávu
        sizeof(msg)	    Maximální velikost přijímané zprávy
        nullptr	        Prioritu zprávy nepotřebujeme
         */
        // < 0 kdyz nastane chyba preskoc
        if (mq_receive(g_mq_receive,
                       (char *)&msg,
                       sizeof(msg), nullptr) < 0)
        {
            continue;
        }

        pthread_mutex_lock(&g_clients_mutex);

        //Procházení připojených klientů
        for (int inx = STDERR_FILENO + 1; inx < MAXFD; inx++)
        {
            //Kontrola, zda je klient připojený
            if (g_aio_cbs[inx].aio_fildes != -1)
            {
                //Odeslání zprávy klientovi
                //write(deskriptor, data, pocet_bajtu);
                /*
                g_aio_cbs[inx].aio_fildes	Socket klienta
                msg.data	                Text zprávy
                msg.length	                Počet bajtů, které chceme odeslat
                */
                write(g_aio_cbs[inx].aio_fildes,
                      msg.data, msg.length);
            }
        }

        pthread_mutex_unlock(&g_clients_mutex);

    }

    return nullptr;
}


// Callback funkce pro SIGEV_THREAD
void aio_callback(union sigval t_val)
{
    // Získání ukazatele na aiocb
    aiocb *lp_cb = (aiocb *) t_val.sival_ptr;

    // get result of aio
    int l_ret = aio_return( lp_cb );

    if ( l_ret < 0 )
    { // error
        pthread_mutex_lock(&g_clients_mutex);

        log_msg( LOG_ERROR, "aio_return() failed." );
        close( lp_cb->aio_fildes );
        lp_cb->aio_fildes = -1;

        pthread_mutex_unlock(&g_clients_mutex);

        return;
    }

    if ( l_ret == 0 )
    { // socket closed
        pthread_mutex_lock(&g_clients_mutex);

        log_msg( LOG_INFO, "Socket %d closed.", lp_cb->aio_fildes );
        close( lp_cb->aio_fildes );
        lp_cb->aio_fildes = -1;

        pthread_mutex_unlock(&g_clients_mutex);

        return;
    }

    if ( l_ret > 0 )
    {
        // pointer to data
        char *lp_buf = ( char * ) lp_cb->aio_buf;

        if ( lp_cb->aio_fildes == STDIN_FILENO )
        { // data from stdin (keyboard)
            if ( strncmp( lp_buf, STR_QUIT, strlen( STR_QUIT ) ) == 0 )
            {
                log_msg( LOG_INFO, "Request to 'quit'." );
                exit( EXIT_SUCCESS );
            }
        }
        // print received data to stdout (terminal)
        write( STDOUT_FILENO, lp_buf , l_ret );


        // předávat zprávy mezi dvěma vlákny pomocí POSIX fronty zpráv
        // Vytvoření zprávy pro vysílací vlákno
        Message msg;

        msg.length = l_ret;

        // Zkopírování textu
        // memcpy(cil, zdroj, pocet_bajtu);
        /*
            msg.data	Kam budeme kopírovat
            lp_buf	    Odkud budeme kopírovat
            l_ret	    Kolik bajtů zkopírujeme
        */
        // lp_buf ukazuje na buffer, do kterého předtím AIO načetlo data od klienta.
        memcpy(msg.data, lp_buf, l_ret);

        // Vložení zprávy do fronty
        /*
            g_mq_send	        Deskriptor fronty, do které zapisujeme
            (const char *)&msg	Adresa zprávy, kterou chceme vložit
            sizeof(msg)	        Velikost celé struktury v bajtech
            0	                Priorita zprávy
        */
        if (mq_send(g_mq_send, (const char *)&msg, sizeof(msg), 0) < 0)
        {
            if (errno == EAGAIN)
            {
                // Fronta je plná, zprávu zahodíme.
            }
        }




        // register aio request again
        l_ret = aio_read( lp_cb );
        if ( l_ret < 0 )
        {
            log_msg( LOG_ERROR, "aio_read for file descriptor %d failed!", lp_cb->aio_fildes );
            log_msg( LOG_INFO, "File descriptor %d closed!", lp_cb->aio_fildes );

            pthread_mutex_lock(&g_clients_mutex);

            close( lp_cb->aio_fildes );
            lp_cb->aio_fildes = -1;

            pthread_mutex_unlock(&g_clients_mutex);
        }
    }
}


// cleanup on exit, registered in main
void clean()
{
    log_msg( LOG_INFO, "Closing all connections." );

    pthread_mutex_lock(&g_clients_mutex);

    for ( int inx = 1; inx < MAXFD; inx++ )
    {
        if ( g_aio_cbs[ inx ].aio_fildes != -1 )
        {
            close( g_aio_cbs[ inx ].aio_fildes );
            g_aio_cbs[ inx ].aio_fildes = -1;
        }
    }

    pthread_mutex_unlock(&g_clients_mutex);

    mq_close(g_mq_send);
    mq_close(g_mq_receive);

    mq_unlink("/aio_chat");
}


// well known code from socket server
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

    // listening on set port
    if ( listen( l_sock_listen, 1 ) < 0 )
    {
        log_msg( LOG_ERROR, "Unable to listen on given port!" );
        close( l_sock_listen );
        exit( EXIT_FAILURE );
    }

    log_msg( LOG_INFO, "Enter 'quit' to quit server." );



    // AIO CODE

    // INICIALIZACE FRONTY ZPRAV
    struct mq_attr attr;

    attr.mq_flags = 0; // Standardní blokující režim
    attr.mq_maxmsg = 10; // Maximálně 10 zpráv
    attr.mq_msgsize = sizeof(Message); // Maximální velikost jedné zprávy
    attr.mq_curmsgs = 0; // Aktuální počet zpráv při vytvoření

    /*
        Tady vytvoříme frontu a otevřeme ji pro čtení.
        - "/aio_chat" - nazev fronty
        - O_CREAT – vytvoří frontu, pokud ještě neexistuje.
        - O_RDONLY – otevře ji pouze pro čtení.
        - 0600 – vlastník může číst i zapisovat, ostatní nemají přístup.
        - &attr – předáme nastavené atributy.
     */
    g_mq_receive = mq_open(
        "/aio_chat",
        O_CREAT | O_RDONLY,
        0600,
        &attr
    );

    if (g_mq_receive == (mqd_t)-1)
    {
        perror("mq_open receive");
        exit(EXIT_FAILURE);
    }

    /*
       Tady vytvoříme frontu a otevřeme ji pro posilani.
       - "/aio_chat" - nazev fronty
       - O_WRONLY – povolí zápis do fronty.
       - O_NONBLOCK – pokud je fronta plná, mq_send() nebude čekat.
    */
    g_mq_send = mq_open(
        "/aio_chat",
        O_WRONLY | O_NONBLOCK
    );

    if (g_mq_send == (mqd_t)-1)
    {
        perror("mq_open send");
        exit(EXIT_FAILURE);
    }

    // Create broadcast thread
    pthread_t l_thread;

    int l_ret = pthread_create(
        &l_thread,
        nullptr,
        broadcast_thread,
        nullptr
    );

    if (l_ret != 0)
    {
        fprintf(stderr, "pthread_create failed: %s\n", strerror(l_ret));
        exit(EXIT_FAILURE);
    }

    pthread_detach(l_thread);

    // aio code
    // cleanup registration
    atexit( clean );


    // initialization of all aiocb structures without file descriptor
    // notification method: signal
    for ( int inx = 0; inx < MAXFD; inx++ )
    {

        // Každý klient má svůj vlastní aiocb a vlastní buffer.
        aiocb &cb = g_aio_cbs[ inx ];

        // Souborový deskriptor, ze kterého čteme
        cb.aio_fildes = -1;
        cb.aio_offset = 0;

        // Buffer, do kterého se načtou data
        cb.aio_buf = g_aio_buffers[ inx ];

        // Maximální počet načítaných bajtů BUFLEN je 512
        cb.aio_nbytes = BUFLEN;
        cb.aio_reqprio = 0;
        cb.aio_lio_opcode = LIO_NOP;

        //Způsob oznámení dokončení operace
        /*
            SIGEV_THREAD – po dokončení asynchronního čtení se zavolá funkce v novém vlákně.
            sigev_notify_function – určuje, kterou funkci má systém zavolat.
            sigev_notify_attributes – nastavení atributů nového vlákna; nullptr znamená výchozí nastavení.
            sigev_value.sival_ptr – předáváme ukazatel na konkrétní aiocb, abychom věděli, kterého klienta se dokončené čtení týká.
         */
        cb.aio_sigevent.sigev_notify = SIGEV_THREAD;
        cb.aio_sigevent.sigev_notify_function = aio_callback;
        cb.aio_sigevent.sigev_notify_attributes = nullptr;
        cb.aio_sigevent.sigev_value.sival_ptr = &cb;
    }

    // register aio request for stdin
    g_aio_cbs[ STDIN_FILENO ].aio_fildes = STDIN_FILENO;

    //žádáme o asynchronní čtení z klávesnice.
    aio_read( &g_aio_cbs[ STDIN_FILENO ] );

    // go!
    while ( 1 )
    {
        sockaddr_in l_rsa;
        int l_rsa_size = sizeof( l_rsa );
        // new connection
        int l_sock_client = accept( l_sock_listen, ( sockaddr * ) &l_rsa, ( socklen_t * ) &l_rsa_size );
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

        // register aio for new client
        // se začne asynchronně číst ze socketu klienta.

        pthread_mutex_lock(&g_clients_mutex);

        g_aio_cbs[ l_sock_client ].aio_fildes = l_sock_client;

        pthread_mutex_unlock(&g_clients_mutex);

        aio_read( &g_aio_cbs[ l_sock_client ] );

    } // while ( 1 )

    return 0;
} // main
