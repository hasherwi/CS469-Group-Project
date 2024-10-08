#ifndef _COMMUNICATIONCONSTANTS_H

#define _COMMUNICATIONCONSTANTS_H
// Error codes
static const int RPC_ERROR_TOO_MANY_ARGS = -1;
static const int RPC_ERROR_TOO_FEW_ARGS = -2;
static const int RPC_ERROR_BAD_OPERATION = -3;

// Operations to marshall with user input
static const char RPC_SEARCH_OPERATION[] = "SEARCH"; // search for mp3s using term
static const char RPC_DOWNLOAD_OPERATION[] = "DOWNLOAD"; // download mp3
static const char RPC_LIST_OPERATION[] = "LIST"; // list all mp3s available

// RPC Error messages
static const char ERROR_FILE_ERROR[] = "FILEERROR";
static const char ERROR_RPC_ERROR[] = "RPCERROR";

static const int DEFAULT_PORT = 8080;



#endif
