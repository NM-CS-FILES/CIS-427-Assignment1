#include "shared.h"

#define USERS_CREATE_QUERY  "CREATE TABLE IF NOT EXISTS Users(ID INTEGER PRIMARY KEY AUTOINCREMENT,first_name TEXT,last_name TEXT,user_name TEXT NOT NULL,password TEXT,usd_balance DOUBLE NOT NULL);"
#define USERS_EMPTY_QUERY   "SELECT count(*) FROM (select 0 from Users limit 1)"

#define STOCKS_CREATE_QUERY "CREATE TABLE IF NOT EXISTS Stocks(ID INTEGER PRIMARY KEY AUTOINCREMENT,stock_symbol VARCHAR(4) NOT NULL,stock_name VARCHAR(20) NOT NULL,stock_balance DOUBLE,user_id INTEGER,FOREIGN KEY (user_id) REFERENCES Users (ID));"
#define STOCKS_LIST_QUERY   ""

#define WHITESPACE_DELIM    " \t\b\r\n\a"

//
//

typedef struct _client_t {
    int id;
    int sock_fd;
    sockaddr_in addr;
    struct _client_t* next;
    struct _client_t* prev;
} client_t;

typedef struct _strtoken_t {
    const char* str;
    struct _strtoken_t* next;
} strtoken_t;

typedef size_t(*command_callback)(
    client_t*,   // client
    char*, // input
    size_t,      // input len
    char*,       // output
    size_t       // output len
);

//
//

sqlite3* DATABASE;
client_t* CLIENT_LIST;
int SERVER_FD;
int BROADCAST_FD;

//
//

void vlog_ns(
    const char* ns,
    const char* fmt,
    va_list vargs
) {
    printf("[%s]: ", ns);
    vprintf(fmt, vargs);
    putchar('\n');
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

strtoken_t* strtokenize(
    const char* str,
    const char* delim
) {
    strtoken_t* head = NULL;
    strtoken_t* tail = NULL;

    const char* beg = str;
    const char* end;

    while (*beg && strchr(delim, *beg) != NULL) {
        beg++;        
    }

    if (*beg == '\0') {
        return NULL;
    }

    end = beg;

    while (*end && strchr(delim, *end) == NULL) {
        ++end;
    }

    printf("\"%.*s\"\n", (int)(end-beg), beg);

    return head;
}

void strtoken_free(
    strtoken_t* pstrtok
) {
    strtoken_t* next;

    while (pstrtok != NULL) {
        next = pstrtok->next;
        free(pstrtok->str);
        free(pstrtok);
        pstrtok = next;
    }
}

//
//

size_t buy_command(
    client_t* pclient,
    char* in,
    size_t in_len,
    char* out,
    size_t out_len
) {
    
    return 0;
}

size_t sell_command(
    client_t* pclient,
    char* in,
    size_t in_len,
    char* out,
    size_t out_len
) {
    return 0;
}

size_t list_command(
    client_t* pclient,
    char* in,
    size_t in_len,
    char* out,
    size_t out_len
) {
    return true;
}

size_t balance_command(
    client_t* pclient,
    char* in,
    size_t in_len,
    char* out,
    size_t out_len
) {
    return true;
}

size_t shutdown_command(
    client_t* pclient,
    char* in,
    size_t in_len,
    char* out,
    size_t out_len
) {
    return true;
}

size_t quit_command(
    client_t* pclient,
    char* in,
    size_t in_len,
    char* out,
    size_t out_len
) {
    return true;
}

//
//

void heartbeat() {
    sockaddr_in broadcast_addr;
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;

    int ret = sendto(BROADCAST_FD, MAGIC_TEXT, strlen(MAGIC_TEXT), 0, 
        (sockaddr*)&broadcast_addr, sizeof(sockaddr));

    if (ret < 0) {
        log_ns("Heartbeat", "Failed To Send Magic: %s", strerror(errno));
    } else {
        log_ns("Heartbeat", "Sent Magic!");
    }
}

void client_handle(
    client_t* pclient,
    const char* in_buffer,
    size_t in_len
) {
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

    size_t command_idx = 0;
    while (command_idx != LENGTHOF(commands)) {
        if (strincmp(commands[command_idx].prefix, in_buffer, 
            strlen(commands[command_idx].prefix)) == 0) {
            break;
        }
        command_idx++;
    }

    if (command_idx == LENGTHOF(commands)) {
        log_inet(pclient->addr, "Client Entered Unknown Command");
        return;
    }

    log_inet(pclient->addr, "Client Ran Command: %s", commands[command_idx].prefix);

    char out_buffer[1024];
    size_t written_len = commands[command_idx].callback(pclient, 
        strtok(in_buffer, WHITESPACE_DELIM), in_len, out_buffer, LENGTHOF(out_buffer));

    if (written_len != 0) {
        // send response to client
    }
}

void client_accept(
    int client_fd,
    const sockaddr_in* pclient_addr
) {
    if (client_fd < 0 || pclient_addr == NULL) {
        fatal_error("Invalid Arguments");
    }

    client_t* new_client = (client_t*)calloc(1, sizeof(client_t));

    if (new_client == NULL) {
        fatal_error("Total Memory Error");
    }

    new_client->addr = *pclient_addr;
    new_client->sock_fd = client_fd;

    if (CLIENT_LIST != NULL) {
        CLIENT_LIST->prev = new_client;
        new_client->next = CLIENT_LIST;
    }

    CLIENT_LIST = new_client;

    log_inet(*pclient_addr, "Client Added To Pool");    
}

void client_remove(
    client_t* pclient
) {
    log_inet(pclient->addr, "Removing Client");

    if (pclient->prev != NULL) {
        pclient->prev->next = pclient->next;
    }

    if (pclient->next != NULL) {
        pclient->next->prev = pclient->prev;
    }

    if (pclient == CLIENT_LIST) {
        CLIENT_LIST = pclient->next;
    }
}

void client_recv(
    client_t* pclient
) {
    if (pclient == NULL) {
        return;
    }

    char in_buffer[1024];

    int ret = recv(pclient->sock_fd, in_buffer, LENGTHOF(in_buffer) - 1, MSG_DONTWAIT);

    if (ret <= 0) {
        if (!FD_WOULDBLOCK) {
            log_inet(pclient->addr, "Client Failed To Send Data: %s", strerror(errno));
            client_remove(pclient);
        }
        return;
    }

    in_buffer[ret] = '\0';

    log_inet(pclient->addr, "Got Data: %s", in_buffer);

    client_handle(pclient, in_buffer, ret);
}

//
//

void initialize() {
    // Server Socket

    if ((SERVER_FD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        fatal_error("Failed To Create Server Socket");        
    }

    sockaddr_in server_addr = { 0 };
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(SERVER_FD, (sockaddr*)&server_addr, sizeof(sockaddr)) < 0) {
        fatal_error("Failed To Bind Server Socket");
    }

    log_ns("Init", "Server Bound To Port %hu", SERVER_PORT);

    if (listen(SERVER_FD, SOMAXCONN) < 0) {
        fatal_error("Failed To Listen On Server Socket");
    }

    log_ns("Init", "Server Is Listening...");

    if (fcntl(SERVER_FD, F_SETFL, fcntl(SERVER_FD, F_GETFL) | O_NONBLOCK) == -1) {
        fatal_error("Failed To Put Server Socket Into Non-Blocking Mode");
    }

    // Broadcast Socket

    if ((BROADCAST_FD = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        fatal_error("Failed To Create Broadcast Socket");
    }

    int opt_true = 1;
    if (setsockopt(BROADCAST_FD, SOL_SOCKET, SO_BROADCAST, &opt_true, sizeof(opt_true)) < 0) {
        fatal_error("Failed To Enable Broadcasting On Socket");
    }

    //

    CLIENT_LIST = NULL;

    // setup database
    
    sqlite3_open("system.db", &DATABASE);
    char* error_msg = NULL;

    if (DATABASE == NULL) {
        fatal_error("Failed To Open SQLite Database");
    }

    if (sqlite3_exec(DATABASE, USERS_CREATE_QUERY, NULL, 0, &error_msg) != SQLITE_OK) {
        sqlite3_free(error_msg);
    }

    if (sqlite3_exec(DATABASE, STOCKS_CREATE_QUERY, NULL, 0, &error_msg) != SQLITE_OK) {
        sqlite3_free(error_msg);
    }

}

void deinitialize() {
    close(SERVER_FD);
    close(BROADCAST_FD);
}

int main() {

    strtoken_t* tokl = strtokenize("Hello, World! I Am Here", WHITESPACE_DELIM);
    strtoken_t* cpy = tokl; 

    while (cpy != NULL) {
        cpy = cpy->next;
    }

    strtoken_free(tokl);

    return 1;

    initialize();

    //
    //

    clock_t last = 0;
    clock_t now = 0;

    while (true) {
        // send a heartbeat every 3 seconds

        if (((now = clock()) - last) / CLOCKS_PER_SEC >= 3) {
            heartbeat();
            last = now;
        }

        // check if a client is connecting

        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(sockaddr);
        int client_fd = accept(SERVER_FD, (sockaddr*)&client_addr, &client_addr_len);        

        if (client_fd >= 0) {
            client_accept(client_fd, &client_addr);
        } 
        else if (!FD_WOULDBLOCK) {
            fatal_error("Failed To Accept Client");
        }

        // loop through clients 

        client_t* iter = CLIENT_LIST;

        while (iter != NULL) {
            client_recv(iter);
            iter = iter->next;
        }
    }

    //
    //

    deinitialize();
}

