// ***************************************************************************
//
// l_sock_listen slouží k přijímání nových klientů.
//
// l_sock_client slouží ke komunikaci s již připojeným klientem.
//
// V tomto programu jsou sledovány dva deskriptory:
// l_read_poll[0].fd = STDIN_FILENO;
// l_read_poll[1].fd = l_sock_listen;
// To znamená:
// 1. STDIN_FILENO — někdo něco zadal do terminálu serveru.
// 2. l_sock_listen — chce se připojit nový klient.
//
// Přijetí klienta
// Když se chce někdo připojit, server provede:
// l_sock_client = accept(
//    l_sock_listen,
//    (sockaddr *)&l_rsa,
//    (socklen_t *)&l_rsa_size
// );
//
//
//
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
#include <sys/wait.h>
#include <sys/types.h>
#include <strings.h>
#include <sys/types.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#define STR_CLOSE   "close"
#define STR_QUIT    "quit"

// ***************************************************************************
//                      SHARED DATA

#define N 10

struct SharedData {
//    board[RADKU - ROWS][SLOUPCU - COLS]
    char board[9][9];
    pid_t pids[N];
    sem_t sems[N];

    int current_player;
};


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

// INICIALIZACE HRY
void init_game(SharedData *game)
{
    // Inicializace hracího pole
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            game->board[i][j] = '.';
        }
    }

    // Inicializace PID
    for (int i = 0; i < N; i++) {
        game->pids[i] = 0;
    }

    // Inicializace semaforů
    for (int i = 0; i < N; i++) {
        int var;

        if (i == 0)
        {
            var = 1;
        }
        else
        {
            var = 0;
        }

        sem_init(&game->sems[i], 1, var);
    }

    game->current_player = 0;
}

int find_next_player(SharedData *game, int current_player)
{
    for (int i = 1; i <= N; i++)
    {
        int next = (current_player + i) % N;

        if (game->pids[next] > 0)
        {
            return next;
        }
    }

    return -1;
}

void pass_turn(SharedData *game, int current_player)
{
    int next_player = find_next_player(game, current_player);

    if (next_player != -1)
    {
        game->current_player = next_player;
        // UP = sem_post – přidá jednu jednotku semaforu a případně probudí čekající proces.
        sem_post(&game->sems[next_player]);
    }
    else
    {
        game->current_player = -1;
    }
}

void send_board(int socket, SharedData *game)
{
    char buffer[256];
    int offset = 0;

    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                       "  A B C D E F G H I\n");

    for (int i = 0; i < 9; i++)
    {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                           "%d ", i + 1);

        for (int j = 0; j < 9; j++)
        {
            offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                               "%c ", game->board[i][j]);
        }

        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\n");
    }

    write(socket, buffer, offset);
}

int process_move(SharedData *game, char *command)
{
    char column;
    int row;
    char letter;
    char extra;

    // Nacteni prikazu
    if (sscanf(command, "%c-%d-%c%c",
               &column, &row, &letter, &extra) != 3)
    {
        printf("Neplatny format tahu!\n");
        return 0;
    }

    // Prevod malych pismen na velka
    if (column >= 'a' && column <= 'i')
        column -= 32;

    if (letter >= 'a' && letter <= 'z')
        letter -= 32;

    // Kontrola souradnic
    if (column < 'A' || column > 'I' || row < 1 || row > 9)
    {
        printf("Neplatna pozice!\n");
        return 0;
    }

    // Prevod na indexy pole
    int col_index = column - 'A';
    int row_index = row - 1;

    // Kontrola pismena
    if (letter < 'A' || letter > 'Z')
    {
        printf("Neplatne pismeno!\n");
        return 0;
    }

    // Kontrola, zda je policko volne
    if (game->board[row_index][col_index] != '.')
    {
        printf("Policko je obsazene!\n");
        return 0;
    }

    // Zapis pismena
    game->board[row_index][col_index] = letter;

    printf("Tah: %c-%d-%c\n", column, row, letter);

    return 1;
}

void remove_player(SharedData *game, int player_index)
{
    printf("Hrac na pozici %d se odpojil.\n", player_index);

    // Uvolneni mista
    game->pids[player_index] = 0;

    // Pokud odchazi aktualni hrac, predame tah dal
    if (game->current_player == player_index)
    {
        pass_turn(game, player_index);
    }
}

void notify_parent(int notify_fd) {
    char signal = 'U';
    write(notify_fd, &signal, 1);
}

void broadcast_game(SharedData *game, int client_sockets[N])
{
    char message[128];

    for (int i = 0; i < N; i++)
    {
        if (client_sockets[i] == -1)
            continue;

        if (game->current_player == i)
        {
            snprintf(message, sizeof(message),
                     "\nJsi na tahu!\n");
        }
        else
        {
            snprintf(message, sizeof(message),
                     "\nNa tahu je hrac na pozici %d.\n",
                     game->current_player);
        }

        write(client_sockets[i], message, strlen(message));
        send_board(client_sockets[i], game);
    }
}

//komunikace od clienta do kuchyne
void communication_from_client_to_server(int l_sock_client, SharedData *game, int player_index, int notify_fd)
{
    // Zaregistrujeme PID hrace
    game->pids[player_index] = getpid();
    notify_parent(notify_fd);

    printf("Hrac obsadil misto %d, PID: %d\n",
           player_index, getpid());

    pollfd l_read_poll;
    l_read_poll.fd = l_sock_client;
    l_read_poll.events = POLLIN;

    char buffer[256];
    char remaining[1024] = "";

    bool disconnected = false;

    // HLAVNI CYKLUS - CEKANI NA TAH
    while (!disconnected)
    {
        /*
        // 1. Cekani na svuj tah
        // DOWN = sem_wait
        if (sem_wait(&game->sems[player_index]) == -1)
        {
            log_msg(LOG_ERROR, "sem_wait failed!");
            break;
        }
        */

        // 1. Cekani na svuj tah a kontrola odpojeni klienta
        bool turn_received = false;

        while (!disconnected && !turn_received)
        {
            // Zkusime ziskat semafor bez blokovani
            if (sem_trywait(&game->sems[player_index]) == 0)
            {
                turn_received = true;
                break;
            }

            // EAGAIN znamena, ze semafor zatim nema token
            if (errno != EAGAIN && errno != EINTR)
            {
                log_msg(LOG_ERROR, "sem_trywait failed!");
                disconnected = true;
                break;
            }

            // Pokud jeste nemame tah, kratce zkontrolujeme socket
            pollfd disconnect_poll;
            disconnect_poll.fd = l_sock_client;
            disconnect_poll.events = POLLRDHUP;
            disconnect_poll.revents = 0;

            int result = poll(&disconnect_poll, 1, 200);

            if (result < 0 && errno != EINTR)
            {
                log_msg(LOG_ERROR, "poll failed while waiting for turn!");
                disconnected = true;
                break;
            }

            if (result > 0 &&
                (disconnect_poll.revents & (POLLRDHUP | POLLHUP | POLLERR | POLLNVAL)))
            {
                log_msg(LOG_INFO, "Client closed connection.");
                disconnected = true;
                break;
            }
        }

        if (disconnected)
            break;

        /*
        // 2. Hrac dostal tah - posleme mu aktualni pole
        write(l_sock_client, "Jsi na tahu!\n", 13);
        send_board(l_sock_client, game);
        */

        // 3. Cekani na prikaz od klienta
        bool turn_finished = false;

        while (!turn_finished && !disconnected)
        {
            int l_poll = poll(&l_read_poll, 1, -1);

            if (l_poll < 0)
            {
                log_msg(LOG_ERROR, "Function poll failed!");
                disconnected = true;
                break;
            }

            if (!(l_read_poll.revents & POLLIN))
            {
                if (l_read_poll.revents & (POLLHUP | POLLERR | POLLNVAL))
                    disconnected = true;

                continue;
            }

            // 4. Cteni dat od klienta
            int ret = read(l_sock_client, buffer, sizeof(buffer) - 1);

            if (ret < 0)
            {
                log_msg(LOG_ERROR, "Unable to read from client.");
                disconnected = true;
                break;
            }

            if (ret == 0)
            {
                log_msg(LOG_INFO, "Client closed connection.");
                disconnected = true;
                break;
            }

            buffer[ret] = '\0';

            // 5. Kontrola kapacity bufferu
            size_t remaining_len = strlen(remaining);

            if ((size_t)ret >= sizeof(remaining) - remaining_len)
            {
                log_msg(LOG_ERROR, "Input buffer is full.");
                disconnected = true;
                break;
            }

            strcat(remaining, buffer);

            // 6. Rozdeleni dat na jednotlive radky
            char *start = remaining;

            while (1)
            {
                char *end = strchr(start, '\n');

                if (end == nullptr)
                    break;

                *end = '\0';

                // Odstraneni CR
                int len = strlen(start);

                if (len > 0 && start[len - 1] == '\r')
                    start[len - 1] = '\0';

                // Preskoceni prazdneho radku
                if (strlen(start) == 0)
                {
                    start = end + 1;
                    continue;
                }

                // Kontrola CLOSE
                if (strncasecmp(start, STR_CLOSE, strlen(STR_CLOSE)) == 0)
                {
                    log_msg(LOG_INFO, "Client requested close.");
                    disconnected = true;
                    break;
                }

                // 7. Zpracovani tahu
                if (process_move(game, start))
                {
                    // Predani tahu dalsimu hraci
                    pass_turn(game, player_index);
                    notify_parent(notify_fd);
                    turn_finished = true;
                    start = end + 1;
                    break;
                }

                // Neplatny tah - hrac zustava na tahu
                start = end + 1;
            }

            // Zachovani zbyvajicich dat
            memmove(remaining, start, strlen(start) + 1);
        }
    }

    // 8. Uvolneni mista po odpojeni klienta
    remove_player(game, player_index);
    notify_parent(notify_fd);
}




int main( int t_argc, char **t_argv )
{

    int l_port = 0;

    // parsing arguments
    // proste a jednoduse prochazi to argumenty pri spusteni serveru ./socket_srv 12345(<- toto je ten l_port protoze ten argument nezacina '-')
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
            if ( l_port == 0 )
            {
                l_port = atoi( t_argv[ inx ] );
            }
        }
    }

    //PROSTE ERROR S PORTEM
    if ( l_port <= 0 )
    {
        log_msg( LOG_INFO, "Bad or missing port number %d!", l_port );
        printf( g_help, t_argv[ 0 ] );
        exit( EXIT_FAILURE );
    }

    //KDYZ JE PORT SPRAVNY TOTO VYPISE TU ZPRAVU
    //PORT MUSI BYT VETSI JAK 0
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
    if ( listen( l_sock_listen, 10 ) < 0 )
    {
        log_msg( LOG_ERROR, "Unable to listen on given port!" );
        close( l_sock_listen );
        exit( EXIT_FAILURE );
    }

    log_msg( LOG_INFO, "Enter 'quit' to quit server." );

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///     CREATING SHARED MEMORY AND INITIALIZING SCRABBLE GAME
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    // VYTVORENI SDILENE PAMETI
    int shared_memory_fd = shm_open( "/osyscrabble", O_RDWR | O_CREAT, 0600 );

    if ( shared_memory_fd < 0 ) {
        perror( "shm_open(shared_memory_fd)" );
        exit( EXIT_FAILURE );
    }

    // NASTAVENI VELIKOSTI
    if (ftruncate(shared_memory_fd, sizeof(SharedData)) < 0) {
        perror("ftruncate(shared_memory_fd)");
        exit( EXIT_FAILURE );
    }

    // NAMAPOVANI PAMETI
    SharedData *shared_memory = (SharedData *) mmap(
        nullptr,
        sizeof(SharedData),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        shared_memory_fd,
        0
    );

    if (shared_memory == MAP_FAILED) {
        perror( "mmap(shared_memory)" );
        exit( EXIT_FAILURE );
    }

    // INICIALIZACE HRY
    init_game( shared_memory );

    // Sockety jednotlivych klientu
    int client_sockets[N];

    for (int i = 0; i < N; i++)
    {
        client_sockets[i] = -1;
    }

    int notify_pipe_fd[2];

    if (pipe(notify_pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // go! Two loops inside.
    while ( 1 )
    {
        int l_sock_client = -1;

        // list of fd sources
        struct pollfd l_read_poll[3];

        l_read_poll[0].fd = STDIN_FILENO;
        l_read_poll[0].events = POLLIN;

        l_read_poll[1].fd = l_sock_listen;
        l_read_poll[1].events = POLLIN;

        l_read_poll[2].fd = notify_pipe_fd[0];
        l_read_poll[2].events = POLLIN;

        while ( 1 ) // wait for new client
        {
            // Uklizeni ukoncenych potomku
            while (waitpid(-1, nullptr, WNOHANG) > 0)
            {
            }

            // Kontrola odpojenych klientu
            for (int i = 0; i < N; i++)
            {
                if (shared_memory->pids[i] == 0 &&
                    client_sockets[i] != -1)
                {
                    close(client_sockets[i]);
                    client_sockets[i] = -1;
                }
            }

            // select from fds
            //ZDE TEN POLL CEKA
            int l_poll = poll( l_read_poll, 3, 1000 );

            if ( l_poll < 0 )
            {
                log_msg( LOG_ERROR, "Function poll failed!" );
                exit( EXIT_FAILURE );
            }

            // Pokud poll vyprsel, vratime se na zacatek smycky
            if (l_poll == 0)
            {
                continue;
            }

            if (l_read_poll[2].revents & POLLIN) {
                char signals[64];

                ssize_t bytes = read(notify_pipe_fd[0],
                                     signals,
                                     sizeof(signals));

                if (bytes > 0) {

                    // Kontrola odpojenych klientu
                    for (int i = 0; i < N; i++)
                    {
                        if (shared_memory->pids[i] == 0 &&
                            client_sockets[i] != -1)
                        {
                            close(client_sockets[i]);
                            client_sockets[i] = -1;
                        }
                    }

                    broadcast_game(shared_memory, client_sockets);
                }
            }

            if ( l_read_poll[ 0 ].revents & POLLIN )
            { // data on stdin
                char buf[ 128 ];


                int l_len = read( STDIN_FILENO, buf, sizeof( buf) );
                if ( l_len == 0 )
                {
                    log_msg( LOG_DEBUG, "Stdin closed." );
                    exit( EXIT_SUCCESS );
                }
                if ( l_len < 0 )
                {
                    log_msg( LOG_DEBUG, "Unable to read from stdin!" );
                    exit( EXIT_FAILURE );
                }

                log_msg( LOG_DEBUG, "Read %d bytes from stdin", l_len );
                // request to quit?
                if ( !strncmp( buf, STR_QUIT, strlen( STR_QUIT ) ) )
                {
                    log_msg( LOG_INFO, "Request to 'quit' entered.");
                    close( l_sock_listen );
                    exit( EXIT_SUCCESS );
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
                ///     VOLNE MISTO A OBSAZOVANI MISTA
                ///////////////////////////////////////////////////////////////////////////////////////////////////

                int player_index = -1;
                bool game_empty = true;

                // Kontrola, zda je hra prazdna
                for (int i = 0; i < N; i++)
                {
                    if (shared_memory->pids[i] != 0)
                    {
                        game_empty = false;
                        break;
                    }
                }

                // Hledani volneho mista
                for (int i = 0; i < N; i++)
                {
                    if (shared_memory->pids[i] == 0)
                    {
                        player_index = i;

                        // Rezervace mista
                        shared_memory->pids[i] = -1;

                        break;
                    }
                }

                if ( player_index == -1 ) {
                    printf("Hra je plna!\n");
                    close( l_sock_client );
                    continue;
                }

                // Ulozeni socketu klienta
                client_sockets[player_index] = l_sock_client;

                // Pokud se pripojuje prvni hrac do prazdne hry,
                // musi dostat moznost hrat jako prvni.
                if (game_empty)
                {
                    int value;

                    sem_getvalue(&shared_memory->sems[player_index], &value);

                    if (value == 0)
                    {
                        sem_post(&shared_memory->sems[player_index]);
                    }

                    shared_memory->current_player = player_index;
                }

                ///////////////////////////////////////////////////////////////////////////////////////////////////
                ///     CREATING PROCESS
                ///////////////////////////////////////////////////////////////////////////////////////////////////

                //CREATION OF FIRST CHILD -> messages from client to server (cisnik)
                pid_t client_child = fork();

                if (client_child < 0)
                {
                    //ERROR
                    perror("Unable to create new child process");

                    shared_memory->pids[player_index] = 0;
                    client_sockets[player_index] = -1;

                    close(l_sock_client);
                    continue;
                }

                if (client_child != 0)
                {
                    // PARENT1 (kuchyn)
                    // kuchyn nepotrebuje komunikovat s clientem protoze od toho ma cisnika
                    //sleep(2);

                    printf("PID potomka: %d\n", client_child);
                    //printf("Rodic vidi na board[0][0]: %c\n", shared_memory->board[0][0]);

                }
                else
                {
                    // CHILD1 (cisnik)
                    //cisnik nepotrebuje komunikovat s kuchyni
                    close(l_sock_listen);
                    close(notify_pipe_fd[0]);
                    // Potomek nepotrebuje sockety ostatnich klientu
                    for (int i = 0; i < N; i++)
                    {
                        if (i != player_index && client_sockets[i] != -1)
                        {
                            close(client_sockets[i]);
                        }
                    }

                    //ZDE JE FUNKCE KDE CISNIK PREDAVA ZPRAVY OD KLIENTA DO KUCHYNE A ZASE ZPET
                    communication_from_client_to_server(l_sock_client, shared_memory, player_index, notify_pipe_fd[1]);

                    //cisnik po uspesnem predani informaci do kuchyne uz nepotrebuje komunikovat s clientem
                    close(l_sock_client);
                    close(notify_pipe_fd[1]);
                    exit(EXIT_SUCCESS);
                }
                // PARENT
                //KUCHYNE NEPOTREBUJE KOMUNIKOVAT S KLIENTEM OD TOHO JE ZDE CISNIK(child1)
                //close(l_sock_client);

                break;
            }

        } // while wait for client
    } // while ( 1 )

    return 0;
} // main