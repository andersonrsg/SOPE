#ifndef constants_h_
#define constants_h_

// Defined constants
#define MAX_MSG_LEN            100      // Maximum message lenght
#define MAX_ROOM_SEATS        9999      // Maximum number of seats
#define MAX_CLI_SEATS           99      // Maximum number of seats a client can book at a time
#define RQT_FIFO_NAME    "requests"     // Requests FIFO name
#define SV_LOG_NAME      "slog.txt"     // Server logging file name
#define SV_BOOK_NAME    "sbook.txt"     // Server bookings file name

// Defined Error constants
#define MAX                    "-1"     // Number of desired seats is higher than permited
#define NST                    "-2"     // Number of preferred seats is inval
#define IID                    "-3"     // At least one preferred seat id is invalid
#define ERR                    "-4"     // Any other parameter error
#define NAV                    "-5"     // Number of desired seats not available
#define FUL                    "-6"     // The room is full

#endif 