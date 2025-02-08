#include "shared.h"

#define CODE_200 "200 OK"
#define CODE_400 "400 Invalid Command"
#define CODE_401 "401 User Does Not Exist"
#define CODE_402 "402 Insufficient Balance"
#define CODE_403 "403 Message Format Error"

#define USERS_CREATE_QUERY "CREATE TABLE IF NOT EXISTS Users(ID INTEGER PRIMARY KEY AUTOINCREMENT,first_name TEXT,last_name TEXT,user_name TEXT NOT NULL,password TEXT,usd_balance DOUBLE NOT NULL);"
#define USERS_EMPTY_QUERY "SELECT COUNT(*) FROM (select 0 from Users limit 1)"
#define USERS_COUNT_QUERY "SELECT COUNT(*) FROM Users"
#define USERS_INSERT_QUERY "INSERT INTO Users (first_name, last_name, user_name, password, usd_balance) VALUES (?1, ?2, ?3, ?4, ?5)"
#define USERS_BALANCE_QUERY "SELECT usd_balance FROM Users WHERE ID = ?1"
#define USERS_UPDATE_BALANCE_QUERY "UPDATE Users SET usd_balance = ?1 WHERE id = ?2"

#define STOCKS_CREATE_QUERY "CREATE TABLE IF NOT EXISTS Stocks(ID INTEGER PRIMARY KEY AUTOINCREMENT,stock_symbol VARCHAR(4) NOT NULL,stock_name VARCHAR(20) NOT NULL,stock_balance DOUBLE,user_id INTEGER,FOREIGN KEY (user_id) REFERENCES Users (ID));"
#define STOCKS_INSERT_QUERY "INSERT INTO Stocks (stock_symbol, stock_balance, user_id) VALUES (?1, ?2, ?3)"
#define STOCKS_COUNT_QUERY "SELECT COUNT(*) FROM (select 0 from Stocks limit 1) WHERE user_id = ?1 AND stock_symbol = ?2"
#define STOCKS_BALANCE_QUERY "SELECT stock_balance from STOCKS WHERE user_id = ?1 AND stock_symbol = ?2"
#define STOCKS_UPDATE_BALANCE_QUERY "UPDATE Stocks SET stock_balance = ?1 WHERE id = ?2 AND stock_symbol = ?3"

//
// Structures
//

typedef struct _client_t {
    int sock_fd;
    sockaddr_in addr;
    struct _client_t* next;
    struct _client_t* prev;
} client_t;

typedef void(*command_callback)(
    client_t*,  // client
    const char* // input
);

typedef struct _command_t {
    const char* prefix;
    command_callback callback;
} command_t;

//
// Declarations
//

void buy_command(client_t*, const char*);
void sell_command(client_t*, const char*);
void list_command(client_t*, const char*);
void balance_command(client_t*, const char*);
void shutdown_command(client_t*, const char*);
void quit_command(client_t*, const char*);

void client_handle(client_t*, const char*, size_t);
void client_accept(int, const sockaddr_in*);
void client_remove(client_t*);
void client_send(client_t*, const char*, ...);

//
// State / Global Variables
//

sqlite3* DATABASE;
client_t* CLIENT_LIST;

int SERVER_FD;
int BROADCAST_FD;

command_t COMMANDS[] = {
    { "buy",      buy_command      },
    { "sell",     sell_command     },
    { "list",     list_command     },
    { "balance",  balance_command  },
    { "shutdown", shutdown_command },
    { "quit",     quit_command     },
};

//
// Logging Utils
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
// Database Interactions
//

void db_add_user(
    const char* first_name,
    const char* last_name,
    const char* user_name,
    const char* password,
    double balance
) {
    sqlite3_stmt* statement;

    int ret = sqlite3_prepare_v2(DATABASE, USERS_INSERT_QUERY, -1, &statement, NULL);

    fatal_assert(ret == SQLITE_OK, "Failed To Prepare User Insertion");

    sqlite3_bind_text(statement, 1, first_name, -1, NULL);
    sqlite3_bind_text(statement, 2, last_name, -1, NULL);
    sqlite3_bind_text(statement, 3, user_name, -1, NULL);
    sqlite3_bind_text(statement, 4, password, -1, NULL);
    sqlite3_bind_double(statement, 5, balance);

    fatal_assert(sqlite3_step(statement) == SQLITE_DONE, "Failed To Run User Insertion Query");
    fatal_assert(sqlite3_finalize(statement) == SQLITE_OK, "Failed To Finalize User Insertion Query");
}

double db_get_balance(
    int user_id
) {
    sqlite3_stmt* statement;

    int ret = sqlite3_prepare_v2(DATABASE, USERS_BALANCE_QUERY, -1, &statement, NULL);

    fatal_assert(ret == SQLITE_OK, "Failed To Prepare User Balance Query");

    sqlite3_bind_int(statement, 1, user_id);

    ret = sqlite3_step(statement);

    double balance = sqlite3_column_double(statement, 0);

    fatal_assert(sqlite3_finalize(statement) == SQLITE_OK, "Failed To Finalize User Balance Query");

    return balance;
}

void db_set_balance(
    int user_id,
    double balance
) {
    sqlite3_stmt* statement;

    int ret = sqlite3_prepare_v2(DATABASE, USERS_UPDATE_BALANCE_QUERY, -1, &statement, NULL);

    fatal_assert(ret == SQLITE_OK, "Failed To Prepare Balance Update Query");

    sqlite3_bind_double(statement, 1, balance);
    sqlite3_bind_int(statement, 2, user_id);

    fatal_assert(sqlite3_step(statement) == SQLITE_DONE, "Failed To Run Balance Update Query");
    fatal_assert(sqlite3_finalize(statement) == SQLITE_OK, "Failed To Finalize Balance Update Query");
}

int db_user_count() {
    sqlite3_stmt* statement;

    int ret = sqlite3_prepare_v2(DATABASE, USERS_COUNT_QUERY, -1, &statement, NULL);

    fatal_assert(ret == SQLITE_OK, "Failed To Prepare User Count Query");

    ret = sqlite3_step(statement);

    int count = sqlite3_column_int(statement, 0);

    fatal_assert(sqlite3_finalize(statement) == SQLITE_OK, "Failed To Finalize User Count Query");

    return count;
}

bool db_has_stock(
    int user_id,
    const char* symbol
) {
    sqlite3_stmt* statement;

    int ret = sqlite3_prepare_v2(DATABASE, STOCKS_COUNT_QUERY, -1, &statement, NULL);

    fatal_assert(ret == SQLITE_OK, "Failed To Prepare Stock Count Query");
    
    sqlite3_bind_int(statement, 1, user_id);
    sqlite3_bind_text(statement, 2, symbol, -1, NULL);

    ret = sqlite3_step(statement);

    int count = sqlite3_column_int(statement, 0);

    fatal_assert(sqlite3_finalize(statement) == SQLITE_OK, "Failed To Finalize Stock Count Query");

    return !!count;
}

double db_get_stock_balance(
    int user_id,
    const char* symbol
) {
    sqlite3_stmt* statement;

    int ret = sqlite3_prepare_v2(DATABASE, STOCKS_BALANCE_QUERY, -1, &statement, NULL);

    fatal_assert(ret == SQLITE_OK, "Failed To Prepare User Balance Query");

    sqlite3_bind_int(statement, 1, user_id);

    ret = sqlite3_step(statement);

    double balance = sqlite3_column_double(statement, 0);

    fatal_assert(sqlite3_finalize(statement) == SQLITE_OK, "Failed To Finalize User Balance Query");

    return balance;
}

void db_set_stock_balance(
    int user_id,
    const char* symbol,
    double balance
) {
    sqlite3_stmt* statement;

}

//
//  Commands
//

void buy_command(
    client_t* pclient, 
    const char* args
) { 
    char ticker[1024];
    double amount;
    double price;
    uint32_t id;
    
    int arg_count = sscanf(args, "%s %lf %lf %u", ticker, &amount, &price, &id);
    
    if (arg_count != 4 || amount <= 0.0 || price <= 0.0) {
        client_send(pclient, CODE_403);
        return;
    }

    int user_count = db_user_count();

    if (id == 0 || id > user_count) {
        client_send(pclient, CODE_401);
        return;
    }
    
    double cost = amount * price;
    double balance = db_get_balance(id);

    if (cost > balance) {
        client_send(pclient, CODE_402);
        return;
    }

    db_set_balance(id, balance - cost);

    
}

void sell_command(
    client_t* pclient, 
    const char* args
) {

}

void list_command(
    client_t* pclient, 
    const char* args
) { }

void balance_command(
    client_t* pclient, 
    const char* args
) { }

void shutdown_command(
    client_t* pclient, 
    const char* args
) { }

void quit_command(
    client_t* pclient, 
    const char* args
) { }


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

//
//  Client Interactions
//

void client_handle(
    client_t* pclient,
    const char* in_buffer,
    size_t in_len
) {
    size_t command_idx = 0;
    while (command_idx != LENGTHOF(COMMANDS)) {
        if (strincmp(COMMANDS[command_idx].prefix, in_buffer, 
            strlen(COMMANDS[command_idx].prefix)) == 0) {
            break;
        }
        command_idx++;
    }

    if (command_idx == LENGTHOF(COMMANDS)) {
        client_send(pclient, CODE_400);
        return;
    }

    log_inet(pclient->addr, "Client Ran Command: %s", COMMANDS[command_idx].prefix);
    
    const char* args = strchr(in_buffer, ' ');

    COMMANDS[command_idx].callback(
        pclient,
        args == NULL ? args : args + 1
    );
}

void client_accept(
    int client_fd,
    const sockaddr_in* pclient_addr
) {
    fatal_assert(client_fd >= 0 && pclient_addr != NULL, "Invalid Arguments");

    client_t* new_client = (client_t*)calloc(1, sizeof(client_t));

    fatal_assert(new_client != NULL, "Out Of Memory");

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

void client_send(
    client_t* pclient,
    const char* fmt,
    ...
) {
    va_list vargs;
    va_start(vargs, fmt);

    char* msg = vformat(fmt, vargs);

    int ret = send(pclient->sock_fd, msg, strlen(msg), MSG_DONTWAIT);

    if (ret <= 0 && !FD_WOULDBLOCK) {
        log_inet(pclient->addr, "Failed To Send Data, Removing...");
        client_remove(pclient);
    }

    va_end(vargs);
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

    log_ns("Init", "Broadcast Socket Created");

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

    log_ns("Init", "Database Connected");
}

void deinitialize() {
    // clean sockets

    close(SERVER_FD);
    close(BROADCAST_FD);
    
    log_ns("DeInit", "Sockets Closed");

    // close database

    sqlite3_close(DATABASE);

    log_ns("DeInit", "Database Disconnected");
}

int main() {
    initialize();

    db_set_balance(1, 10);

    return 1;

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