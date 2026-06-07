#include <winsock2.h>

#include "Minhook.h"
#include "fkhook.h"
#include "platform.h"
#include "sockaddrin.h"
#include "fakeaddrpool.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <iphlpapi.h>
#include <map>
#include <string>
#include <unordered_map>
#include <ws2tcpip.h>

// #define DEBUG_ENABLE
#define LOG_ENABLE

static const ULONG FAKE_IP_RANGE = 100;
static const IN_ADDR FAKE_IP_START = {127, 0, 127, 1};

static std::unordered_map<SOCKET, SOCKET> socks;
static FakeAddrPool fake_addr_pool(FAKE_IP_START, FAKE_IP_RANGE);
static char address[256] = "255.255.255.255";

typedef int WINAPI(*sendto_func) (SOCKET, const char *, int, int, const sockaddr *, int);
typedef int WINAPI(*select_func) (int, fd_set *, fd_set *, fd_set *, const TIMEVAL *);
typedef int WINAPI(*recvfrom_func) (SOCKET, char *, int, int, sockaddr *, int *);
typedef int WINAPI(*closesocket_func) (SOCKET);

static sendto_func _sendto = NULL;
static select_func _select = NULL;
static recvfrom_func _recvfrom = NULL;
static closesocket_func _closesocket = NULL;

#ifdef DEBUG_ENABLE
static void write_debug_impl(const char *fmt, ...)
{
    FILE *fp = fopen("kskbl-debug.txt", "a+");
    if (fp) {
        time_t now = time(0);
        struct tm *timeinfo = localtime(&now);
        char timestamp[20];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
        fprintf(fp, "[%s] ", timestamp);
        va_list args;
        va_start(args, fmt);
        vfprintf(fp, fmt, args);
        va_end(args);
        fclose(fp);
    }
}
#define write_debug(fmt, ...) write_debug_impl(fmt, __VA_ARGS__)
#else
#define write_debug(fmt, ...)
#endif

#ifdef LOG_ENABLE
static void write_log_impl(const char *fmt, ...)
{
    FILE *fp = fopen("kskbl-log.txt", "a+");
    if (fp) {
        time_t now = time(0);
        struct tm *timeinfo = localtime(&now);
        char timestamp[20];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
        fprintf(fp, "[%s] ", timestamp);
        va_list args;
        va_start(args, fmt);
        vfprintf(fp, fmt, args);
        va_end(args);
        fclose(fp);
    }
}
#define write_log(fmt, ...) write_log_impl(fmt, __VA_ARGS__)
#else
#define write_log(fmt, ...)
#endif

static void read_config()
{
    FILE *fp = fopen("kskbl-config.txt", "r");
    if (fp) {
        fscanf(fp, "%s", address);
    } else {
        fp = fopen("kskbl-config.txt", "w+");
        if (fp) {
            fprintf(fp, "%s", address);
        }
    }
    fclose(fp);
}

in_addr get_broadcast_ip(const struct in_addr& ip) {
    in_addr broadcast_ip = {0};
    IP_ADAPTER_INFO adapterInfo[32];
    PIP_ADAPTER_INFO pAdapterInfo = adapterInfo;
    ULONG ulOutBufLen = sizeof(adapterInfo);

    DWORD dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        pAdapterInfo = (IP_ADAPTER_INFO *) malloc(ulOutBufLen);
        dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
    }

    if (dwRetVal != NO_ERROR) {
        write_log("get adapters info failed: %d\n", dwRetVal);
        return broadcast_ip;
    }

    for (PIP_ADAPTER_INFO pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next) {
        ULONG adpt_ip = inet_addr(pAdapter->IpAddressList.IpAddress.String);
        ULONG mask = inet_addr(pAdapter->IpAddressList.IpMask.String);
        if (adpt_ip == INADDR_ANY || mask == INADDR_ANY) {
            continue;
        }
        if ((ip.S_un.S_addr & mask) == (adpt_ip & mask)) {
            broadcast_ip.S_un.S_addr = ip.S_un.S_addr | ~mask;
            return broadcast_ip;
        }
    }

    if (pAdapterInfo != adapterInfo) {
        free(pAdapterInfo);
    }

    return broadcast_ip;
}

bool is_broadcast_ip(const struct in_addr& ip) {
    if (ip.S_un.S_addr == INADDR_BROADCAST) return true;
    in_addr broadcast_ip = get_broadcast_ip(ip);
    if (ip.S_un.S_addr == broadcast_ip.S_un.S_addr) return true;
    return false;
}

static int WINAPI fake_sendto(SOCKET s, const char *buf, int len, int flags, const sockaddr *to, int tolen)
{
    sockaddr_in *origin_to = (sockaddr_in *)to;

    if (is_broadcast_ip(origin_to->sin_addr)) {
        sockaddr_in new_to;
        memset(&new_to, 0, sizeof(new_to));
        new_to.sin_family = AF_INET;
        new_to.sin_port = origin_to->sin_port;
        new_to.sin_addr.s_addr = inet_addr(address);
        if (new_to.sin_addr.s_addr == INADDR_NONE) {
            write_log("parse address failed while sendto: %s\n", address);
            goto fallback;
        }

        int result = _sendto(s, buf, len, flags, (sockaddr *)&new_to, sizeof(sockaddr_in));
        if (result == SOCKET_ERROR) {
            int errorcode = WSAGetLastError();
            write_log("redirect sendto failed: %s -> %s, %d, errorcode: %d\n", std::to_string(*origin_to).c_str(), std::to_string(new_to).c_str(), s, errorcode);
            goto fallback;
        }
        write_debug("redirect sendto: %s -> %s, %d, result: %d\n", std::to_string(*origin_to).c_str(), std::to_string(new_to).c_str(), s, result);
        return result;
    }

fallback:
    int result = _sendto(s, buf, len, flags, to, tolen);
    if (result == SOCKET_ERROR) {
        int errorcode = WSAGetLastError();
        write_log("original sendto failed: %s, %d, errorcode: %d\n", std::to_string(*origin_to).c_str(), s, errorcode);
    }
    return result;
}

static int WINAPI fake_recvfrom(SOCKET s, char *buf, int len, int flags, sockaddr *from, int *fromlen)
{
    int result = _recvfrom(s, buf, len, flags, from, fromlen);
    if (result == SOCKET_ERROR) {
        int errorcode = WSAGetLastError();
        if (errorcode != WSAEWOULDBLOCK) write_log("recvfrom failed: %d, errorcode: %d\n", s, errorcode);
        return result;
    }

    sockaddr_in *origin_from = (sockaddr_in *)from;
    sockaddr_in fake_from = fake_addr_pool.get_fake_addr(origin_from);
    fake_from.sin_port = origin_from->sin_port;
    memcpy(from, &fake_from, sizeof(sockaddr_in));
    *fromlen = sizeof(sockaddr_in);
    return result;
}

static int WINAPI fake_closesocket(SOCKET s)
{
    if (socks.count(s)) {
        _closesocket(socks[s]);
        socks.erase(s);
    }
    return _closesocket(s);
}

static int WINAPI fake_select(int n, fd_set *rd, fd_set *wr, fd_set *ex, const TIMEVAL *timeout)
{
    if (rd && rd->fd_count == 1) {
        SOCKET s = rd->fd_array[0];
        if (socks.count(s)) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(socks[s], &fds);
            int r = _select(0, &fds, NULL, NULL, timeout);
            if (r > 0) {
                return fds.fd_count;
            }
            rd->fd_count = 0;
            return 0;
        }
    }
    return _select(n, rd, wr, ex, timeout);
}

template <typename T>
void hook_func(const char *func_name, T* new_func, T *&p_old_func) {
    HMODULE hModule = GetModuleHandleA("ws2_32.dll");
    auto handler = GetProcAddress(hModule, func_name);
    MH_STATUS status = MH_CreateHook(reinterpret_cast<LPVOID*>(handler), reinterpret_cast<LPVOID*>(new_func), reinterpret_cast<LPVOID*>(&p_old_func));
    if (status != MH_OK) {
        write_log("hook %s failed: %d\n", func_name, status);
    }
    status = MH_EnableHook(reinterpret_cast<LPVOID*>(handler));
    if (status != MH_OK) {
        write_log("enable %s hook failed: %d\n", func_name, status);
    }
}

void unhook_func(const char *func_name) {
    HMODULE hModule = GetModuleHandleA("ws2_32.dll");
    auto handler = GetProcAddress(hModule, func_name);
    MH_STATUS status = MH_DisableHook(reinterpret_cast<LPVOID*>(handler));
    if (status != MH_OK) {
        write_log("disable %s hook failed: %d\n", func_name, status);
    }
    status = MH_RemoveHook(reinterpret_cast<LPVOID*>(handler));
    if (status != MH_OK) {
        write_log("remove hook %s failed: %d\n", func_name, status);
    }
}

void init_hooklib()
{
    static std::once_flag once_flag;
    std::call_once(once_flag, []() {
        MH_STATUS status = MH_Initialize();
        if (status != MH_OK) {
            write_log("init hooklib failed: %d\n", status);
        }
    });
}

void uninit_hooklib()
{
    MH_STATUS status = MH_Uninitialize();
    if (status != MH_OK) {
        write_log("uninit hooklib failed: %d\n", status);
    }
}

void hook()
{
    init_hooklib();
    read_config();
    hook_func("sendto", fake_sendto, _sendto);
    hook_func("select", fake_select, _select);
    hook_func("recvfrom", fake_recvfrom, _recvfrom);
    hook_func("closesocket", fake_closesocket, _closesocket);
}

void unhook()
{
    for (auto it = socks.begin(); it != socks.end(); it++) {
        _closesocket(it->second);
    }
    unhook_func("sendto");
    unhook_func("select");
    unhook_func("recvfrom");
    unhook_func("closesocket");
    uninit_hooklib();
}
