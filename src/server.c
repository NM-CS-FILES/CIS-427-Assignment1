#include "shared.h"

#define USERS_CREATE_QUERY  "CREATE TABLE IF NOT EXISTS Users(ID INTEGER PRIMARY KEY AUTOINCREMENT,first_name TEXT,last_name TEXT,user_name TEXT NOT NULL,password TEXT,usd_balance DOUBLE NOT NULL);"
#define USERS_EMPTY_QUERY   "SELECT count(*) FROM (select 0 from Users limit 1)"

#define STOCKS_CREATE_QUERY "CREATE TABLE IF NOT EXISTS Stocks(ID INTEGER PRIMARY KEY AUTOINCREMENT,stock_symbol VARCHAR(4) NOT NULL,stock_name VARCHAR(20) NOT NULL,stock_balance DOUBLE,user_id INTEGER,FOREIGN KEY (user_id) REFERENCES Users (ID));"
#define STOCKS_LIST_QUERY   ""

//
//

typedef struct _clnt {
    int id;
    int sock_fd;
    sockaddr_in addr;
    pthread_t th;
    bool th_flag;
    struct _clnt* next;
    struct _clnt* prev;
} client_t;

typedef struct _strtok {
    char* string;
    struct _strtok* next;
} strtoken;

typedef bool(*command_callback)(
    client_t*,     // client
    const char*, // input
    char*,       // output
    size_t       // output len
);

//
//

strtoken* str_tokenize(
    const char* str,
    const char delim
) {
    if (str == NULL || !*str) {
        return NULL;
    }

    strtoken* head = NULL;
    strtoken* tail = NULL;

    const char* back = str;
    const char* front = str;

    //while (*back && )

    //return head;

    return NULL;
}

//
//

sqlite3* DATABASE;
client_t* CLIENT_LIST;
pthread_mutex_t STDOUT_LOCK;
pthread_mutex_t DATABASE_LOCK;
bool BROADCAST_FLAG;
pthread_t BROADCAST_TH;
fd_set CLIENT_FD_SET;

//
//

void vprintf_locked(
    const char* fmt,
    va_list vargs
) {
    pthread_mutex_lock(&STDOUT_LOCK);
    vprintf(fmt, vargs);
    pthread_mutex_unlock(&STDOUT_LOCK);
}

void printf_locked(
    const char* fmt,
    ...
) {
    va_list vargs;
    va_start(vargs, fmt);
    vprintf_locked(fmt, vargs);
    va_end(vargs);
}

void vlog_ns(
    const char* ns,
    const char* fmt,
    va_list vargs
) {
    char* vmsg = vformat(fmt, vargs);
    char* totmsg = format("[%s]: %s\n", ns, vmsg);
    printf_locked(totmsg);
    free(vmsg);
    free(totmsg);
}

void log_ns(
    const char* ns,
    const char* fmt,
    ...
) {
    va_list vargs;
    va_start(vargs, fmt);
    vlog_ns(ns, fmt, vargs);
    va_end(vargs);
}

#define log_inet(_addr, _fmt, ...) log_ns(inet_ntoa((_addr).sin_addr), (_fmt) __VA_OPT__(,) __VA_ARGS__)

//
//

bool buy_command(
    client_t* pclient,
    const char* input,
    char* output,
    size_t output_len
) {
    return true;
}

bool sell_command(
    client_t* pclient,
    const char* input,
    char* output,
    size_t output_len
) {
    return true;
}

bool list_command(
    client_t* pclient,
    const char* input,
    char* output,
    size_t output_len
) {
    return true;
}

bool balance_command(
    client_t* pclient,
    const char* input,
    char* output,
    size_t output_len
) {
    return true;
}

bool shutdown_command(
    client_t* pclient,
    const char* input,
    char* output,
    size_t output_len
) {
    return true;
}

bool quit_command(
    client_t* pclient,
    const char* input,
    char* output,
    size_t output_len
) {
    return true;
}

//
//

void* broadcast_task(
    void* args
) {
    log_ns("Broadcast", "Started On Port %hu", BROADCAST_PORT);

    int broadcast_fd = -1; // socket object

    // create datagram socket
    if ((broadcast_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        log_ns("Broadcast", "Failed To Create Broadcast Socket");
        goto broadcast_out;
    }

    // enable broadcasting
    int opt_true = 1;
    if (setsockopt(broadcast_fd, SOL_SOCKET, SO_BROADCAST, &opt_true, sizeof(opt_true)) < 0) {
        log_ns("Broadcast", "Failed To Broadcast On Socket");
        goto broadcast_out;
    }

    sockaddr_in broadcast_addr = { 0 };
    broadcast_addr.sin_family = AF_INET;               // I fucking hate ipv6
    broadcast_addr.sin_port = htons(BROADCAST_PORT);   // network order
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST; // byte-palindrome, order?

    bool* pflag = (bool*)args;
    clock_t prev = 0;
    clock_t now = 0;

    while (*pflag) {
        now = clock();

        if ((now - prev) / CLOCKS_PER_SEC >= 3) {
            // Waste bandwidth here
            sendto(broadcast_fd, MAGIC_TEXT, strlen(MAGIC_TEXT), 0, 
                (sockaddr*)&broadcast_addr, sizeof(sockaddr));

            log_ns("Broadcast", "Heartbeat Sent");
            prev = now;
        }
    }

broadcast_out:

    close(broadcast_fd);
    *pflag = false;

    return NULL;
}

void* client_task(
    void* args
) {
    client_t* client = (client_t*)args;
    char input_buffer[1024] = { 0 };
    char output_buffer[1024] = { 0 };

    log_inet(client->addr, "Started Client Thread");

    const struct {
        const char* prefix;
        command_callback callback;
    } commands[] = { 
        { "buy",      buy_command      },  
        { "sell",     sell_command     },  
        { "list",     list_command     },  
        { "balance",  balance_command  },  
        { "shutdown", shutdown_command },  
        { "quit",     quit_command     }
    };

    while (client->th_flag) {

        int len = recv(client->sock_fd, input_buffer, sizeof(input_buffer), 0);

        if (len <= 0) {
            if  (errno == EWOULDBLOCK) {
                continue;
            } else {
                log_inet(client->addr, "Client Closed");
                break;
            }
        }

        // ensure a nulled string
        input_buffer[len] = '\0';

        size_t i = 0;
        while (i != LENGTHOF(commands)) {
            if (strincmp(commands[i].prefix, input_buffer, 
                strlen(commands[i].prefix)) == 0) {
                    break;
            }

            i++;
        }

        if (i == LENGTHOF(commands)) {
            log_inet(client->addr, "Unkown Command: %s : %d", input_buffer, len);
            continue;
        }

        log_inet(client->addr, "Calling Command: %s", commands[i].prefix);

        //commands[i].callback(client, strtoken(input_buffer, ' '), output_buffer, sizeof(output_buffer));
    }

    return NULL;
}

//
//

void initialize() {
    // setup mutex locks

    if (pthread_mutex_init(&DATABASE_LOCK, NULL) != 0) {
        printf("TOTAL FAIL\n");
        exit(-1);
    }

    if (pthread_mutex_init(&STDOUT_LOCK, NULL) != 0) {
        printf("TOTAL FAIL 2\n");
        exit(-2);
    }

    // setup database
    
    sqlite3_open("system.db", &DATABASE);
    char* error_msg = NULL;

    if (DATABASE == NULL) {
        // todo fatal
    }

    if (sqlite3_exec(DATABASE, USERS_CREATE_QUERY, NULL, 0, &error_msg) != SQLITE_OK) {
        sqlite3_free(error_msg);
    }

    if (sqlite3_exec(DATABASE, STOCKS_CREATE_QUERY, NULL, 0, &error_msg) != SQLITE_OK) {
        sqlite3_free(error_msg);
    }

    // start broadcasting

    BROADCAST_FLAG = true;

    pthread_create(&BROADCAST_TH, NULL, broadcast_task, &BROADCAST_FLAG);
}

void deinitialize() {
    pthread_mutex_destroy(&STDOUT_LOCK);
    pthread_mutex_destroy(&DATABASE_LOCK);
}


int a__main() {
    initialize();

    //
    //

    fd_set clients;
    int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (server_fd < 0) {
        printf_locked("Failed To Create Server Socket\n");        
    }

    sockaddr_in server_addr = { 0 };
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(sockaddr)) < 0) {
        fatal_error("Failed To Bind");
    }

    log_ns("Server", "Bound To Port %hu", SERVER_PORT);

    if (listen(server_fd, SOMAXCONN) < 0) {
        fatal_error("Failed To Listen");
    }

    log_ns("Server", "Listening For Clients");

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(sockaddr);

        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_addr_len);

        log_inet(client_addr, "Accepted Client");

        client_t* client = (client_t*)calloc(1, sizeof(client));

        client->id = 0;
        client->sock_fd = client_fd;
        client->addr = client_addr;
        client->th_flag = true;

        pthread_create(&client->th, NULL, client_task, client);
    }

    deinitialize();

    return 0;
}


int main() {
    initialize();

    int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (server_fd < 0) {
        printf_locked("Failed To Create Server Socket\n");        
    }

    sockaddr_in server_addr = { 0 };
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(sockaddr)) < 0) {
        fatal_error("Failed To Bind");
    }

    log_ns("Server", "Bound To Port %hu", SERVER_PORT);

    if (listen(server_fd, SOMAXCONN) < 0) {
        fatal_error("Failed To Listen");
    }

    log_ns("Server", "Listening For Clients");

    fd_set working_fd_set;

    
}