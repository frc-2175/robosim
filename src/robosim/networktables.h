/*
 * A single-header NetworkTables library. Compatible with both C and C++.
 * Designed to be as simple as possible to drop into client programs or bind to
 * other languages.
 */

#ifndef NETWORKTABLES_H
#define NETWORKTABLES_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(_WIN32)
#define NOUSER
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
#undef NOUSER
#endif

#include "hexdump.h"

static uint16_t NTProtocolVersion = 0x0200;
#define NT_PROTOCOL_VERSION 0x02, 0x00

#define NT_MAX_ENTRIES 255 // TODO: This should be higher, but it can't live in the client because it obliterates the stack.

typedef enum NTEntryType {
    NTEntryType_Boolean = 0x00,
    NTEntryType_Double  = 0x01,
    NTEntryType_String  = 0x02,

    NTEntryType_BooleanArray = 0x10,
    NTEntryType_DoubleArray  = 0x11,
    NTEntryType_StringArray  = 0x12,
} NTEntryType;

typedef enum NTMessageType {
    NTMessageType_KeepAlive                  = 0x00,
    NTMessageType_ClientHello                = 0x01,
    NTMessageType_ProtocolVersionUnsupported = 0x02,
    NTMessageType_ServerHelloComplete        = 0x03,
    
    NTMessageType_EntryAssignment = 0x10,
    NTMessageType_EntryUpdate     = 0x11,
} NTMessageType;

typedef struct NTString {
    uint16_t Length;
    const char *Data;
} NTString;

typedef struct NTEntryString {
    NTString Value;
} NTEntryString;

typedef struct NTBooleanArray {
    uint8_t Length;
    bool *Elements;
} NTBooleanArray;

typedef struct NTDoubleArray {
    uint8_t Length;
    double *Elements;
} NTDoubleArray;

typedef struct NTStringArray {
    uint8_t Length;
    NTString *Elements;
} NTStringArray;

typedef union NTEntryValue {
    bool     Boolean;
    double   Double;
    NTString String;

    NTBooleanArray BooleanArray;
    NTDoubleArray  DoubleArray;
    NTStringArray  StringArray;
} NTEntryValue;

typedef struct NTEntry {
    NTString Name;
    uint16_t ID;
    NTEntryType Type;
    NTEntryValue Value;

    // generation ID of the server connection; used to ensure that we send all
    // the right fields on reconnect
    int ServerGen;

    bool Valid; // If false, entry is "free" and should not be used. Using NTEntryIsValid is recommended.
    int _Index; // Index in the client's entries array; used for iteration and should not be touched.
} NTEntry;

// TODO: Make SOCKET define on non-Windows platforms

typedef struct NTClient {
    SOCKET Socket;
    int ServerGen; // increments on each NTConnect

    NTEntry Entries[NT_MAX_ENTRIES];
    int NumEntries;

    void *TempBuffer; // Used for temporary allocations, e.g. string formatting. Uses default if null.
} NTClient;

typedef struct NTError {
    const char *Error;
    int ErrorCode;
} NTError;

#define NT_RESULT(type) \
    struct NT_##type##Result { \
        type Value; \
        const char *Error; \
        int ErrorCode; \
    }

typedef NT_RESULT(bool) NTBooleanResult;
typedef NT_RESULT(double) NTDoubleResult;
typedef NT_RESULT(NTString) NTStringResult;
typedef NT_RESULT(NTBooleanArray) NTBooleanArrayResult;
typedef NT_RESULT(NTDoubleArray) NTDoubleArrayResult;
typedef NT_RESULT(NTStringArray) NTStringArrayResult;

#define NT_RETURN_ERROR(type, err, code) \
    type __res = {0}; \
    __res.Error = err; \
    __res.ErrorCode = code; \
    return __res;

#define NTEachEntry(client, it) NTEntry *it = &(client)->Entries[0]; NTEntryIsValid(it); it = _NTNextEntry(client, it)

#ifdef __cplusplus
extern "C" {
#endif

NTError NTConnect(NTClient *client, const char *addr);
void NTDisconnect(NTClient *client);

NTBooleanResult NTRecvBoolean(NTClient *client);
NTDoubleResult NTRecvDouble(NTClient *client);
NTStringResult NTRecvString(NTClient *client);
NTBooleanArrayResult NTRecvBooleanArray(NTClient *client);
NTDoubleArrayResult NTRecvDoubleArray(NTClient *client);
NTStringArrayResult NTRecvStringArray(NTClient *client);

int NTStringCmp(NTString a, NTString b);

void NTStringFree(NTString *s);
void NTBooleanArrayFree(NTBooleanArray *arr);
void NTDoubleArrayFree(NTDoubleArray *arr);
void NTStringArrayFree(NTStringArray *arr);
void NTEntryFreeContents(NTEntry *e);

uint16_t NTBigEndianInt16(char *bytes);
double NTBigEndianDouble(char *bytes);

bool NTGetBoolean(NTClient *client, const char *key, bool defaultValue);
double NTGetDouble(NTClient *client, const char *key, double defaultValue);
NTString NTGetString(NTClient *client, const char *key, NTString defaultValue);
const char *NTGetStringC(NTClient *client, const char *key, const char *defaultValue);
NTBooleanArray NTGetBooleanArray(NTClient *client, const char *key);
NTDoubleArray NTGetDoubleArray(NTClient *client, const char *key);
NTStringArray NTGetStringArray(NTClient *client, const char *key);

// Alias of NTGetDouble.
double NTGetNumber(NTClient *client, const char *key, double defaultValue);

// Alias of NTGetDoubleArray.
NTDoubleArray NTGetNumberArray(NTClient *client, const char *key);

bool NTEntryIsValid(NTEntry *entry);
NTEntry *_NTNextEntry(NTClient *client, NTEntry *it);

#ifndef NT_TEMP_SIZE
#define NT_TEMP_SIZE (10 * 1024)
#endif
static char _NTTempBuffer[NT_TEMP_SIZE];

#ifdef NETWORKTABLES_IMPLEMENTATION

NTError NTConnect(NTClient *client, const char *addr) {
    client->Socket = INVALID_SOCKET;
    client->ServerGen += 1;

    // Init winsock
    WSADATA wsaData;
    int wsresult;
    wsresult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsresult != NO_ERROR) {
        NT_RETURN_ERROR(NTError, "WSAStartup failed", wsresult);
    }

    // Create a socket
    client->Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client->Socket == INVALID_SOCKET) {
        WSACleanup();
        NT_RETURN_ERROR(NTError, "error at socket()", WSAGetLastError());
    }

    // Connect
    struct sockaddr_in clientService;
    clientService.sin_family = AF_INET;
    clientService.sin_addr.s_addr = inet_addr(addr);
    clientService.sin_port = htons(1735);

    wsresult = connect(client->Socket, (SOCKADDR*) &clientService, sizeof(clientService));
    if (wsresult == SOCKET_ERROR) {
        closesocket(client->Socket);
        WSACleanup();
        NT_RETURN_ERROR(NTError, "unable to connect to server", WSAGetLastError());
    }

    // Handshake

    char hello[] = {
        NTMessageType_ClientHello,
        NT_PROTOCOL_VERSION,
    };
    wsresult = send(client->Socket, hello, sizeof(hello), 0);
    if (wsresult == SOCKET_ERROR) {
        closesocket(client->Socket);
        WSACleanup();
        NT_RETURN_ERROR(NTError, "hello failed", WSAGetLastError());
    }

    // char debuggeroni[512];
    // if (recv(client->Socket, debuggeroni, 512, 0) <= 0) {
    //     NT_RETURN_ERROR(NTError, "error reading initial message type", WSAGetLastError());
    // }
    // HexDump("Raw Bytes", debuggeroni, 512);

    while (true) {
        char msgType;
        if (recv(client->Socket, &msgType, 1, 0) <= 0) {
            NT_RETURN_ERROR(NTError, "error reading initial message type", WSAGetLastError());
        }
        switch (msgType) {
            case NTMessageType_ProtocolVersionUnsupported: {
                NT_RETURN_ERROR(NTError, "protocol version unsupported", 0);
            } break;
            case NTMessageType_EntryAssignment: {
                NTStringResult nameRes = NTRecvString(client);
                if (nameRes.Error) {
                    NT_RETURN_ERROR(NTError, nameRes.Error, nameRes.ErrorCode);
                }
                NTString name = nameRes.Value;

                char metadata[1 + 2 + 2]; // type + id + sequence number (I didn't want to do more than one recv)
                if (recv(client->Socket, metadata, sizeof(metadata), 0) <= 0) {
                    NT_RETURN_ERROR(NTError, "error reading entry metadata", WSAGetLastError());
                }
                uint8_t type = metadata[0];
                uint16_t id = NTBigEndianInt16(&metadata[1]);
                uint16_t seq = NTBigEndianInt16(&metadata[3]);

                NTEntryValue value = {0};
                switch (type) {
                    case NTEntryType_Boolean: {
                        NTBooleanResult res = NTRecvBoolean(client);
                        if (res.Error) {
                            NT_RETURN_ERROR(NTError, res.Error, res.ErrorCode);
                        }
                        value.Boolean = res.Value;
                    } break;
                    case NTEntryType_Double: {
                        NTDoubleResult res = NTRecvDouble(client);
                        if (res.Error) {
                            NT_RETURN_ERROR(NTError, res.Error, res.ErrorCode);
                        }
                        value.Double = res.Value;
                    } break;
                    case NTEntryType_String: {
                        NTStringResult res = NTRecvString(client);
                        if (res.Error) {
                            NT_RETURN_ERROR(NTError, res.Error, res.ErrorCode);
                        }
                        value.String = res.Value;
                    } break;
                    case NTEntryType_BooleanArray: {
                        NTBooleanArrayResult res = NTRecvBooleanArray(client);
                        if (res.Error) {
                            NT_RETURN_ERROR(NTError, res.Error, res.ErrorCode);
                        }
                        value.BooleanArray = res.Value;
                    } break;
                    case NTEntryType_DoubleArray: {
                        NTDoubleArrayResult res = NTRecvDoubleArray(client);
                        if (res.Error) {
                            NT_RETURN_ERROR(NTError, res.Error, res.ErrorCode);
                        }
                        value.DoubleArray = res.Value;
                    } break;
                    case NTEntryType_StringArray: {
                        NTStringArrayResult res = NTRecvStringArray(client);
                        if (res.Error) {
                            NT_RETURN_ERROR(NTError, res.Error, res.ErrorCode);
                        }
                        value.StringArray = res.Value;
                    } break;
                    default: {
                        NT_RETURN_ERROR(NTError, "unrecognized entry type", type);
                    }
                }
                
                NTEntry *entry = NULL;
                bool foundExistingEntry = false;
                for (int i = 0; i < client->NumEntries; i++) {
                    NTEntry *potentialEntry = &client->Entries[i];
                    if (NTStringCmp(name, potentialEntry->Name) == 0) {
                        foundExistingEntry = true;
                        entry = potentialEntry;
                    }
                }
                if (!foundExistingEntry) {
                    // Exists only on server; replicate to client.
                    entry = &client->Entries[client->NumEntries];
                    entry->_Index = client->NumEntries;
                    client->NumEntries += 1;
                }

                NTEntryFreeContents(entry);
                entry->Name = name;
                entry->ServerGen = client->ServerGen;
                entry->ID = id;
                entry->Type = (NTEntryType)type;
                entry->Value = value;
                entry->Valid = true;
            } break;
            case NTMessageType_ServerHelloComplete: {
                goto helloComplete;
            } break;
            default: {
                NT_RETURN_ERROR(NTError, "did not recognize message type", msgType);
            } break;
        }        
    }
helloComplete:

    NTError res = {0};
    return res;
}

void NTDisconnect(NTClient *client) {
    closesocket(client->Socket);
    WSACleanup();
}

NTBooleanResult NTRecvBoolean(NTClient *client) {
    char b;
    if (recv(client->Socket, &b, 1, 0) <= 0) {
        NT_RETURN_ERROR(NTBooleanResult, "error reading boolean", WSAGetLastError());
    }
    
    NTBooleanResult res = {0};
    res.Value = b;
    return res;
}

NTDoubleResult NTRecvDouble(NTClient *client) {
    char bytes[8];
    if (recv(client->Socket, bytes, 8, 0) <= 0) {
        NT_RETURN_ERROR(NTDoubleResult, "error reading double", WSAGetLastError());
    }
    
    NTDoubleResult res = {0};
    res.Value = NTBigEndianDouble(bytes);
    return res;
}

NTStringResult NTRecvString(NTClient *client) {
    NTString str = {0};
    char lengthBytes[2];
    if (recv(client->Socket, lengthBytes, 2, 0) <= 0) {
        NT_RETURN_ERROR(NTStringResult, "error reading string length", WSAGetLastError());
    }
    str.Length = NTBigEndianInt16(lengthBytes);
    
    char *buf = (char*)malloc(str.Length + 1);
    if (str.Length > 0) {
        if (recv(client->Socket, buf, str.Length, 0) <= 0) {
            NT_RETURN_ERROR(NTStringResult, "error reading string data", WSAGetLastError());
        }
    }
    buf[str.Length] = 0; // null-terminate
    str.Data = buf;

    NTStringResult res = {0};
    res.Value = str;
    return res;
}

NTBooleanArrayResult NTRecvBooleanArray(NTClient *client) {
    NTBooleanArray arr = {0};
    char length;
    if (recv(client->Socket, &length, 1, 0) <= 0) {
        NT_RETURN_ERROR(NTBooleanArrayResult, "error reading array length", WSAGetLastError());
    }
    arr.Length = (uint8_t)length;
    arr.Elements = (bool*)malloc(arr.Length * sizeof(bool));

    for (int i = 0; i < arr.Length; i++) {
        NTBooleanResult res = NTRecvBoolean(client);
        if (res.Error) {
            NT_RETURN_ERROR(NTBooleanArrayResult, res.Error, res.ErrorCode);
        }
        arr.Elements[i] = res.Value;
    }

    NTBooleanArrayResult res = {0};
    res.Value = arr;
    return res;
}

NTDoubleArrayResult NTRecvDoubleArray(NTClient *client) {
    NTDoubleArray arr = {0};
    char length;
    if (recv(client->Socket, &length, 1, 0) <= 0) {
        NT_RETURN_ERROR(NTDoubleArrayResult, "error reading array length", WSAGetLastError());
    }
    arr.Length = (uint8_t)length;
    arr.Elements = (double*)malloc(arr.Length * sizeof(double));

    for (int i = 0; i < arr.Length; i++) {
        NTDoubleResult res = NTRecvDouble(client);
        if (res.Error) {
            NT_RETURN_ERROR(NTDoubleArrayResult, res.Error, res.ErrorCode);
        }
        arr.Elements[i] = res.Value;
    }

    NTDoubleArrayResult res = {0};
    res.Value = arr;
    return res;
}

NTStringArrayResult NTRecvStringArray(NTClient *client) {
    NTStringArray arr = {0};
    char length;
    if (recv(client->Socket, &length, 1, 0) <= 0) {
        NT_RETURN_ERROR(NTStringArrayResult, "error reading array length", WSAGetLastError());
    }
    arr.Length = (uint8_t)length;
    arr.Elements = (NTString*)malloc(arr.Length * sizeof(NTString));

    for (int i = 0; i < arr.Length; i++) {
        NTStringResult res = NTRecvString(client);
        if (res.Error) {
            NT_RETURN_ERROR(NTStringArrayResult, res.Error, res.ErrorCode);
        }
        arr.Elements[i] = res.Value;
    }

    NTStringArrayResult res = {0};
    res.Value = arr;
    return res;
}

int NTStringCmp(NTString a, NTString b) {
    int lenDiff = a.Length - b.Length;
    if (lenDiff < 0) {
        return -1;
    } else if (lenDiff > 0) {
        return 1;
    } else {
        return strncmp(a.Data, b.Data, a.Length);
    }
}

void NTStringFree(NTString *s) {
    free((void*)s->Data);
}

void NTBooleanArrayFree(NTBooleanArray *arr) {
    free(arr->Elements);
}

void NTDoubleArrayFree(NTDoubleArray *arr) {
    free(arr->Elements);
}

void NTStringArrayFree(NTStringArray *arr) {
    for (int i = 0; i < arr->Length; i++) {
        NTStringFree(&arr->Elements[i]);
    }
    free(arr->Elements);
}

void NTEntryFreeContents(NTEntry *e) {
    if (e->Name.Data) {
        NTStringFree(&e->Name);
    }
    switch (e->Type) {
        case NTEntryType_Boolean:
        case NTEntryType_Double: break;

        case NTEntryType_String: {
            NTStringFree(&e->Value.String);
        } break;
        case NTEntryType_BooleanArray: {
            NTBooleanArrayFree(&e->Value.BooleanArray);
        } break;
        case NTEntryType_DoubleArray: {
            NTDoubleArrayFree(&e->Value.DoubleArray);
        } break;
        case NTEntryType_StringArray: {
            NTStringArrayFree(&e->Value.StringArray);
        } break;
    }
}

uint16_t NTBigEndianInt16(char *bytes) {
    char swapped[] = {bytes[1], bytes[0]};
    return *(uint16_t*)(swapped);
}

double NTBigEndianDouble(char *bytes) {
    char swapped[] = {
        bytes[7],
        bytes[6],
        bytes[5],
        bytes[4],
        bytes[3],
        bytes[2],
        bytes[1],
        bytes[0],
    };
    return *(double*)(swapped);
}

NTString NTStringC(const char *s) {
    NTString res = {0};
    res.Data = s;
    res.Length = strlen(s);
    return res;
}

bool NTGetBoolean(NTClient *client, const char *key, bool defaultValue) {
    NTString ntKey = NTStringC(key);
    for (NTEachEntry(client, it)) {
        if (NTStringCmp(it->Name, ntKey) == 0 && it->Type == NTEntryType_Boolean) {
            return it->Value.Boolean;
        }
    }
    return defaultValue;
}

double NTGetDouble(NTClient *client, const char *key, double defaultValue) {
    NTString ntKey = NTStringC(key);
    for (NTEachEntry(client, it)) {
        if (NTStringCmp(it->Name, ntKey) == 0 && it->Type == NTEntryType_Double) {
            return it->Value.Double;
        }
    }
    return defaultValue;
}

NTString NTGetString(NTClient *client, const char *key, NTString defaultValue) {
    NTString ntKey = NTStringC(key);
    for (NTEachEntry(client, it)) {
        if (NTStringCmp(it->Name, ntKey) == 0 && it->Type == NTEntryType_String) {
            return it->Value.String;
        }
    }
    return defaultValue;
}

const char *NTGetStringC(NTClient *client, const char *key, const char *defaultValue) {
    NTString ntKey = NTStringC(key);
    for (NTEachEntry(client, it)) {
        if (NTStringCmp(it->Name, ntKey) == 0 && it->Type == NTEntryType_String) {
            return it->Value.String.Data;
        }
    }
    return defaultValue;
}

NTBooleanArray NTGetBooleanArray(NTClient *client, const char *key) {
    NTString ntKey = NTStringC(key);
    for (NTEachEntry(client, it)) {
        if (NTStringCmp(it->Name, ntKey) == 0 && it->Type == NTEntryType_BooleanArray) {
            return it->Value.BooleanArray;
        }
    }
    NTBooleanArray def = {0};
    return def;
}

NTDoubleArray NTGetDoubleArray(NTClient *client, const char *key) {
    NTString ntKey = NTStringC(key);
    for (NTEachEntry(client, it)) {
        if (NTStringCmp(it->Name, ntKey) == 0 && it->Type == NTEntryType_DoubleArray) {
            return it->Value.DoubleArray;
        }
    }
    NTDoubleArray def = {0};
    return def;
}

NTStringArray NTGetStringArray(NTClient *client, const char *key) {
    NTString ntKey = NTStringC(key);
    for (NTEachEntry(client, it)) {
        if (NTStringCmp(it->Name, ntKey) == 0 && it->Type == NTEntryType_StringArray) {
            return it->Value.StringArray;
        }
    }
    NTStringArray def = {0};
    return def;
}

double NTGetNumber(NTClient *client, const char *key, double defaultValue) {
    return NTGetDouble(client, key, defaultValue);
}

NTDoubleArray NTGetNumberArray(NTClient *client, const char *key) {
    return NTGetDoubleArray(client, key);
}

bool NTEntryIsValid(NTEntry *entry) {
    bool valid = entry && entry->Valid;
    if (valid) {
        // some sanity checks
        assert(entry->Name.Data[entry->Name.Length] == 0); // name is null-terminated
        if (entry->Type == NTEntryType_String) { // string value is null-terminated
            NTString val = entry->Value.String;
            assert(val.Data[val.Length] == 0);
        }
        if (entry->Type == NTEntryType_StringArray) { // string array values are all null-terminated
            NTStringArray val = entry->Value.StringArray;
            for (int i = 0; i < val.Length; i++) {
                assert(val.Elements[i].Data[val.Elements[i].Length] == 0);
            }
        }
    }
    return valid;
}

NTEntry *_NTNextEntry(NTClient *client, NTEntry *it) {
    for (int i = it->_Index + 1; i < client->NumEntries; i++) {
        NTEntry *entry = &client->Entries[i];
        if (NTEntryIsValid(entry)) {
            assert(entry == &client->Entries[entry->_Index]); // entry index is correct
            return entry;
        }
    }
    return NULL;
}

#endif // NETWORKTABLES_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // NETWORKTABLES_H
