# DDCL.cpp  

This is the MD file of the technical documentation of `../DDCL.cpp`  
This will take me so long to write...  

## ToC  

- [Header](#ddclcpp)
  - [ToC](#toc)
  - [Preprocessors](#preprocessors)
    - [Includes and Libraries](#includes-and-libraries)
    - [Macros and Definitiosn](#macros-and-definitions)
  - [Functions](#function-descriptions)
    - [can_write_file_dir](#bool-can_write_file_dirconst-stdfilesystempath-dirpath)
    - [ensure_wsa_initialized](#bool-ensure_wsa_initialized)
    - [to_wide](#stdwstring-to_wideconst-char-s)
    - [wstring_to_utf8_string](#stdstring-wstring_to_utf8_stringconst-wchar_t-wstr)
    - [ensure_default_conf](#void-ensure_default_confconst-stdfilesystempath-path)
    - [find_vpn_by_interface_name](#stdoptionalvpnconnection-find_vpn_by_interface_name)
    - [get_active_vpn](#stdoptionalvpnconnection-get_active_vpn)
    - [EnableVirtualTerminal](#bool-enablevirtualterminal)
    - [get_timestamp](#stdstring-get_timestamp)
    - [is_drive_ready](#bool-is_drive_readychar-drive)
    - [udp_dns_test](#bool-udp_dns_testconst-char-dns_ip)
    - [http_head_probe](#bool-http_head_probeconst-char-url_cstr-dword-timeout_ms)
    - [has_internet](#bool-has_internetconst-char-check_ip-const-char-dns_resolver)
    - [resolve_hostname](#stdvectorbool-resolve_hostnameconst-stdstring-hostname-const-stdstring-dns_server_ip)
    - [GetEthernetNetworkInfo](#stdarrayconst-char-2-getethernetnetworkinfo)
    - [is_unc_available](#bool-is_unc_availableconst-char-unc)
    - [get_exe_dir](#stdfilesystempath-get_exe_dir)
    - [cstr_equal](#bool-cstr_equalconst-char-a-const-char-b)
    - [get_safe_filename_timestamp](#stdstring-get_safe_filename_timestamp)
    - [ensure_log_location](#void-ensure_log_location)
    - [write_to_log](#void-write_to_logconst-stdstring-message)
    - [log_change](#void-log_changeconst-status_change_type-diff-stdstring-time-void-info)
    - [status_check](#void-status_checkbool-nowrite)
    - [update_status](#void-update_status)
    - [initial_status_write](#stdsize_t-initial_status_write)
    - [initialize_runtime](#void-initialize_runtime)
    - [print_config_summary](#void-print_config_summary)
    - [main](#int-mainint-argc-char-argv)

## Preprocessors  

### Includes and Libraries

DDCL has the following includes and lib comments:  

```cpp
#include <cstddef>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <memory>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <thread>
#include "toml.hpp" // https://github.com/marzer/tomlplusplus/blob/v3.4.0/toml.hpp - Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
#include <vector>
#include <future>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wininet.h>
#include <iphlpapi.h>
#include <ras.h>
#include <regex>
#include <windns.h>
#include <stdexcept>

#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "rasapi32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "user32.lib")
```

These are from the Windows API:

```cpp
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wininet.h>
#include <iphlpapi.h>
#include <ras.h>
#include <windns.h>

#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "rasapi32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "user32.lib")
```

And

```cpp
#include <filesystem>
#include <regex>
#include <thread>
#include <future>
#include <chrono>
#include <atomic>
```

are also Windows-specific versions.  

These are used for network tests, devices, file management, threading and process handling.  
I did not write the use of WinAPI myself, it sucks so bad.  
Used in the following functions:  

```cpp
//? Files and system
static bool can_write_file_dir(const std::filesystem::path& dirPath)
static void ensure_default_conf(const std::filesystem::path& path)
static bool is_drive_ready(char drive)
static std::array<const char*, 2> GetEthernetNetworkInfo()
static bool is_unc_available(const char* unc)
static std::filesystem::path get_exe_dir()

//? Network related
static inline bool ensure_wsa_initialized()
static std::optional<VpnConnection> find_vpn_by_interface_name()
static std::optional<VpnConnection> get_active_vpn()
static bool udp_dns_test(const char* dns_ip)
static bool http_head_probe(const char* url_cstr, DWORD timeout_ms)
static bool has_internet(const char* check_ip, const char* dns_resolver)
static std::vector<bool> resolve_hostname(const std::string& hostname, const std::string& dns_server_ip)

//? Output
static bool EnableVirtualTerminal()
static std::string get_timestamp()
static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
int main(int argc, char* argv[])
```

Everything else is standard universal C++.

### Macros and Definitions

DDCL has the following definitions for ease of use and compatibility for compiling on differing setups.  

```cpp
#ifndef ERROR_BUFFER_TOO_SMALL
#define ERROR_BUFFER_TOO_SMALL 603
#endif

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET 0x9800000C
#endif

#ifndef DNS_MAX_SOCKADDR_LENGTH
#define DNS_MAX_SOCKADDR_LENGTH 128
#endif

#ifdef __MINGW32__ 
typedef IP4_ARRAY *PIP4_ARRAY;

typedef struct _DNS_ADDR {
CHAR MaxSa[DNS_MAX_SOCKADDR_LENGTH];
DWORD DnsAddrUserDword[8];
} DNS_ADDR, *PDNS_ADDR;

typedef struct _DNS_ADDR_ARRAY {
    DWORD MaxCount;
    DWORD AddrCount;
    DNS_ADDR AddrArray[ANYSIZE_ARRAY];
} DNS_ADDR_ARRAY, *PDNS_ADDR_ARRAY;

typedef enum _DNS_QUERY_REQUEST_VERSION {
    DNS_QUERY_REQUEST_VERSION_1 = 0x1,
    DNS_QUERY_REQUEST_VERSION_2 = 0x2
} DNS_QUERY_REQUEST_VERSION;

typedef struct _DNS_QUERY_REQUEST {
    DNS_QUERY_REQUEST_VERSION Version;
    DWORD                     Options;
    PWSTR                     QueryName;
    WORD                      QueryType;
    WORD                      QueryClass;
    union {
        PIP4_ARRAY             pIp4AddrArray;
        PDNS_ADDR_ARRAY        pDnsServerList;
        PWSTR                  pQueryString;
    };
} DNS_QUERY_REQUEST, *PDNS_QUERY_REQUEST;
#endif

#define RED     "\x1B[31m"
#define GREEN   "\x1B[32m"
#define YELLOW  "\x1B[33m"
#define BLUE    "\x1B[34m"
#define MAGENTA "\x1B[35m"
#define CYAN    "\x1B[36m"
#define WHITE   "\x1B[37m"
#define RESET   "\x1B[0m"
#define BOLD    "\x1B[1m"
#define CLEAR   "\x1B[2J\x1B[H"
```

This is in two segments:  
\- Supporting preprocessor blocks for compilation  
\- Ease of use macros for ANSI/VT100 escape codes  

The first part has definitions for things SDKs might not have defined as well as a lot of definitions intended for cross-compiling from linux with MSYS2/MingGW64:  
This includes network-related types and static values.

The second part defines standard escape codes which are used in everything that writes to the terminal.

## Function descriptions  

### `bool can_write_file_dir(const std::filesystem::path& dirPath)`

Checks if a directory is writable by creating and immediately deleting a temporary test file "write_test.tmp" inside it.

Writes "hiiiii >w<" to the test file; uses `std::filesystem::remove()` which silently fails if file doesn't exist.

### `bool ensure_wsa_initialized()`

Ensures Windows Sockets (Winsock) API version 2.2 is initialized using `WSAStartup()`, required for all network operations; uses static initialization to run only once.

Returns `false` immediately if prior initialization failed; no cleanup on failure.

### `std::wstring to_wide(const char* s)`

Converts UTF-8 `const char*` to `std::wstring` using `MultiByteToWideChar(CP_UTF8)`.

Ignores null terminator in output length (-1 adjustment); returns empty on null input or conversion failure.

### `std::string wstring_to_utf8_string(const wchar_t* wstr)`

Converts wide string to UTF-8 `std::string` using `WideCharToMultiByte(CP_UTF8)`.

Similar to `to_wide()` but reverse direction; early exit on null/empty input.

### `void ensure_default_conf(const std::filesystem::path& path)`

Creates a default TOML config file at `path` with embedded `DEFAULT_CONF` if it doesn't exist, then exits(0) after 5-second sleep.

Prints message and sleeps to let the user know to edit before exit.

### `std::optional<VpnConnection> find_vpn_by_interface_name()`

Scans network adapters via `GetAdaptersAddresses()` for active ("Up") interfaces with "vpn" in name/description (case-insensitive, skips loopback).

Extracts first IPv4 from unicast addresses; falls back hostname to DNS suffix, description, or "VPN_Interface".

### `std::optional<VpnConnection> get_active_vpn()`

Detects active VPN connections via `RasEnumConnectionsA` (legacy RAS), then interface scan for IKEv2/SSTP/etc. (PPP type 131, "RasSstp", etc.).

Fills `VpnConnection{name, hostname, local_ip}` from RAS entry/device fields or adapter details; chains to `find_vpn_by_interface_name()` as fallback.

### `bool EnableVirtualTerminal()`

Enables ANSI/VT100 escape sequences and UTF-8 in console via `SetConsoleMode(ENABLE_VIRTUAL_TERMINAL_PROCESSING)` and codepage settings.

Fails gracefully on old Windows (<Win10); used for colored output.

### `std::string get_timestamp()`

Generates formatted local timestamp string ("%d.%m.%Y-%H:%M:%S") using `std::put_time` with system locale.

Disables deprecated warning for `localtime()`; used everywhere for logs/UI.

### `bool is_drive_ready(char drive)`

Checks if a drive letter (e.g., 'C') exists and is ready via `GetDriveTypeA()` on root path.

Returns `false` for `DRIVE_NO_ROOT_DIR` or `DRIVE_UNKNOWN`.

### `bool udp_dns_test(const char* dns_ip)`

Sends minimal UDP DNS query (A record header only) to port 53; async `select()` with 250ms timeout to detect response.

Disables UDP `SIO_UDP_CONNRESET`; tests reachability to DNS servers like 1.1.1.1.

### `bool http_head_probe(const char* url_cstr, DWORD timeout_ms)`

Sends HTTP HEAD request via WinINet (`InternetOpen/Connect/HttpOpenRequest`) with custom timeouts; checks 2xx/3xx status.

Handles HTTPS; cracks URL components; no-cache flags; fallback for `InternetCheckConnectionW` failures.

### `bool has_internet(const char* check_ip, const char* dns_resolver)`

Multi-probe internet: `InternetCheckConnectionW` → HTTP HEAD → UDP DNS to 1.1.1.1/8.8.8.8/resolver.

Prepends "http://" if missing; handles captive portals/proxies.

### `std::vector<bool> resolve_hostname(const std::string& hostname, const std::string& dns_server_ip)`

Async parallel DNS lookups (250ms each): custom `DnsQuery_A` via `DNS_QUERY_REQUEST` (specified server) + `getaddrinfo` (system resolvers).

Returns `{custom_success, system_success}`; frees results to avoid leaks.

### `std::array<const char*, 2> GetEthernetNetworkInfo()`

Finds first active Ethernet (`IF_TYPE_ETHERNET_CSMACD`, `IfOperStatusUp`) adapter via `GetAdaptersAddresses()`; returns `{friendly_name_cstr, dns_suffix_cstr}`.

Uses static string storage for returned C-strings (persistent pointers).

### `bool is_unc_available(const char* unc)`

Async `GetFileAttributesA()` checks if UNC path is directory (`FILE_ATTRIBUTE_DIRECTORY`); 250ms timeout.

Used for SMB/NAS shares.

### `std::filesystem::path get_exe_dir()`

Returns directory of current executable via `GetModuleFileNameW()`.

Truncates at last `\`; empty on failure.

### `bool cstr_equal(const char* a, const char* b)`

Safe strcmp wrapper: handles nulls, same-pointer optimization.

Used for `curr_eth_info` comparisons.

### `std::string get_safe_filename_timestamp()`

Like `get_timestamp()` but replaces `: / \` with `_` for Windows filenames.

Used in log paths.

### `void ensure_log_location()`

Sets `log_path` via config → `%LOCALAPPDATA%` → OneDrive → user profile → exe dir; appends "DDCL-Logs/DDCL_log-TIMESTAMP.csv"; creates dirs.

Disables logging on permission/path errors after fallback attempts.

### `void write_to_log(const std::string& message)`

Appends message + "\r\n" to `log_path` (binary/append mode); flushes and explicit close.

Prints `GetLastError()` on failures; exception-safe.

### `void log_change(const status_change_type diff, std::string time, void* info)`

Formats CSV log entries by `diff` type (internet/ethernet/etc.); calls `write_to_log()`.

Uses void* casting macros for passing indexes/strings through info; throws on invalid args.

### `void status_check(bool nowrite)`

Compares `curr_*` vs `prev_*` globals; calls `log_change()` on differences (skips if `nowrite`).

Updates `prev_*` at end; logs once per DNS/ethernet changes; nowrite disables calling `log_change`

### `void update_status()`

Refreshes all `curr_*` globals: internet/DNS/ethernet/VPN/drives/UNC via respective functions.

Resizes vectors; validates/skips invalid drive letters; expands `$username` in UNC at config time.

### `std::size_t initial_status_write()`

Initial `update_status()` + CSV header + logs all current statuses as "initial_status".

Sleeps 250ms post-header; returns 1 on success.

### `void initialize_runtime()`

Sets up globals/config parsing (`toml::parse_file`); expands UNC `$username`; enables VT/handler; `ensure_log_location()`.

Populates `detection_kinds` from config or defaults all.

### `void print_config_summary()`

Prints colored summary of config/detections/log path/VT status/network/disks after parsing.

Used for `--config` flag; shows UNC importance colors.

### `int main(int argc, char* argv[])`

Parses command line arguments (`--config`, `-c`) and sets up program execution flow.

Either prints config summary and exits (config flag), ensures default config and exits (no config), or enters main monitoring loop with `initialize_runtime()`, initial status write, and 1-second timed `update_status()` + `status_check()` cycles; handles console Ctrl+C cleanup.

Handles Windows console setup (VT/UTF-8), config validation, logging initialization, and graceful shutdown with status preservation.
