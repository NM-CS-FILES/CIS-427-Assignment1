/*
I find the code below to be self descriptive.
There are not comments describing every line, nor should there be.
 - Nathan Morris
*/

#include "shared.h"

#define CODE_200 "200 OK\x1"
#define CODE_400 "400 Invalid Command\x1"
#define CODE_401 "401 User Does Not Exist\x1"
#define CODE_402 "402 Insufficient Balance\x1"
#define CODE_403 "403 Message Format Error\x1"
#define CODE_404 "404 Insufficient Stock Balance\x1"

#define USERS_CREATE_QUERY "CREATE TABLE IF NOT EXISTS Users(ID INTEGER PRIMARY KEY AUTOINCREMENT,first_name TEXT,last_name TEXT,user_name TEXT NOT NULL,password TEXT,usd_balance DOUBLE NOT NULL);"
#define USERS_EMPTY_QUERY "SELECT COUNT(*) FROM (select 0 from Users limit 1)"
#define USERS_COUNT_QUERY "SELECT COUNT(*) FROM Users"
#define USERS_INSERT_QUERY "INSERT INTO Users (first_name, last_name, user_name, password, usd_balance) VALUES (?1, ?2, ?3, ?4, ?5)"
#define USERS_BALANCE_QUERY "SELECT usd_balance FROM Users WHERE ID = ?1"
#define USERS_UPDATE_BALANCE_QUERY "UPDATE Users SET usd_balance = ?1 WHERE id = ?2"

#define STOCKS_CREATE_QUERY "CREATE TABLE IF NOT EXISTS Stocks(ID INTEGER PRIMARY KEY AUTOINCREMENT,stock_symbol VARCHAR(4) NOT NULL,stock_name VARCHAR(20),stock_balance DOUBLE,user_id INTEGER,FOREIGN KEY (user_id) REFERENCES Users (ID));"
#define STOCKS_INSERT_QUERY "INSERT INTO Stocks (stock_symbol, stock_balance, user_id) VALUES (?1, ?2, ?3)"
#define STOCKS_COUNT_QUERY "SELECT COUNT(*) FROM Stocks WHERE user_id = ?1 AND stock_symbol = ?2"
#define STOCKS_BALANCE_QUERY "SELECT stock_balance FROM Stocks WHERE user_id = ?1 AND stock_symbol = ?2"
#define STOCKS_UPDATE_BALANCE_QUERY "UPDATE Stocks SET stock_balance = ?1 WHERE user_id = ?2 AND stock_symbol = ?3"
#define STOCKS_LIST_QUERY "SELECT stock_symbol, stock_balance FROM Stocks WHERE user_id = ?1"

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

void db_add_user(const char*, const char*, const char*, const char*, double);
double db_get_balance(int);
void db_set_balance(int, double);
int db_user_count();
bool db_has_stock(int, const char*);
void db_add_stock(int, const char*, double);
double db_get_stock_balance(int, const char*);
void db_set_stock_balance(int, const char*, double);
int db_list_stock(int, char**, double*, int);

void client_handle(client_t*, const char*, size_t);
void client_accept(int, const sockaddr_in*);
void client_remove(client_t*);
void client_send(client_t*, const char*, ...);
void client_recv(client_t*);

//
// State / Global Variables
//

bool RUNNING;

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

void db_add_stock(
    int user_id,
    const char* symbol,
    double balance
) {
    sqlite3_stmt* statement;

    int ret = sqlite3_prepare_v2(DATABASE, STOCKS_INSERT_QUERY, -1, &statement, NULL);

    fatal_assert(ret == SQLITE_OK, "Failed To Prepare Stock Insert Query");

    sqlite3_bind_text(statement, 1, symbol, -1, NULL);
    sqlite3_bind_double(statement, 2, balance);
    sqlite3_bind_int(statement, 3, user_id);

    fatal_assert(sqlite3_step(statement) == SQLITE_DONE, "Failed To Run Stock Insert Query");
    fatal_assert(sqlite3_finalize(statement) == SQLITE_OK, "Failed To Finalize Stock Insert Query");
}

double db_get_stock_balance(
    int user_id,
    const char* symbol
) {
    sqlite3_stmt* statement;

    int ret = sqlite3_prepare_v2(DATABASE, STOCKS_BALANCE_QUERY, -1, &statement, NULL);

    fatal_assert(ret == SQLITE_OK, "Failed To Prepare Stock Balance Query");

    sqlite3_bind_int(statement, 1, user_id);
    sqlite3_bind_text(statement, 2, symbol, -1, NULL);

    ret = sqlite3_step(statement);

    double balance = sqlite3_column_double(statement, 0);

    fatal_assert(sqlite3_finalize(statement) == SQLITE_OK, "Failed To Finalize Stock Balance Query");

    return balance;
}

void db_set_stock_balance(
    int user_id,
    const char* symbol,
    double balance
) {
    sqlite3_stmt* statement;

    int ret = sqlite3_prepare_v2(DATABASE, STOCKS_UPDATE_BALANCE_QUERY, -1, &statement, NULL);

    fatal_assert(ret == SQLITE_OK, "Failed To Prepare Stock Update Balance Query");

    sqlite3_bind_double(statement, 1, balance);
    sqlite3_bind_int(statement, 2, user_id);
    sqlite3_bind_text(statement, 3, symbol, -1, NULL);

    ret = sqlite3_step(statement);

    fatal_assert(sqlite3_finalize(statement) == SQLITE_OK, "Failed To Finalize Stock Update Balance Query");
}

int db_list_stock(
    int user_id, 
    char** tickers, 
    double* balances, 
    int count
) {
    sqlite3_stmt* statement;

    int ret = sqlite3_prepare_v2(DATABASE, STOCKS_LIST_QUERY, -1, &statement, NULL);
    int read_count = 0;

    fatal_assert(ret == SQLITE_OK, "Failed To Prepare Stock List Query");

    sqlite3_bind_int(statement, 1, user_id);

    while (sqlite3_step(statement) == SQLITE_ROW) {

        if (tickers != NULL && read_count < count) {
            const char* col_ticker = (const char*)sqlite3_column_text(statement, 0);
            fatal_assert(col_ticker != NULL, "Out Of Memory");
            tickers[read_count] = calloc(strlen(col_ticker) + 1, sizeof(char));
            strcpy(tickers[read_count], col_ticker);
        }

        if (balances != NULL && read_count < count) {
            balances[read_count] = sqlite3_column_double(statement, 1);
        }

        read_count++;
    }

    fatal_assert(sqlite3_finalize(statement) == SQLITE_OK, "Failed To Finalize Stock List Query");

    return read_count;
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
    int id;
    
    if (args == NULL) {
        client_send(pclient, CODE_403);
        return;
    }

    int arg_count = sscanf(args, "%s %lf %lf %d", ticker, &amount, &price, &id);
    
    if (arg_count != 4 || amount <= 0.0 || price <= 0.0) {
        client_send(pclient, CODE_403);
        return;
    }

    int user_count = db_user_count();

    if (id <= 0 || id > user_count) {
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

    if (!db_has_stock(id, ticker)) {
        db_add_stock(id, ticker, amount);
    } else {
        double stock_balance = db_get_stock_balance(id, ticker);
        db_set_stock_balance(id, ticker, stock_balance + amount);
    }

    client_send(pclient, CODE_200);
    return;
}

void sell_command(
    client_t* pclient, 
    const char* args
) {
    char ticker[1024];
    double amount;
    double price;
    int id;

    if (args == NULL) {
        client_send(pclient, CODE_403);
        return;
    }

    int arg_count = sscanf(args, "%s %lf %lf %d", ticker, &price, &amount, &id);

    if (arg_count != 4 || amount <= 0.0 || price <= 0.0) {
        client_send(pclient, CODE_403);
        return;
    }

    int user_count = db_user_count();

    if (id <= 0 || id > user_count) {
        client_send(pclient, CODE_401);
        return;
    }

    if (!db_has_stock(id, ticker)) {
        client_send(pclient, CODE_404);
        return;
    }

    double balance = db_get_stock_balance(id, ticker);

    if (balance < amount) {
        client_send(pclient, CODE_404);
        return;
    }

    db_set_stock_balance(id, ticker, balance - amount);

    double cost = amount * price;

    db_set_balance(id, db_get_balance(id) + cost);

    client_send(pclient, CODE_200);
}

void list_command(
    client_t* pclient, 
    const char* args
) { 
    int id = 1;

    if (args != NULL) {
        int arg_count = sscanf(args, "%d", &id);

        if (arg_count != 1) {
            client_send(pclient, CODE_403);
            return;
        }
    }

    if (id <= 0 || id > db_user_count()) {
        client_send(pclient, CODE_401);
        return;
    }

    char** tickers;
    double* balances;

    int count = db_list_stock(id, NULL, NULL, 0);

    if (count == 0) {
        client_send(pclient, "%s\nNo Stocks Owned", CODE_200);
        return;
    }

    tickers = (char**)calloc(count, sizeof(char*));

    fatal_assert(tickers != NULL, "Out Of Memory");

    balances = (double*)calloc(count, sizeof(double));

    fatal_assert(balances != NULL, "Out Of Memory");

    count = db_list_stock(id, tickers, balances, count);

    // could-a, should-a, would-a used C++
    char list_buffer[1024];

    memcpy(list_buffer, CODE_200, strlen(CODE_200));

    char* pfront = list_buffer + strlen(CODE_200);

    *(pfront++) = '\n';

    char* pend = &list_buffer[1024];

    for (int i = 0; i != count && pfront < pend; i++) {
        int written = snprintf(pfront, (pend - pfront) + 1, "%c%s : %.2lf", i ? ' ' : '\n', tickers[i], balances[i]);

        if (written < 0) {
            break;
        }

        pfront += written;
    }

    for (int i = 0; i != count; i++) {
        free(tickers[i]);
    }

    free(tickers);
    free(balances);

    client_send(pclient, list_buffer);
}

void balance_command(
    client_t* pclient, 
    const char* args
) { 
    int id = 1;

    if (args != NULL) {
        int arg_count = sscanf(args, "%d", &id);

        if (arg_count != 1) {
            client_send(pclient, CODE_403);
            return;
        }
    }

    if (id <= 0 || id > db_user_count()) {
        client_send(pclient, CODE_401);
        return;
    }

    client_send(pclient, "%s\nBalance = %.2lf", CODE_200, db_get_balance(id));
}

void shutdown_command(
    client_t* pclient, 
    const char* args
) { 
    if (args != NULL) {
        client_send(pclient, CODE_403);
        return;
    }
    
    client_send(pclient, CODE_200);
    
    RUNNING = false;
}

void quit_command(
    client_t* pclient, 
    const char* args
) { 
    if (args != NULL) {
        client_send(pclient, CODE_403);
        return;
    }    

    client_send(pclient, CODE_200);

    client_remove(pclient);
    free(pclient);
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

    const char* args = strchr(in_buffer, ' ');

    if (args != NULL) {
        args++;
    }

    log_inet(pclient->addr, "Client Ran Command: %s, With Args: %s", COMMANDS[command_idx].prefix, args);
    
    COMMANDS[command_idx].callback(pclient, args);
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
    va_list vargs_cpy;
    va_start(vargs, fmt);
    va_copy(vargs_cpy, vargs);

    char* buffer;

    int len = vsnprintf(NULL, 0, fmt, vargs_cpy);

    buffer = (char*)malloc(len + 1);

    fatal_assert(buffer != NULL, "Out Of Memory");

    vsnprintf(buffer, len + 1, fmt, vargs);

    int ret = send(pclient->sock_fd, buffer, len, MSG_DONTWAIT);

    free(buffer);

    if (ret <= 0 && !FD_WOULDBLOCK) {
        log_inet(pclient->addr, "Failed To Send Data: %s", strerror(errno));
        client_remove(pclient);
        free(pclient);
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
            log_inet(pclient->addr, "Failed To Recieve Data: %s", strerror(errno));
            client_remove(pclient);
            free(pclient);
        }

        return;
    }

    // ensure null char
    in_buffer[ret] = '\0';

    client_handle(pclient, in_buffer, ret);
}

//
// Other Stuff
//

void initialize() {
    // Globals

    CLIENT_LIST = NULL;
    RUNNING = true;

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

    if (db_user_count() == 0) {
        db_add_user("Nathan", "Morris", "nmorrisk", "password", 1000.0);
        db_add_user("Jeffery", "Epstein", "FinanceKing16", "ilovekids", 10000000.0);
        db_add_user("Robert", "Kelley", "RKelly", "goldenshowers", 100000.0);
    }
}

void deinitialize() {
    // clean any clients

    client_t* iter = CLIENT_LIST;
    client_t* next;

    while (iter != NULL) {
        next = iter->next;
        close(iter->sock_fd);
        free(iter);
        iter = next;
    }

    log_ns("DeInit", "Clients Freed");

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

    //

    clock_t last = 0;
    clock_t now = 0;

    while (RUNNING) {
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

    deinitialize();
}