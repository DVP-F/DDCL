// Copyright (c) DVP-F/Carnx00 2026 <carnx@duck.com>
// License : GNU GPL 3.0 (https://www.gnu.org/licenses/gpl-3.0.html) supplied with the package under `LICENSES`
// Source code hosts:
// - GitHub: https://github.com/DVP-F/DDCL 
// See `NOTICE.txt` for further Licensing information

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <cstddef>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <memory>
#include <mutex>
// #include <limits>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <userenv.h>
#include <thread>
#include "toml.hpp" // https://github.com/marzer/tomlplusplus/blob/v3.4.0/toml.hpp - Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
#include <vector>
#include <future>
#include <wlanapi.h>
#include <objbase.h>
#include <wtsapi32.h>
#include <optional>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wininet.h>
#include <iphlpapi.h>
#include <ras.h>
#include <regex>
#include <windns.h>
#include <stdexcept>


#pragma comment(lib, "user32.lib")
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "rasapi32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wininet.lib")

using namespace std;

// Some SDKs or toolchains may not define ERROR_BUFFER_TOO_SMALL (value 603).
#ifndef ERROR_BUFFER_TOO_SMALL
#define ERROR_BUFFER_TOO_SMALL 603
#endif

// another one that may be missing in some SDKs - defined for winsock2.h
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET 0x9800000C
#endif

// probably generally defined, but not always in all SDKs
#ifndef DNS_MAX_SOCKADDR_LENGTH
#define DNS_MAX_SOCKADDR_LENGTH 128
#endif

// ANSI escape codes for use in VTP (Virtual Terminal Processing)
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
#define NOWRAP  "\x1B[?7l"
#define WRAP    "\x1B[?7h"

static bool show_config = false;
static bool show_help = false;
static bool set_maxRuns = false;

#define VERSION "0.2.0"

// toml config default
constexpr const char* DEFAULT_CONF = R"(
[Meta]
# Location for logs. Overrides the default path prioritization. "\DDCL-Logs\" is appended. If unset, undefined, or empty, uses default.
# Supports paths relative to the executable and absolute paths. Absolute paths must be prefixed with a drive letter (C:\).
log_path = ""
# Enable or disable the use of wincrap's virtual terminal processing for colored output.
# Disabling this will make output unicolored and will fail proper alignment of output. Falls back to false if the terminal does not support it. (Older versions < W10)
use_virtual_terminal = true
# Which checks to perform. Overrides any config below. Ordering is irrelevant.
detections = [
	"Internet",
	"Ethernet",
	"WLAN",
	"DNS resolution",
	"VPN",
	"Disks",
	"UNC",
]

[Network]
# network checks are performed with a timeout of 600ms in the unmodified source code - beware of latency
# check is the ip (or hostname) used to verify if an internet connection is present
check = "1.1.1.1"
# optional dns override (outside of the lan prefferable). DNS resolution attempts resolving `www.wikipedia.org` and [check]
# defaults to system NS config (of whatever network is active). may therefore time out
dns = ""
# expected domain (Ethernet DNS Suffix) is optional, just used to check if the network domain is or is not as expected. used if you have a domain :)
expected_domain = ""
# expected VPN hostname is optional and does not affect normal function. the app might not be able to get the hostname
# case-insensitive regex match in ECMAScript syntax - double backslashes, no need for /regex/ delimiting, and dont use ^$ flags
expected_vpn_hostname = ""

[Disks]
# mounted disks with drive letters; Local direct-attached storage or SAN storage, not network shares.
# add labels by prefixing the drive letter with `<label>#`
# modify the pound `#` to be `#!` to mark a disk as important
locals = [
	"home#!C",
]
# unc paths are network resources to check eg. smb shares etc. (Including NAS storage) 
# quad backslashes per single backslash in the final path due to parsing by C++ string literal and toml parser.
unc = [	
	# add labels by prefixing the path with `<label>#`
	# modify the pound `#` to be `#!` to mark as important
	# localhosts can be used to test the virtual loopback adapter too - if the path is a shared folder or otherwise available. 
	"#!\\\\localhost\\C",
	# add `$username` to use the current user's username in a path, for example to check the user's home directory 
	# or `$userdomain` to use the current user's dns domain name
	#  !!  (UNC only; intended for AD/AAD where a home folder is set up on a file server)  !!
	# obligatory reminder that this is string expansion, not parametrized queries. ensure the envvars %USERNAME% and %USERDNSDOMAIN% are safe.
	"userHome#\\\\localhost\\Users\\$username",
	# this kind of path (C:\Users\*) is natively language-agnostic - `Users` points the same place no matter the syslocale.
]
)";

static bool can_write_file_dir(const std::filesystem::path& dirPath) {
	// self explanatory - checks if a directory is writable by attempting to create and delete a temporary file.
	namespace fs = std::filesystem;
	fs::path testPath = fs::path(dirPath) / "write_test.tmp";
	std::ofstream file(testPath);
	if (!file.is_open())
		return false;
	file << "hiiiii >w<\n";
	file.close();
	return fs::remove(testPath);
};

static inline bool ensure_wsa_initialized() {
	// WINSOCK is required for most network checks and DNS queries, so we need to ensure 
	// it's initialized before we do any of that.
	static WSADATA wsaData = {};
	static bool initialized = false;
	if (!initialized) {
		int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
		initialized = (result == 0);
	}
	return initialized;
}

static std::wstring to_wide(const char* s) {
	// Converter used extensively, just one string format to another. move along, nothing to see here.
	if (!s) return {};
	int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
	if (len <= 0) return {};
	std::wstring ws(len - 1, L'\0');          // -1: ignore null
	MultiByteToWideChar(CP_UTF8, 0, s, -1, ws.data(), len);
	return ws;
}

static std::string wstring_to_utf8_string(const wchar_t* wstr) {
	// Converter used extensively, just one string format to another. move along, nothing to see here.
	if (!wstr || !*wstr) return {};
	int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
	if (len <= 0) return {};
	std::string result(len - 1, 0);  // -1 excludes null terminator
	WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], len, nullptr, nullptr);
	return result;
}

static void ensure_default_conf(const std::filesystem::path& path) {
	// if the config file already exists, do nothing. otherwise, 
	// create a new one with default settings and exit to allow user to edit it.
	if (std::filesystem::exists(path)) return;
	std::cout << "No conf.toml found, creating default at " << path << std::endl;
	std::ofstream out(path, std::ios::binary);
	if (!out) {
		std::cerr << "Failed to create " << path << std::endl;
		return;
	}
	out << DEFAULT_CONF;
	std::cout << "Default conf.toml created. Please edit it with your desired settings and restart the program.\n";
	std::this_thread::sleep_for(std::chrono::seconds(5));
	exit(0);
}

struct NetworkConfig {
	std::string check;
	std::string dns;
	std::string expected_domain;
	std::string expected_vpn_hostname;
};

struct DiskConfig {
	std::vector<std::string> locals;
	std::vector<std::string> locals_labels;
	std::vector<int> locals_imp;
	std::vector<std::string> unc;
	std::vector<std::string> unc_labels;
	std::vector<int> unc_imp;
};

struct VpnConnection {
	std::string name;
	std::string hostname;  // "nlfree35.protonvpn.com"
	std::string local_ip;
	bool connected = false;
};

// the following all use winapi calls - describing them and why theyre performed is,,, horrible. 
// just know these calls barely work bc thats how winapi rolls.

static std::optional<VpnConnection> find_vpn_by_interface_name() {
	ULONG flags = GAA_FLAG_INCLUDE_ALL_INTERFACES;
	ULONG family = AF_UNSPEC;
	ULONG bufLen = 0;
	DWORD result = GetAdaptersAddresses(family, flags, nullptr, nullptr, &bufLen);
	if (result != ERROR_BUFFER_OVERFLOW && result != ERROR_NO_DATA) {  // Allow empty
		return std::nullopt;
	}
	auto buffer = std::make_unique<BYTE[]>(bufLen);
	PIP_ADAPTER_ADDRESSES addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.get());
	result = GetAdaptersAddresses(family, flags, nullptr, addrs, &bufLen);
	if (result != NO_ERROR && result != ERROR_NO_DATA) {  // Allow empty
		return std::nullopt;
	}
	for (auto p = addrs; p != nullptr; p = p->Next) {
		if (p->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue; // explicitly skip loopback interfaces
		if (p->OperStatus != IfOperStatusUp) continue;
		std::string nameUtf8 = p->FriendlyName ? wstring_to_utf8_string(p->FriendlyName) : "";
		std::string descUtf8 = p->Description ? wstring_to_utf8_string(p->Description) : "";
		std::string hostUtf8 = (p->DnsSuffix != nullptr) ? wstring_to_utf8_string(p->DnsSuffix) : "";
		// Detect "VPN-ish" adapters via type instead of fragile string matching
		bool looksLikeVpn =
			p->IfType == IF_TYPE_PPP ||       // PPP
			p->IfType == 131 ||               // IKEv2
			strstr(p->AdapterName, "Ras") ||
			strstr(p->AdapterName, "VPN") ||
			strstr(p->AdapterName, "tun") ||
			strstr(p->AdapterName, "tap");
		if (!looksLikeVpn) continue;
		// Get first valid IPv4
		for (auto ua = p->FirstUnicastAddress; ua; ua = ua->Next) {
			if (ua->Address.lpSockaddr &&
				ua->Address.lpSockaddr->sa_family == AF_INET) {
				sockaddr_in* ip = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
				char ipStr[INET_ADDRSTRLEN] = {};
				if (!inet_ntop(AF_INET, &ip->sin_addr, ipStr, sizeof(ipStr)))
					continue;
				VpnConnection vpn;
				vpn.connected = true;
				// Name
				vpn.name = !nameUtf8.empty() ? nameUtf8 : p->AdapterName;
				// Hostname (THIS is the important bit)
				if      (!hostUtf8.empty()) vpn.hostname = hostUtf8;   // <- real server (when available)
				else if (!descUtf8.empty()) vpn.hostname = descUtf8;
				else if (!nameUtf8.empty()) vpn.hostname = nameUtf8;
				else                        vpn.hostname = "VPN_Interface";
				vpn.local_ip = ipStr;
				return vpn;
			}
		}
	}
	return std::nullopt;
}

static std::optional<VpnConnection> get_active_vpn() {
	// FIRST: try interface scan
	auto vpn = find_vpn_by_interface_name();
	if (vpn != std::nullopt) return vpn;
	DWORD dwConnections = 0; DWORD dwSize = 0;
	DWORD firstResult = RasEnumConnectionsA(NULL, &dwSize, &dwConnections);
	// ignore errors. we ball. 
	std::vector<BYTE> buffer(dwSize);
	auto* connections = reinterpret_cast<RASCONNA*>(buffer.data());
	if (connections != nullptr && dwSize >= sizeof(RASCONNA)) {
		connections[0].dwSize = sizeof(RASCONNA);  // REQUIRED!
		DWORD actualCount = 0;
		DWORD result = RasEnumConnectionsA(connections, &dwSize, &actualCount);
		if (result == 0 && actualCount > 0) {
			HRASCONN hRasConn = connections[0].hrasconn;
			RASCONNSTATUSA status;
			status.dwSize = sizeof(RASCONNSTATUS);
			if (RasGetConnectStatusA(hRasConn, &status) <= 0) {
				if (status.rasconnstate <= RASCS_Connected) {
					VpnConnection vpn;
					vpn.connected = true;
					// These fields are often garbage — only use if non-empty
					if (connections[0].szEntryName[0] != '\0')
						vpn.name = connections[0].szEntryName;
					if (status.szDeviceName[0] != '\0')
						vpn.hostname = status.szDeviceName;
					// If both are still empty → bail (don't return junk)
					if (vpn.name.empty() && vpn.hostname.empty()) {
						// fall through to nullopt
					} else {
						return vpn;
					}
				}
			}
		}
	}
	// LAST fallback
	ULONG bufSize = 0;
	GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_ALL_INTERFACES | GAA_FLAG_SKIP_DNS_SERVER, NULL, NULL, &bufSize);
	PIP_ADAPTER_ADDRESSES pAddrs = (PIP_ADAPTER_ADDRESSES)malloc(bufSize);
	if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_ALL_INTERFACES | GAA_FLAG_SKIP_DNS_SERVER, NULL, pAddrs, &bufSize) == NO_ERROR) {
		for (PIP_ADAPTER_ADDRESSES p = pAddrs; p; p = p->Next) {
			if (p->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
			if (p->OperStatus == IfOperStatusUp && (
				p->IfType == IF_TYPE_PPP ||
				p->IfType == 131 ||
				strstr(p->AdapterName, "RasSstp") ||
				strstr(p->AdapterName, "AgileVPN") ||
				strstr(p->AdapterName, "wanarp") ||
				strstr(p->AdapterName, "RasAgileVpn")
			)) {
				for (auto ua = p->FirstUnicastAddress; ua; ua = ua->Next) {
					if (ua->Address.lpSockaddr &&
						ua->Address.lpSockaddr->sa_family == AF_INET) {
						sockaddr_in* ip = (sockaddr_in*)ua->Address.lpSockaddr;
						char ipStr[INET_ADDRSTRLEN] = {};
						if (!inet_ntop(AF_INET, &ip->sin_addr, ipStr, sizeof(ipStr)))
							continue;
						VpnConnection vpn;
						vpn.connected = true;
						vpn.name = p->FriendlyName ? wstring_to_utf8_string(p->FriendlyName) : p->AdapterName;
						vpn.local_ip = ipStr;
						std::string desc = p->Description ? wstring_to_utf8_string(p->Description) : "";
						std::string suffix = p->DnsSuffix ? wstring_to_utf8_string(p->DnsSuffix) : "";
						if      (!suffix.empty()) vpn.hostname = suffix;
						else if (!desc.empty())   vpn.hostname = desc;
						else                      vpn.hostname = "IKEv2_VPN";
						free(pAddrs);
						return vpn;
					}
				}
			}
		}
	}
	if (pAddrs) free(pAddrs);
	return std::nullopt;
}

// Enable Virtual Terminal Processing + UTF-8
static bool EnableVirtualTerminal() {
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE) return false;
	DWORD mode;
	if (!GetConsoleMode(hOut, &mode)) return false;
	mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(hOut, mode);
	// Force UTF-8 codepage
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	return true;
}

#pragma warning(disable:4996) // not multithreaded - zero risk, dont care.
// Get formatted timestamp
static std::string get_timestamp() {
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	std::stringstream ss;
	ss.imbue(std::locale("")); // sys locale
	ss << std::put_time(std::localtime(&time_t), "%d.%m.%Y-%H:%M:%S");
	return ss.str();
}
#pragma warning(default:4996)

static void set_internet_timeout(HINTERNET handle, DWORD timeout_ms) {
    InternetSetOptionW(
        handle,
        INTERNET_OPTION_CONNECT_TIMEOUT,
        &timeout_ms,
        sizeof(timeout_ms));
    InternetSetOptionW(
        handle,
        INTERNET_OPTION_SEND_TIMEOUT,
        &timeout_ms,
        sizeof(timeout_ms));
    InternetSetOptionW(
        handle,
        INTERNET_OPTION_RECEIVE_TIMEOUT,
        &timeout_ms,
        sizeof(timeout_ms));
}

static bool is_drive_ready(char drive) {
	char root[4] = { drive, ':', '\\', 0 };
	UINT driveType = GetDriveTypeA(root);
	return driveType != DRIVE_NO_ROOT_DIR && driveType != DRIVE_UNKNOWN;
}

static bool udp_dns_test(const char* dns_ip) {
	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) return false;
	sockaddr_in sa = {};
	sa.sin_family = AF_INET;
	sa.sin_port = htons(53);
	if (inet_pton(AF_INET, dns_ip, &sa.sin_addr) != 1) {
		closesocket(s);
		return false;
	}
	uint8_t query[14] = { 0xAB,0xCD, 0x01,0x00, 0x00,0x01, 0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x01 };
	auto future = std::async(std::launch::async, [s, sa, query]() mutable -> bool {
		BOOL bNewBehavior = FALSE;
		DWORD dwBytesReturned = 0;
		WSAIoctl(s, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior),
			NULL, 0, &dwBytesReturned, NULL, NULL);
		sendto(s, reinterpret_cast<const char*>(query),
			static_cast<int>(sizeof(query)), 0,
			reinterpret_cast<const sockaddr*>(&sa), sizeof(sa));
		fd_set rf;
		FD_ZERO(&rf);
		FD_SET(s, &rf);
		timeval tv = { 0, 250000 };
		sockaddr_in from;
		int fromlen = sizeof(from);
		char buf[512];
		bool got =
			(select(static_cast<int>(s) + 1, &rf, nullptr, nullptr, &tv) > 0) &&
			(recvfrom(s, buf, sizeof(buf), 0,
				reinterpret_cast<sockaddr*>(&from), &fromlen) > 0);
		closesocket(s);
		return got;
	});

	auto future_status = future.wait_for(std::chrono::milliseconds(250));
	return (future_status == std::future_status::ready) ? future.get() : false;
}

// Lightweight HTTP HEAD probe using WinINet (fallback when InternetCheckConnection is unreliable)
static bool http_head_probe(const char* url_cstr, DWORD timeout_ms = 500) {
    if (!url_cstr || !*url_cstr)
        return false;
    std::wstring wurl = to_wide(url_cstr);
    URL_COMPONENTSW uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwHostNameLength = static_cast<DWORD>(-1);
    uc.dwUrlPathLength = static_cast<DWORD>(-1);
    if (!InternetCrackUrlW(wurl.c_str(), 0, 0, &uc))
        return false;
    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring path = uc.lpszUrlPath && uc.dwUrlPathLength ? std::wstring(uc.lpszUrlPath, uc.dwUrlPathLength) : L"/";
    const INTERNET_PORT port = uc.nPort ? uc.nPort : (uc.nScheme == INTERNET_SCHEME_HTTPS ? 443 : 80);
    DWORD flags = INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD;
    if (uc.nScheme == INTERNET_SCHEME_HTTPS)
        flags |= INTERNET_FLAG_SECURE;
    HINTERNET internet = InternetOpenW(L"DDCLProbe", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!internet)
        return false;
    set_internet_timeout(internet, timeout_ms);
    HINTERNET connection = InternetConnectW(internet, host.c_str(), port, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!connection) {
        InternetCloseHandle(internet);
        return false;
    }
    set_internet_timeout(connection, timeout_ms);
    HINTERNET request = HttpOpenRequestW(connection, L"HEAD", path.c_str(), nullptr, nullptr, nullptr, flags, 0);
    if (!request) {
        InternetCloseHandle(connection);
        InternetCloseHandle(internet);
        return false;
    }
    set_internet_timeout(request, timeout_ms);
    BOOL sent = HttpSendRequestW(request, nullptr, 0, nullptr, 0);
    bool success = false;
    if (sent) {
        DWORD status = 0;
        DWORD status_size = sizeof(status);
        if (HttpQueryInfoW(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &status_size, nullptr)) {
            success = status >= 200 && status < 400;
        }
    }
    InternetCloseHandle(request);
    InternetCloseHandle(connection);
    InternetCloseHandle(internet);
    return success;
}

static bool has_internet(const char* check_ip = "www.msftconnecttest.com", const char* dns_resolver = "9.9.9.9") {
	// Build a URL (InternetCheckConnection expects one) and derive a wide string for the WinINet API.
	std::string url = check_ip;
	if (url.find("://") == std::string::npos) url = "http://" + url;
	std::wstring wurl = to_wide(url.c_str());
	// 1) OS-level quick probe (use explicit wide API to avoid accidental ANSI/Unicode mismatches).
	if (InternetCheckConnectionW(wurl.c_str(), FLAG_ICC_FORCE_CONNECTION, 0) != FALSE)
		return true;
	// 2) Lightweight HTTP HEAD probe (handles captive portals / proxies)
	if (http_head_probe(url.c_str()))
		return true;
	// 3) UDP DNS probes to well-known resolvers (fast UDP round-trips) // avoid quad9 as it may not respond
	if (udp_dns_test("1.1.1.1") || udp_dns_test("8.8.8.8") || (dns_resolver && *dns_resolver && udp_dns_test(dns_resolver)))
		return true;
	return false;
}

static bool resolve_with_system_dns(const std::string& hostname, const std::string& dns_server_ip, std::chrono::milliseconds timeout) {
    if (hostname.empty() || dns_server_ip.empty())
        return false;
    IP4_ARRAY dns_servers{};
    dns_servers.AddrCount = 1;
    dns_servers.AddrArray[0] = inet_addr(dns_server_ip.c_str());
    DNS_RECORD* records = nullptr;
    // DnsQueryConfig has no reliable per-call timeout. Run it on a detached
    // worker and return promptly. The DNS API owns its own buffers.
    std::atomic<bool> finished{ false };
    std::atomic<bool> success{ false };
    std::thread([&]() {
        DNS_STATUS status = DnsQuery_A(
            hostname.c_str(),
            DNS_TYPE_A,
            DNS_QUERY_STANDARD | DNS_QUERY_NO_WIRE_QUERY,
            &dns_servers,
            &records,
            nullptr);
        success.store(
            status == ERROR_SUCCESS && records != nullptr,
            std::memory_order_release);
        if (records)
            DnsRecordListFree(records, DnsFreeRecordList);
        finished.store(true, std::memory_order_release);
    }).detach();
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!finished.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return finished.load(std::memory_order_acquire) && success.load(std::memory_order_acquire);
}

static std::vector<bool> resolve_hostname(const std::string& hostname, const std::string& dns_server_ip) {
    constexpr auto timeout = std::chrono::milliseconds(250);
    std::vector<bool> results(2, false);
    // Custom DNS server
    if (!dns_server_ip.empty()) {
        results[0] = resolve_with_system_dns(hostname, dns_server_ip, timeout);
    }
    // System resolver
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    auto start = std::chrono::steady_clock::now();
    int status = getaddrinfo(
        hostname.c_str(),
        nullptr,
        &hints,
        &result);
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (status == 0 &&
        result != nullptr &&
        elapsed <= timeout) {
        results[1] = true;
    }
    if (result)
        freeaddrinfo(result);
    return results;
}

struct NetworkInfo {
	// default initialization
	std::string GUID = "N/A";
	std::string FName = "N/A";
	std::string Description = "N/A";
	std::string DNSSuffix = "N/A";
    std::string WSSID = "N/A";
    std::string WName = "N/A";
    std::string WBSSID = "N/A";
    ULONG WSignalQuality = 0;
    std::string WAuthAlgo = "N/A";
    std::string WCipherAlgo = "N/A";
	std::string MAC = "N/A";
	std::string PrimaryDHCPv4 = "N/A";
	std::string PrimaryDNS = "N/A";
	std::string PrimaryGateway = "N/A";

	bool operator==(const NetworkInfo& other) const {
        return (
			GUID == other.GUID &&
			FName == other.FName &&
			Description == other.Description &&
			DNSSuffix == other.DNSSuffix &&
			MAC == other.MAC &&
			PrimaryDHCPv4 == other.PrimaryDHCPv4 &&
			PrimaryDNS == other.PrimaryDNS &&
			PrimaryGateway == other.PrimaryGateway
		);
    }

    bool operator!=(const NetworkInfo& other) const {
		// to hell with optimization this is shorter
        return !(*this == other);
    }
};

struct SessionInfo {
    std::string user = "N/A";
    std::string domain = "N/A";
    DWORD sessionId = 0;
	DWORD loggedInCount = 0;
	std::string hostname = "N/A";
	ULONGLONG upTimeSec = 0;
};

struct WifiConnectionInfo {
    std::string WSSID;
    std::string WName;
    std::string WBSSID{};
    ULONG WSignalQuality = 0;
    std::string WAuthAlgo;
    std::string WCipherAlgo;
};

struct UncProbeState {
	std::string path;
	std::mutex mutex;
	std::condition_variable cv;
	std::exception_ptr exception;
	bool result = false;
	bool ready = false;
};

static std::atomic<unsigned> g_active_unc_probes{0};

static bool is_unc_available(const char* unc) {
    if (!unc || !*unc)
        return false;

    constexpr unsigned MAX_UNC_PROBES = 5;
    constexpr auto TIMEOUT = std::chrono::milliseconds(250);
    unsigned current = g_active_unc_probes.load(std::memory_order_relaxed);

    for (;;) {
		// if 5 workers havent returned for getattributes, assume false and dont create more.
        if (current >= MAX_UNC_PROBES)
            return false;
        if (g_active_unc_probes.compare_exchange_weak(
                current,
                current + 1,
                std::memory_order_acquire,
                std::memory_order_relaxed)) {
            break;
        }
    }

    auto state = std::make_shared<UncProbeState>();
    state->path = unc;

    std::thread worker([state]() {
        struct ProbeGuard {
            ~ProbeGuard() {
                g_active_unc_probes.fetch_sub(
                    1,
                    std::memory_order_release);
            }
        } guard;
        try {
            DWORD attr = GetFileAttributesA(state->path.c_str());
            {
                std::lock_guard lock(state->mutex);
                state->result =
                    attr != INVALID_FILE_ATTRIBUTES &&
                    (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
                state->ready = true;
            }
        }
        catch (...) {
            {
                std::lock_guard lock(state->mutex);
                state->exception = std::current_exception();
                state->ready = true;
            }
        }
        state->cv.notify_one();
    });

    {
        std::unique_lock lock(state->mutex);
        bool completed = state->cv.wait_for(
            lock,
            TIMEOUT,
            [&state]() {
                return state->ready;
            });
        if (!completed) {
            worker.detach();
            return false;
        }
    }

    worker.join();
    if (state->exception)
        std::rethrow_exception(state->exception);
    return state->result;
}

static std::filesystem::path get_exe_dir() {
	// hard to explain beyond that it returns the dir of the executable
	wchar_t buf[MAX_PATH];
	DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
	if (len == 0 || len == MAX_PATH) return std::filesystem::path();
	std::wstring path(buf, len);
	std::size_t pos = path.find_last_of(L"\\");
	path.resize(pos);
	return std::filesystem::path(path);
}

static inline bool cstr_equal(const char* a, const char* b) {
	if (a == b) return true;            // same pointer or both null
	if (!a || !b) return false;         // one is null, the other not
	return std::strcmp(a, b) == 0;      // compare contents
}

// Global initalized string storage used by log_change
struct Store {
	std::vector<std::string> strings;
};
Store storage;

// status vars
NetworkInfo prev_EthernetInfo;
NetworkInfo prev_WLANInfo;
bool prev_internet = false;
std::vector<bool> prev_resolve_by_dns(4, false);
VpnConnection prev_vpn_host;
std::vector<bool> prev_drives(false);
std::vector<bool> prev_unc;

NetworkInfo curr_EthernetInfo;
NetworkInfo curr_WLANInfo;
bool curr_internet = false;
std::vector<bool> curr_resolve_by_dns(4, false);
VpnConnection curr_vpn_host;
std::vector<bool> curr_drives(false);
std::vector<bool> curr_unc;

SessionInfo session;
uint64_t runCounter = 0;
uint64_t maxRuns = 0; // 0 should be no limit

// config vars
bool firstRun = true;
static std::filesystem::path conf_path;
static std::filesystem::path log_path;
static NetworkConfig net;
static DiskConfig disks;
static bool use_vt = true;
static bool vt_enabled = false;

static std::string AuthAlgoToString(DOT11_AUTH_ALGORITHM algo) {
    switch (algo)
    {
    case DOT11_AUTH_ALGO_80211_OPEN:        return "OPEN";
    case DOT11_AUTH_ALGO_80211_SHARED_KEY:  return "SHARED_KEY";
    case DOT11_AUTH_ALGO_WPA:               return "WPA";
    case DOT11_AUTH_ALGO_WPA_PSK:           return "WPA_PSK";
    case DOT11_AUTH_ALGO_WPA_NONE:          return "WPA_NONE";
    case DOT11_AUTH_ALGO_RSNA:              return "RSNA";
    case DOT11_AUTH_ALGO_RSNA_PSK:          return "RSNA_PSK";
	#ifdef DOT11_AUTH_ALGO_WPA3
    case DOT11_AUTH_ALGO_WPA3:              return "WPA3";
	#endif
	#ifdef DOT11_AUTH_ALGO_WPA3_SAE
    case DOT11_AUTH_ALGO_WPA3_SAE:          return "WPA3_SAE";
	#endif
    default:                                return "UNKNOWN";
    }
}

static std::string CipherAlgoToString(DOT11_CIPHER_ALGORITHM algo) {
    switch (algo)
    {
    case DOT11_CIPHER_ALGO_NONE:       return "NONE";
    case DOT11_CIPHER_ALGO_WEP40:      return "WEP40";
    case DOT11_CIPHER_ALGO_TKIP:       return "TKIP";
    case DOT11_CIPHER_ALGO_CCMP:       return "CCMP";
    case DOT11_CIPHER_ALGO_WEP104:     return "WEP104";
    case DOT11_CIPHER_ALGO_BIP:        return "BIP";
    case DOT11_CIPHER_ALGO_GCMP:       return "GCMP";
    case DOT11_CIPHER_ALGO_GCMP_256:   return "GCMP_256";
    case DOT11_CIPHER_ALGO_CCMP_256:   return "CCMP_256";
    default:                           return "UNKNOWN";
    }
}

std::optional<WifiConnectionInfo> GetWifiConnectionInfo(const IP_ADAPTER_ADDRESSES* adapter) {
    if (!adapter || adapter->IfType != IF_TYPE_IEEE80211)
        return std::nullopt;
    HANDLE hClient = nullptr;
    DWORD version = 0;
    if (WlanOpenHandle(2, nullptr, &version, &hClient) != ERROR_SUCCESS)
        return std::nullopt;
    PWLAN_INTERFACE_INFO_LIST interfaces = nullptr;
    if (WlanEnumInterfaces(hClient, nullptr, &interfaces) != ERROR_SUCCESS) {
        WlanCloseHandle(hClient, nullptr);
        return std::nullopt;
    }
    std::optional<WifiConnectionInfo> result;
    for (DWORD i = 0; i < interfaces->dwNumberOfItems; i++) {
        const auto& iface = interfaces->InterfaceInfo[i];
        if (iface.isState != wlan_interface_state_connected)
            continue;
        wchar_t guid[39]{};
        StringFromGUID2(iface.InterfaceGuid, guid, ARRAYSIZE(guid));
        if (_wcsicmp(guid, to_wide(adapter->AdapterName).c_str()) != 0)
            continue;
        PWLAN_CONNECTION_ATTRIBUTES attrs = nullptr;
        DWORD size = 0;
        WLAN_OPCODE_VALUE_TYPE opcodeType;
        if (WlanQueryInterface(
                hClient,
                &iface.InterfaceGuid,
                wlan_intf_opcode_current_connection,
                nullptr,
                &size,
                reinterpret_cast<PVOID*>(&attrs),
                &opcodeType)
			!= ERROR_SUCCESS) {
            break;
        }
        WifiConnectionInfo info{};
        const auto& assoc = attrs->wlanAssociationAttributes;
        const auto& sec = attrs->wlanSecurityAttributes;
        // SSID
        info.WSSID.assign(
            reinterpret_cast<const char*>(assoc.dot11Ssid.ucSSID),
            assoc.dot11Ssid.uSSIDLength);
        // Profile name
        info.WName = wstring_to_utf8_string(attrs->strProfileName);
        // BSSID
        char bssid[18];
		sprintf_s(
			bssid,
			"%02X:%02X:%02X:%02X:%02X:%02X",
			assoc.dot11Bssid[0],
			assoc.dot11Bssid[1],
			assoc.dot11Bssid[2],
			assoc.dot11Bssid[3],
			assoc.dot11Bssid[4],
			assoc.dot11Bssid[5]);
		info.WBSSID = bssid;
        // Signal
        info.WSignalQuality = assoc.wlanSignalQuality;
        // Security
        info.WAuthAlgo = AuthAlgoToString(sec.dot11AuthAlgorithm);
		info.WCipherAlgo = CipherAlgoToString(sec.dot11CipherAlgorithm);
        result = std::move(info);
        WlanFreeMemory(attrs);
        break;
    }
    if (interfaces)
        WlanFreeMemory(interfaces);
    WlanCloseHandle(hClient, nullptr);
    return result;
}

void GetNetworkInfo() {
	ULONG bufLen = 15 * 1024;
	PIP_ADAPTER_ADDRESSES pAddrs = (PIP_ADAPTER_ADDRESSES)malloc(bufLen);
	DWORD ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddrs, &bufLen);
	if (ret == ERROR_BUFFER_OVERFLOW) {
		free(pAddrs);
		pAddrs = (PIP_ADAPTER_ADDRESSES)malloc(bufLen);
		ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddrs, &bufLen);
	}
	if (ret == NO_ERROR) {
		auto GetMetric = [](const IP_ADAPTER_ADDRESSES* p) {
			ULONG metric = ULONG_MAX;
			if (p->Ipv4Metric)
				metric = std::min(metric, p->Ipv4Metric);
			if (p->Ipv6Metric)
				metric = std::min(metric, p->Ipv6Metric);
			return metric;
		};
		auto IsCandidate = [](const IP_ADAPTER_ADDRESSES* p) {
			// has to be UP
			if (p->OperStatus != IfOperStatusUp)
				return false;
			// no loopbacks
			if (p->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
				return false;
			// Ignore tunnel adapters (Teredo, ISATAP, 6to4, etc.)
			if (p->TunnelType != TUNNEL_TYPE_NONE)
				return false;
			// not virtual or ms wifi direct
			if (p->Description) {
				std::wstring_view name{ p->Description };
				if (name.find(L"Wi-Fi Direct") != std::wstring_view::npos)
					return false;
				if (name.find(L"Virtual") != std::wstring_view::npos)
					return false;
			}
			return true;
		};
		PIP_ADAPTER_ADDRESSES bestEth = nullptr;
		PIP_ADAPTER_ADDRESSES bestWifi = nullptr;
		for (auto p = pAddrs; p; p = p->Next) {
			if (!IsCandidate(p))
				continue;
			switch (p->IfType)
			{
			case IF_TYPE_ETHERNET_CSMACD:
				if (!bestEth || GetMetric(p) < GetMetric(bestEth))
					bestEth = p;
				break;
			case IF_TYPE_IEEE80211:
				if (!bestWifi || GetMetric(p) < GetMetric(bestWifi))
					bestWifi = p;
				break;
			}
		}
		auto DumpAdapter = [](PIP_ADAPTER_ADDRESSES p) -> NetworkInfo {
			NetworkInfo info{};
			if (!p)
				//! shouldnt hit
				return info;
			// Adapter GUID
			if (p->AdapterName) {
			info.GUID = p->AdapterName;
			} else {
				info.GUID = "N/A";
			}
			// Friendly name
			if (p->FriendlyName) {
			info.FName = wstring_to_utf8_string(p->FriendlyName);
			} else {
				// friendly name is often not available - fall back to description where available.
				info.FName = "N/A";
			}
			// Driver description
			if (p->Description) {
			info.Description = wstring_to_utf8_string(p->Description);
			} else {
				info.Description = "N/A";
			}
			// DNS suffix
			if (p->DnsSuffix) {
				info.DNSSuffix = wstring_to_utf8_string(p->DnsSuffix);
			} else {
				info.DNSSuffix = "N/A";
			}
			// MAC
			std::ostringstream mac;
			for (ULONG i = 0; i < p->PhysicalAddressLength; ++i) {
				if (i) mac << ':'; // append colon if not first byte
				mac << std::uppercase
					<< std::hex
					<< std::setw(2)
					<< std::setfill('0')
					<< static_cast<int>(p->PhysicalAddress[i]);
			}
			info.MAC = (mac.str().empty() ? "N/A" : mac.str());
			// DHCPv4 server
			if (p->Dhcpv4Server.lpSockaddr) {
				char ip[INET6_ADDRSTRLEN]{};
				getnameinfo(
					p->Dhcpv4Server.lpSockaddr,
					p->Dhcpv4Server.iSockaddrLength,
					ip,
					sizeof(ip),
					nullptr,
					0,
					NI_NUMERICHOST);
				info.PrimaryDHCPv4 = ip;
			} else {
				info.PrimaryDHCPv4 = "N/A";
			}
			// Primary DNS server
			if (p->FirstDnsServerAddress) {
				char ip[INET6_ADDRSTRLEN]{};
				getnameinfo(
					p->FirstDnsServerAddress->Address.lpSockaddr,
					p->FirstDnsServerAddress->Address.iSockaddrLength,
					ip,
					sizeof(ip),
					nullptr,
					0,
					NI_NUMERICHOST);
				info.PrimaryDNS = ip;
			} else {
				info.PrimaryDNS = "N/A";
			}
			// Primary gateway (walking the list until valid entry)
			if (p->FirstGatewayAddress) {
				bool found = false;
				for (auto* gw = p->FirstGatewayAddress; gw; gw = gw->Next) {
					char ip[INET6_ADDRSTRLEN]{};
					if (getnameinfo(
							gw->Address.lpSockaddr,
							gw->Address.iSockaddrLength,
							ip,
							sizeof(ip),
							nullptr,
							0,
							NI_NUMERICHOST) == 0)
					{
						info.PrimaryGateway = ip;
						found = true;
						break;
					}
				}
				if (!found)
					info.PrimaryGateway = "N/A";
			}
			else {
				info.PrimaryGateway = "N/A";
			}
			if (p->IfType == IF_TYPE_IEEE80211) {
				// get wifi info
				auto winfo = GetWifiConnectionInfo(p);
				// and reassign it
				if (winfo) {
					info.WSSID          = !winfo->WSSID.empty() ?       winfo->WSSID :         "N/A";
					info.WName          = !winfo->WName.empty() ?       winfo->WName :         "N/A";
					info.WBSSID         = !winfo->WBSSID.empty() ?      winfo->WBSSID :        "N/A";
					info.WSignalQuality =  winfo->WSignalQuality ?      winfo->WSignalQuality : 0;
					info.WAuthAlgo      = !winfo->WAuthAlgo.empty() ?   winfo->WAuthAlgo :     "N/A";
					info.WCipherAlgo    = !winfo->WCipherAlgo.empty() ? winfo->WCipherAlgo :   "N/A";
				}
			}
			return info;
		};
		curr_EthernetInfo = DumpAdapter(bestEth);
		curr_WLANInfo = DumpAdapter(bestWifi);
	}
	free(pAddrs); // manually free the one thing using malloc
}

std::wstring QueryWtsString(DWORD sessionId, WTS_INFO_CLASS infoClass) {
    LPWSTR buffer = nullptr;
    DWORD bytes = 0;
    std::wstring result;
    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionId, infoClass, &buffer, &bytes) && buffer) {
        result = buffer;
        WTSFreeMemory(buffer);
    }
    return result;
}

static ULONGLONG uptime() { return GetTickCount64() / 1000; }

static SessionInfo getLocalSessionInfo(bool firstRun = false) {
	SessionInfo activeUser;
	//* only query session info once at runtime
	if (firstRun) {
		wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1] = {};
		DWORD computerLen = MAX_COMPUTERNAME_LENGTH + 1;
		if (GetComputerNameW(computerName, &computerLen)) {
			activeUser.hostname = wstring_to_utf8_string(computerName);
		} else {
			activeUser.hostname = "N/A";
		}
		DWORD activeSessionId = WTSGetActiveConsoleSessionId();
		PWTS_SESSION_INFOW sessions = nullptr;
		DWORD sessionCount = 0;
		DWORD loggedInCount = 0;
		if (WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &sessionCount)) {
			for (DWORD i = 0; i < sessionCount; ++i) {
				DWORD sid = sessions[i].SessionId;
				std::wstring user = QueryWtsString(sid, WTSUserName);
				std::wstring domain = QueryWtsString(sid, WTSDomainName);
				if (!user.empty()) {
					++loggedInCount;
					if (sid == activeSessionId) { // || sessions[i].State == WTSActive //? grab active console user, idc about interactive
						activeUser.user = !user.empty() ? wstring_to_utf8_string(user.c_str()) : "N/A";
						activeUser.domain = !domain.empty() ? wstring_to_utf8_string(domain.c_str()) : "N/A";
						activeUser.sessionId = sid? sid : 0;
					}
				}
			}
			WTSFreeMemory(sessions);
		}
		activeUser.loggedInCount = loggedInCount; //
	} else {
		activeUser.upTimeSec = uptime(); // both guar to be real initialized nums
	}
	// bc of default values this is a safe return
	return activeUser;
}

enum class status_change_type {
	internet_connectivity,
	ethernet,
	wlan,
	dns_resolution,
	vpn_connection,
	drive_availability,
	unc_availability,
};
static std::vector<status_change_type> detection_kinds;

// beginning with casting macros for logging
#define _SIZE_T__VOIDP(idx) reinterpret_cast<void*>(static_cast<std::uintptr_t>(idx)) // converting via a pointer-size uint for type safety. idx should be size_t, in my case a 64-bit.
#define _VOIDP__SIZE_T(ptr) static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(ptr)) // safer mirrored cast back to size_t via uintptr_t for pointer-size uint
#define _VOIDP__STRING(ptr) reinterpret_cast<std::string*>(ptr) // casting string directly would throw so we assume we good and that only a string pointer is passed
//#define _STRING__VOIDP(strp) reinterpret_cast<void*>(strp)
#define _STRING__CHAR(strp) ((strp)->empty() ? '?' : static_cast<char>((*strp)[0])) // explicit cast bc fuck the compiler
//const char sss = _STRING__CHAR(&"haha"); // <const char>('h')

static std::string get_safe_filename_timestamp() {
	auto ts = get_timestamp();
	// self explanatory, but this makes a windows filename safe timestamp
	std::replace(ts.begin(), ts.end(), ':', '_');  // : -> _
	std::replace(ts.begin(), ts.end(), '/', '_');  // / -> _
	std::replace(ts.begin(), ts.end(), '\\', '_'); // \ -> _
	return ts;
}

static void ensure_log_location() {
	namespace fs = std::filesystem;

	#pragma warning(disable:4996)
	const char* userEnv = getenv("USERNAME");
	const char* tmp_p = getenv("OneDriveCommercial");
	if (!tmp_p) tmp_p = getenv("OneDrive"); 
	const char* localappdata = getenv("LOCALAPPDATA");
	#pragma warning(default:4996)

	// if not predefined during meta config
	if (log_path.empty()) {
		// Build safe fallback chain for log path
		std::string temp1;
		if (localappdata && can_write_file_dir(localappdata)) { // %LOCALAPPDATA% is ideal if available and writable
			log_path = localappdata;
		}
		else if (tmp_p) {
			temp1 = tmp_p; // OneDrive Commercial or Personal
		}
		else if (userEnv) {
			temp1 = std::string("C:\\Users\\") + userEnv; // User profile directory is next best
		}
		else {
			temp1 = get_exe_dir().string(); // Fallback to executable directory if all else fails
		}
		if (log_path != localappdata && can_write_file_dir(temp1)) { // Use the fallback if it is writable and we didnt use %LOCALAPPDATA%
			log_path = temp1;
		}
	}
	else {
		if (!can_write_file_dir(log_path)){
			log_path = fs::path(get_exe_dir().string()); // Reset to default executable path
		}
	}
	// append the rest of the path and filename
	log_path /= "DDCL-Logs";
	log_path /= std::string("DDCL_log-" + get_safe_filename_timestamp() + ".csv");
	// Ensure log directory exists (no error thrown on failure thanks to error_code)
	std::filesystem::path dir = log_path.parent_path();
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	// check for specific errors that would cause logging to fail, and act appropriately
	if (ec == std::errc::permission_denied) {
		std::cerr << "Permission denied when creating log directory: " << dir.string() << std::endl;
		std::cerr << "Please check permissions or specify a different log path in conf.toml\n";
	}
	else if (ec == std::errc::no_such_file_or_directory) {
		std::cerr << "Invalid path when creating log directory: " << dir.string() << std::endl;
		std::cerr << "Please check the log path in conf.toml\n";
	}
	else if (ec) {
		// Unexpected errors we can't handle - user should define a valid directory in config to avoid this or fix permissions
		std::cerr << "Unexpected error ocurred when creating log directory : " << static_cast<const char*>(ec.message().c_str());
	}
	if (ec) {
		std::cerr << YELLOW << BOLD << "\nLogging will be disabled due to above error. Program will continue to run and monitor changes, but no logs will be written.\n" << RESET;
		std::this_thread::sleep_for(std::chrono::seconds(5)); // Give user time to read error message before continuing without logging
		// Logging will still attempt, but print errors on each run to keep the user alert in case they missed the initial message
	}
}

static void write_to_log(const std::string& message) {
	// I can't explain this even if i tried. It just safely writes the passed string into the log file
	try {
		// Append text to the log file. Use binary + append to avoid accidental truncation.
		std::ofstream out(log_path, std::ios::binary | std::ios::app);
		if (!out) {
			DWORD err = GetLastError();
			std::cerr << "ofstream FAILED, err=" << err << " path=[" << log_path.string() << "]\n";
			return;
		}
		out << message << "\r\n";
		out.flush();
		if (out.fail()) {
			DWORD err = GetLastError();
			std::cerr << "FLUSH FAILED after write! path=[" << err << " path=[" << log_path.string() << "]\n";
			return;
		}
		out.close();  // Explicit close
		//std::cerr << "SUCCESS wrote to [" << log_path.string() << "] size=" << std::filesystem::file_size(log_path) << std::endl;
		if (!out) {
			std::cerr << "Failed to open log file: " << log_path.string() << std::endl;
			return;
		}
	}
	catch (const std::exception& ex) {
		std::cerr << "write_to_log exception: " << ex.what() << std::endl;
	}
};

static void log_change(const status_change_type diff, std::string time, void* info = nullptr) {
	// This function converts the given signals into a string passed to write_to_log in a CSV format.
	// called with type of change, timestamp, and optional info (used for drive letter, UNC path, etc.) specific to the change type

	// Expectation: when `info` is provided for textual info it points to a `std::string`.
	// We make an internal copy into `storage.strings`
	void* info_copy = info;

	// only init one oss per call
	std::ostringstream oss;

	// These are going to be a bit monolithic but relatively straightforward
	// The given status_change_type determines the formatting of the log entry, 
	// for which the relevant info is already known - this just translates a lot of information into simple strings for logging.
	switch (diff) {
		case status_change_type::internet_connectivity: {
			oss << time << ",internet_connectivity," << (curr_internet ? "online" : "offline");
			write_to_log(oss.str());
			break;
		}

		case status_change_type::ethernet: {
			if (info_copy == nullptr || !info_copy) {
				// nothing passed, ethernet info change
				// safely assume non-empty strings
				const char* friendly = curr_EthernetInfo.FName.c_str();
				const char* c_suffix = curr_EthernetInfo.DNSSuffix.c_str();
				const char* p_suffix = prev_EthernetInfo.DNSSuffix.c_str();
				const char* c_guid = curr_EthernetInfo.GUID.c_str();
				const char* p_guid = prev_EthernetInfo.GUID.c_str();
				const char* c_mac = curr_EthernetInfo.MAC.c_str();
				const char* p_mac = prev_EthernetInfo.MAC.c_str();
				const char* c_dhcp = curr_EthernetInfo.PrimaryDHCPv4.c_str();
				const char* p_dhcp = prev_EthernetInfo.PrimaryDHCPv4.c_str();
				const char* c_dns = curr_EthernetInfo.PrimaryDNS.c_str();
				const char* p_dns = prev_EthernetInfo.PrimaryDNS.c_str();
				const char* c_gateway = curr_EthernetInfo.PrimaryGateway.c_str();
				const char* p_gateway = prev_EthernetInfo.PrimaryGateway.c_str();
				if (firstRun) {
					// eight used fields for ethernet info
					oss << time << ",ethernet," ;
					if (curr_EthernetInfo.GUID != "N/A") {
						oss << "GUID:" << c_guid << ";" \
						<< "FriendlyName:'" << friendly << "';" \
						<< "Description:'" << curr_EthernetInfo.Description << "';" \
						<< "DNSSuffix:" << c_suffix << ";" \
						<< "MAC:'" << c_mac << "';" \
						<< "PrimaryDHCPv4:" << c_dhcp << ";" \
						<< "PrimaryDNS:" << c_dns << ";" \
						<< "PrimaryGateway:" << c_gateway << "," ;
					} else {
						oss << "no_connection," ;
					}
					write_to_log(oss.str());
					break; // stop processing since its first run
				} else {
					oss << time << ",ethernet," << friendly << ";" << c_suffix << ",";
				}
				if (c_guid != p_guid || c_mac != p_mac) {
					// adapter changed
					//* log changed guid, fname, desc., mac
					oss << "adapter_change;(['" \
					// previous info
					<< prev_EthernetInfo.FName << "';" << p_guid << ";" << p_mac << ";" << prev_EthernetInfo.Description \
					// new info
					<< "]:['" << friendly << "';" << c_guid << ";" << c_mac << ";" << curr_EthernetInfo.Description << "])";
				} else if ( c_suffix != p_suffix) {
					// dns suffix
					oss << "dns_suffix_change;(" << p_suffix << ":" << c_suffix << ")";
				} else if (c_dhcp != p_dhcp) {
					// changed dhcp server
					oss << "dhcp_server_change;(" << p_dhcp << ":" << c_dhcp << ")";
				} else if (c_dns != p_dns) {
					// changed dns server
					oss << "dns_server_change;(" << p_dns << ":" << c_dns << ")";
				} else if (c_gateway != p_gateway) {
					// changed gateway for some reason
					oss << "gateway_change;(" << p_gateway << ":" << c_gateway << ")";
				} else {
					// generic unknown change
					oss << "unknow_change;possible:[connection_status]";
				}
				oss << ",";
				write_to_log(oss.str());
			}
			else {
				const std::string* incoming = _VOIDP__STRING(info_copy);
				// a string is passed through info field - log it with current info
				if (incoming) {
					storage.strings.push_back(*incoming); // borrow some global memory
					const char* friendly = curr_EthernetInfo.FName.empty() ? curr_EthernetInfo.FName.c_str() : "N/A";
					const char* c_suffix = curr_EthernetInfo.DNSSuffix.empty() ? curr_EthernetInfo.DNSSuffix.c_str() : "N/A";
					oss << time << ",ethernet," << friendly << ";" << c_suffix << "," << storage.strings.back();
					write_to_log(oss.str());
				}
				else {
					// failed to fetch string
					oss << time << ",ethernet,UNKNOWN,info_unavailable";
					write_to_log(oss.str());
				}
			}
			break;
		}

		case status_change_type::wlan: {
			if (info_copy == nullptr || !info_copy) {
				// nothing passed, wlan info change
				// safely assume non-empty strings
				const char* friendly = curr_WLANInfo.FName.c_str();
				const char* c_suffix = curr_WLANInfo.DNSSuffix.c_str();
				const char* p_suffix = prev_WLANInfo.DNSSuffix.c_str();
				const char* c_guid = curr_WLANInfo.GUID.c_str();
				const char* p_guid = prev_WLANInfo.GUID.c_str();
				const char* c_mac = curr_WLANInfo.MAC.c_str();
				const char* p_mac = prev_WLANInfo.MAC.c_str();
				const char* c_dhcp = curr_WLANInfo.PrimaryDHCPv4.c_str();
				const char* p_dhcp = prev_WLANInfo.PrimaryDHCPv4.c_str();
				const char* c_dns = curr_WLANInfo.PrimaryDNS.c_str();
				const char* p_dns = prev_WLANInfo.PrimaryDNS.c_str();
				const char* c_gateway = curr_WLANInfo.PrimaryGateway.c_str();
				const char* p_gateway = prev_WLANInfo.PrimaryGateway.c_str();
				if (firstRun) {
					// every field is used for wlan
					oss << time << ",wlan,";
					if (curr_WLANInfo.GUID != "N/A") {
						oss << "GUID:" << c_guid << ";" \
						<< "FriendlyName:'" << friendly << "';" \
						<< "Description:'" << curr_WLANInfo.Description << "';" \
						<< "DNSSuffix:" << c_suffix << ";" \
						<< "SSID:'" << curr_WLANInfo.WSSID << "';" \
						<< "Name:'" << curr_WLANInfo.WName << "';" \
						<< "BSSID:" << curr_WLANInfo.WBSSID << ";" \
						<< "SignalQuality:" << curr_WLANInfo.WSignalQuality << ";" \
						<< "AuthAlgo:" << curr_WLANInfo.WAuthAlgo << ";" \
						<< "CipherAlgo:" << curr_WLANInfo.WCipherAlgo << ";" \
						<< "MAC:'" << c_mac << "';" \
						<< "PrimaryDHCPv4:" << c_dhcp << ";" \
						<< "PrimaryDNS:" << c_dns << ";" \
						<< "PrimaryGateway:" << c_gateway << "," ;
					} else {
						oss << "no_connection," ;
					}
					write_to_log(oss.str());
					break; // stop processing since its first run
				} else {
					oss << time << ",wlan," << friendly << ";" << c_suffix << ",";
				}
				if (c_guid != p_guid || c_mac != p_mac) {
					// adapter changed
					//* log changed guid, fname, desc., mac
					oss << "adapter_change;(['" \
					// previous info
					<< prev_WLANInfo.FName << "';" << p_guid << ";" << p_mac << ";" << prev_WLANInfo.Description \
					// new info
					<< "]:['" << friendly << "';" << c_guid << ";" << c_mac << ";" << curr_WLANInfo.Description << "])";
				} else if ( c_suffix != p_suffix) {
					// dns suffix
					oss << "dns_suffix_change;(" << p_suffix << ":" << c_suffix << ")";
				} else if (c_dhcp != p_dhcp) {
					// changed dhcp server
					oss << "dhcp_server_change;(" << p_dhcp << ":" << c_dhcp << ")";
				} else if (c_dns != p_dns) {
					// changed dns server
					oss << "dns_server_change;(" << p_dns << ":" << c_dns << ")";
				} else if (c_gateway != p_gateway) {
					// changed gateway for some reason
					oss << "gateway_change;(" << p_gateway << ":" << c_gateway << ")";
				} else {
					// generic unknown change
					oss << "unknow_change;possible:[connection_status]";
				}
				oss << ",";
				write_to_log(oss.str());
			}
			else {
				const std::string* incoming = _VOIDP__STRING(info_copy);
				// a string is passed through info field - log it with current info
				if (incoming) {
					storage.strings.push_back(*incoming); // borrow some global memory
					const char* friendly = curr_WLANInfo.FName.empty() ? curr_WLANInfo.FName.c_str() : "N/A";
					const char* c_suffix = curr_WLANInfo.DNSSuffix.empty() ? curr_WLANInfo.DNSSuffix.c_str() : "N/A";
					oss << time << ",wlan," << friendly << ";" << c_suffix << "," << storage.strings.back();
					write_to_log(oss.str());
				}
				else {
					// failed to fetch string
					oss << time << ",wlan,UNKNOWN,info_unavailable";
					write_to_log(oss.str());
				}
			}
			break;
		}

		case status_change_type::dns_resolution: {
			oss << time << ",dns_resolution,"
				<< net.check << ":[sys_dns:" << (curr_resolve_by_dns[0] ? "True;" : "False;") \
				<< net.dns << (curr_resolve_by_dns[1] ? ":True" : ":False") \
				<< "];www.wikipedia.org:[sys_dns:" << (curr_resolve_by_dns[2] ? "True;" : "False;") \
				<< net.dns << (curr_resolve_by_dns[3] ? ":True" : ":False") << "]," ;
			write_to_log(oss.str());
			break;
		}

		case status_change_type::vpn_connection: {
			oss << time << ",vpn_connection," << (curr_vpn_host.connected
				? "connected," + curr_vpn_host.name + ";" + curr_vpn_host.hostname + ";" + curr_vpn_host.local_ip
				: "not_connected,") ;
			write_to_log(oss.str());
			break;
		}

		case status_change_type::drive_availability: {
			std::size_t idx = 0;
			if (info_copy == nullptr) {
				idx = 0;
			} else {
				try {
					idx = _VOIDP__SIZE_T(info_copy);
				} catch (std::exception &ex) {
					std::cerr << "Conversion exception: " << ex.what() << std::endl;
					idx = 0;
				}
			}
			if (idx >= curr_drives.size()) {
				std::cerr << "Index out of range for curr_drives";
				return;
			}
			try {
				char driveLetter = _STRING__CHAR(&disks.locals[idx]);
				std::string label = disks.locals_labels[idx];
				oss << time << ",drive_availability,"
					<< (curr_drives[idx] ? "available," : "unavailable,")
					<< driveLetter << ':' << label;
				write_to_log(oss.str());
			}
			catch (const std::exception& ex) {
				std::cerr << "\nError extracting drive letter in log_change: " << ex.what() << std::endl;
				oss << time << ",drive_availability,unavailable,drive letter unavailable";
				write_to_log(oss.str());
			}
			break;
		}

		case status_change_type::unc_availability: {
			std::size_t idx = _VOIDP__SIZE_T(info_copy);
			if (idx >= curr_unc.size()) {
				std::cerr << "Invalid index for UNC paths";
				return;
			}
			std::string uncPath = disks.unc[idx];
			std::string label = disks.unc_labels[idx];
			oss << time << ",unc_availability,"
				<< (curr_unc[idx] ? "available," : "unavailable,")
				<< uncPath << ':' << label;
			write_to_log(oss.str());
			break;
		}

		default:
			std::cerr << "Unknown ´status_change_type´ in log_change()";
			return;
	}
};

static void status_check(bool nowrite = false) {
	// i think this is in the same order as the ui - which is a bit important for logs to be idiosyncratic and easily parsable by eye
	// the order of the ui is also the most subjectively logical ordering
	// these are all just comparisons of prev_ vs curr_ values, and if different, call log_change with the appropriate type and info.
	const std::string c_time = get_timestamp();
	for (const auto& kind : detection_kinds) {
		switch (kind) {
			case status_change_type::internet_connectivity:
				if (curr_internet != prev_internet) {
					if (!nowrite) { log_change(status_change_type::internet_connectivity, c_time); }
				}
				break;
			case status_change_type::dns_resolution:
				for (int i = 0; i < 4; i++) { // has a known size so we dont care about dynamic sizing
					if (curr_resolve_by_dns.size() != prev_resolve_by_dns.size()) { break; }
					if (curr_resolve_by_dns[i] != prev_resolve_by_dns[i]) {
						if (!nowrite) { log_change(status_change_type::dns_resolution, c_time); }
						break; // Log once per check, not per individual DNS result
					}
				}
				continue;
			case status_change_type::ethernet:
				// use overloaded op
				if (curr_EthernetInfo != prev_EthernetInfo) {
					if (!nowrite) { log_change(status_change_type::ethernet, c_time); }
				}
				continue;
			case status_change_type::wlan:
				// use overloaded op
				if (curr_WLANInfo != prev_WLANInfo) {
					if (!nowrite) { log_change(status_change_type::wlan, c_time); }
				}
				continue;
			case status_change_type::vpn_connection:
				if (curr_vpn_host.connected != prev_vpn_host.connected ||
					curr_vpn_host.hostname != prev_vpn_host.hostname) {
					if (!nowrite) { log_change(status_change_type::vpn_connection, c_time); }
				}
				continue;
			case status_change_type::drive_availability:
				for (std::size_t i = 0; i < curr_drives.size(); i++) {
					if (curr_drives[i] != prev_drives[i]) {
						if (!nowrite) { log_change(status_change_type::drive_availability, c_time, _SIZE_T__VOIDP(i)); }
					}
				}
				continue;
			case status_change_type::unc_availability:
				for (std::size_t i = 0; i < curr_unc.size(); i++) {
					if (curr_unc[i] != prev_unc[i]) {
						if (!nowrite) { log_change(status_change_type::unc_availability, c_time, _SIZE_T__VOIDP(i)); }
					}
				}
				continue;
		}
	}

	// reassign prev_*
	prev_internet = curr_internet;
	prev_resolve_by_dns = curr_resolve_by_dns;
	prev_EthernetInfo = curr_EthernetInfo;
	prev_WLANInfo = curr_WLANInfo;
	prev_vpn_host = curr_vpn_host;
	prev_drives = curr_drives;
	prev_unc = curr_unc;
}

static void update_status() {
	// This function is responsible for updating the curr_ status variables with the latest values from the system every second. 
	// Assign new values to curr_* - theyre saved to prev in status_check() after comparison and logging. 
	bool net_ran = false;
	for (const auto& kind : detection_kinds) {
		switch (kind) {
			case status_change_type::internet_connectivity:
				{
					curr_internet = has_internet(net.check.c_str(), net.dns.c_str());
					continue;
				}
			case status_change_type::dns_resolution:
				{
					std::vector<bool> dns_results = resolve_hostname(net.check, net.dns);
					curr_resolve_by_dns[0] = dns_results[0];
					curr_resolve_by_dns[1] = dns_results[1];
					dns_results = resolve_hostname("www.wikipedia.org", net.dns);
					curr_resolve_by_dns[2] = dns_results[0];
					curr_resolve_by_dns[3] = dns_results[1];
					continue;
				}
			case status_change_type::ethernet:
			case status_change_type::wlan:
				// on either ethernet or wlan but not twice
				if (!net_ran) {
					GetNetworkInfo(); // guards and assignment inside function.
					net_ran = true;
				}
				continue;
			case status_change_type::vpn_connection:
				{
					auto vpn = get_active_vpn();
					if (vpn) { curr_vpn_host = *vpn; }
					continue;
				}
			case status_change_type::drive_availability:
				{
					for (int c = 0; c < disks.locals.size();c++) {
						const std::string& driveStr = disks.locals[c];
						if (driveStr.size() == 1 && std::isalpha(driveStr[0])) {
							curr_drives[c] = is_drive_ready((char)std::toupper(driveStr[0]));
						}
						else {
							std::cerr << "Invalid drive letter in config: " << driveStr << std::endl;
							disks.locals.erase(disks.locals.begin() + c);
						}
					}
					continue;
				}
			case status_change_type::unc_availability:
				{
					curr_unc.clear();
					for (const auto& unc : disks.unc) {
						curr_unc.push_back(is_unc_available(unc.c_str()));
					}
					continue;
				}
		}
	}
	// update uptime
	session.upTimeSec = getLocalSessionInfo().upTimeSec;
}

static std::size_t initial_status_write() {
	// Startup function to write initial status of all monitored items to log, called once after initalization and before entering monitoring loop
	update_status();
	const std::string c_time = get_timestamp();
	write_to_log("timestamp,kind,value,info"); // CSV header
	Sleep(250); // Ensure log file is created before writing initial status (and avoid potential race conditions) 

	// next grab initial session info
	session = getLocalSessionInfo(true);

	// Log the registered detection kinds!
	std::string det_str = "";
	det_str.append(c_time);
	det_str.append(",registered_detections,");
	for (const auto& kind : detection_kinds) {
		switch (kind) {
			case status_change_type::internet_connectivity:
				det_str.append("internet_connectivity;");
				continue;
			case status_change_type::ethernet:
				det_str.append("ethernet;");
				continue;
			case status_change_type::wlan:
				det_str.append("wlan;");
				continue;
			case status_change_type::dns_resolution:
				det_str.append("dns_resolution;");
				continue;
			case status_change_type::vpn_connection:
				det_str.append("vpn_connection;");
				continue;
			case status_change_type::drive_availability:
				det_str.append("drive_availability;");
				continue;
			case status_change_type::unc_availability:
				det_str.append("unc_availability;");
				continue;
		}
	}
	if (det_str.back()==';') {det_str.pop_back();}; det_str.append(",initial_status");
	write_to_log(det_str);

	for (const auto& kind : detection_kinds) {
		switch (kind) {
			case status_change_type::internet_connectivity:
				log_change(status_change_type::internet_connectivity, c_time);
				continue;
			case status_change_type::dns_resolution:
				log_change(status_change_type::dns_resolution, c_time);
				continue;
			case status_change_type::ethernet:
				log_change(status_change_type::ethernet, c_time);
				continue;
			case status_change_type::wlan:
				log_change(status_change_type::wlan, c_time);
				continue;
			case status_change_type::vpn_connection:
				log_change(status_change_type::vpn_connection, c_time);
				continue;
			case status_change_type::drive_availability:
				for (std::size_t i = 0; i < curr_drives.size(); i++) {
					log_change(status_change_type::drive_availability, c_time, _SIZE_T__VOIDP(i));
				}
				continue;
			case status_change_type::unc_availability:
				for (std::size_t i = 0; i < curr_unc.size(); i++) {
					log_change(status_change_type::unc_availability, c_time, _SIZE_T__VOIDP(i));
				}
				continue;
		}
	}
	// register the initial run
	firstRun = false;
	runCounter++;
	return std::size_t(1); // to ensure this actually completes before the main loop starts
}

// for handling ^C sig and exit cleanly. also good practice for console apps in general
static std::atomic<bool> g_running{ true };
static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
	switch (dwCtrlType) {
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		// Signal the monitoring loop to exit cleanly
		g_running.store(false);
		return TRUE; // handled
	default:
		return FALSE; // pass to next handler
	}
}

void countRun() {
	runCounter++;
	runCounter &= 0xFFFFFFFFFFFFFFFF;
	// should the loop end on next iteration?
	if (maxRuns != 0 && runCounter>maxRuns) {
		//* this effectively only runs if maxRuns>0&&runCounter>1 atp
		// store in atomic
		g_running.store(false);
		// now the loop will exit on its NEXT iteration.
		// so if maxRuns was set to 1, itll preform the initial status write and one loop to display the results, then exit.
		// if 0 (default) itll NEVER exit without interruption
	}
}

static void initialize_runtime() {
	// Config file setup
	conf_path = get_exe_dir() / "conf.toml";
	ensure_default_conf(conf_path); // call early to create conf if missing
	// then parse config
	toml::table tbl;
	try {
		tbl = toml::parse_file(conf_path.string());
	}
	catch (const toml::parse_error& err) {
		std::cerr << "Error parsing conf.toml: " << err.description() << std::endl;
		std::exit(1);
	}
	// Meta config
	auto meta_tbl = tbl["Meta"].as_table();
	if (meta_tbl) {
		std::string temp_l = std::string(meta_tbl->operator[]("log_path").value_or(std::string()).c_str());
		if (!temp_l.empty()) { log_path = temp_l; }
		use_vt = meta_tbl->operator[]("use_virtual_terminal").value_or(true);
		detection_kinds.clear();
		// TODO: This should test for presence of incorrect keys and emit a warn with correct keys.
		// explicitly test presence and type (e.g. contains("detections") + as_array()), so you can warn on wrong types and implement the exact semantics you want.
		// •	Use a presence check (e.g. contains("detections")) and then as_array() to distinguish:
		/*•	not present → apply “absent” rule
		  •	present but not an array → emit a warning and fall back to safe default
		  •	present and array → handle values(including empty array)*/
		if (meta_tbl->operator[]("detections").is_array()) { // key present and entries are present or not
			auto arr = meta_tbl->operator[]("detections").as_array();
			for (auto& v : *arr) {
				if (auto s = v.value<std::string_view>()) {
					std::string_view str(*s);
					// Not gonna make this case insensitive, i expect literacy or at least copy-pasting from the example config
					if (str == "Internet") {
						detection_kinds.push_back(status_change_type::internet_connectivity);
					}
					else if (str == "Ethernet") {
						detection_kinds.push_back(status_change_type::ethernet);
					}
					else if (str == "WLAN") {
						detection_kinds.push_back(status_change_type::wlan);
					}
					else if (str == "DNS resolution") {
						detection_kinds.push_back(status_change_type::dns_resolution);
					}
					else if (str == "VPN") {
						detection_kinds.push_back(status_change_type::vpn_connection);
					}
					else if (str == "Disks") {
						detection_kinds.push_back(status_change_type::drive_availability);
					}
					else if (str == "UNC") {
						detection_kinds.push_back(status_change_type::unc_availability);
					}
				}
			}
		}
		else {
			// Default to all detections if key is missing or not an array
			detection_kinds = {
				status_change_type::internet_connectivity,
				status_change_type::ethernet,
				status_change_type::wlan,
				status_change_type::dns_resolution,
				status_change_type::vpn_connection,
				status_change_type::drive_availability,
				status_change_type::unc_availability
			};
			// prolly add log events for it too
		}
	}
	// NetworkConfig net;
	auto net_tbl = tbl["Network"].as_table();
	if (net_tbl) {
		net.check = net_tbl->operator[]("check").value_or(std::string("1.1.1.1")).data();
		net.dns = net_tbl->operator[]("dns").value_or(std::string("")).data();
		net.expected_domain = net_tbl->operator[]("expected_domain").value_or(std::string("")).data();
		net.expected_vpn_hostname = net_tbl->operator[]("expected_vpn_hostname").value_or(std::string("")).data();
	}
	// DiskConfig disks;
	if (auto disks_tbl = tbl["Disks"].as_table()) {
		// Locals
		if (auto arr = disks_tbl->operator[]("locals").as_array()) {
			for (auto& v : *arr) {
				if (auto s = v.value<std::string_view>()) {
					std::string local(s->data(), s->size());
					// Parse optional <label># or <label>#! prefix.
					std::string label;
					int importance = 0;
					const std::size_t hash_pos = local.find('#');
					if (hash_pos != std::string::npos) {
						label = local.substr(0, hash_pos);
						if (hash_pos + 1 < local.size() && local[hash_pos + 1] == '!') {
							importance = 1;
							local.erase(0, hash_pos + 2);
						}
						else {
							local.erase(0, hash_pos + 1);
						}
					}
					disks.locals.emplace_back(std::move(local));
					disks.locals_labels.emplace_back(std::move(label));
					disks.locals_imp.push_back(importance);
				}
			}
		}
		// UNC
		if (auto arr = disks_tbl->operator[]("unc").as_array()) {
			for (auto& v : *arr) {
				if (auto s = v.value<std::string_view>()) {
					std::string unc(s->data(), s->size());
					// Parse optional <label># or <label>#! prefix.
					std::string label;
					int importance = 0;
					const std::size_t hash_pos = unc.find('#');
					if (hash_pos != std::string::npos) {
						label = unc.substr(0, hash_pos);
						if (hash_pos + 1 < unc.size() && unc[hash_pos + 1] == '!') {
							importance = 1;
							unc.erase(0, hash_pos + 2);
						}
						else {
							unc.erase(0, hash_pos + 1);
						}
					}
					// Expand $username and $userdomain placeholders at config-load time.
	#pragma warning(disable:4996)
					const char* userEnvCfg = getenv("USERNAME");
					const char* userDomainEnvCfg = getenv("USERDNSDOMAIN");
	#pragma warning(default:4996)
					std::string username =
						userEnvCfg ? std::string(userEnvCfg) : std::string();
					std::string userdomain =
						userDomainEnvCfg ? std::string(userDomainEnvCfg) : std::string();
					// $username
					{
						const std::string placeholder = "$username";
						std::size_t pos = 0;
						while ((pos = unc.find(placeholder, pos)) != std::string::npos) {
							unc.replace(pos, placeholder.size(), username);
							pos += username.size();
						}
					}
					// $userdomain
					{
						const std::string placeholder = "$userdomain";
						std::size_t pos = 0;
						while ((pos = unc.find(placeholder, pos)) != std::string::npos) {
							unc.replace(pos, placeholder.size(), userdomain);
							pos += userdomain.size();
						}
					}
					disks.unc.emplace_back(std::move(unc));
					disks.unc_labels.emplace_back(std::move(label));
					disks.unc_imp.push_back(importance);
				}
			}
		}
	}

	// Initialize curr_ vars to safe defaults to avoid undefined behavior on first update_status() 
	curr_resolve_by_dns = std::vector<bool>{ false, false, false, false };
	//? do not act upon NetworkInfo structs, they have safe defaults and handling in GetNetworkInfo
	curr_vpn_host		= VpnConnection();
	curr_drives			= std::vector<bool>(disks.locals.size(), false);
	curr_unc			= std::vector<bool>(disks.unc.size(), false);

	// Initialize prev_ vars
	prev_internet		= false;
	prev_resolve_by_dns = std::vector<bool>{ false, false, false, false };
	//? do not act upon NetworkInfo structs, they have safe defaults and handling in GetNetworkInfo
	prev_vpn_host		= VpnConnection();
	if (prev_drives.size() != curr_drives.size() || prev_drives.size() != disks.locals.size()) {
		prev_drives		= std::vector<bool>(curr_drives.size(), false);
	}
	if (prev_unc.size() != curr_unc.size() || prev_unc.size() != disks.unc.size()) {
		prev_unc        = std::vector<bool>(curr_unc.size(), false);
	}

	// Enable VTP if requested and set relevant variables
	if (use_vt) {
		if (!EnableVirtualTerminal()) {
			std::cout << "Virtual Terminal Processing not available\n"; // dont care, not a critical failure
		}
		else { vt_enabled = true; }
	}
	// register the controlhandler:
	if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE)) {
		std::cerr << "Warning: failed to install console control handler\n";
	}
	ensure_log_location(); // make sure a log path is set and valid, or disable logging if not
}

static void print_help_text(char* calltext) {
	// This handles the -h --help option. Self descriptive, really.
	std::cout << BOLD BLUE << "=== DDCL v" << VERSION << " ===" << std::endl << std::endl;
	std::cout << RESET GREEN << calltext << " [-h|--help] [-c|--config]" << std::endl << std::endl;
	std::cout << "  -h --help" << std::endl;
	std::cout << "      Display this help message" << std::endl;
	std::cout << "  -c --config" << std::endl;
	std::cout << "      Display a configuration summary"<< std::endl;
	std::cout << "  -t --times:<count>" << std::endl;
	std::cout << "      Run detection loop <count> times. Defaults to 0." << std::endl;
	std::cout << "      If <count> is not given correctly, assumes 1." << RESET  << std::endl << std::endl;
	std::cout << "DDCL is a tool for surveying network and storage status changes." << std::endl;
	std::cout << "Checks are performed once every second and logged to a location given through a fallback chain." << std::endl;
	std::cout << " (See documentation at https://github.com/DVP-F/DDCL for detail)" << std::endl;
	std::cout << "At the moment, logs will be written to " << YELLOW << log_path.remove_filename().string() << RESET << std::endl;
}

static void print_config_summary(char* choice) {
	// This handles the -c --config option. Just prints the effective configuration after parsing conf.toml
	std::cout << BOLD BLUE << "=== Disk Drive Connection Logger (DDCL) v" << VERSION << " - Startup Summary===\n" << RESET;
	std::cout << "Commandline arguments: " << BOLD << choice;
	std::cout << "\nEnabled Detections:\n  " << RESET GREEN;
	for (const auto& kind : detection_kinds) {
		switch (kind) {
			case status_change_type::internet_connectivity:
				std::cout << "Internet Connectivity, ";
				break;
			case status_change_type::ethernet:
				std::cout << "Ethernet, ";
				break;
			case status_change_type::wlan:
				std::cout << "WLAN, ";
				break;
			case status_change_type::dns_resolution:
				std::cout << "DNS Resolution, ";
				break;
			case status_change_type::vpn_connection:
				std::cout << "VPN Connection, ";
				break;
			case status_change_type::drive_availability:
				std::cout << "Drive Availability, ";
				break;
			case status_change_type::unc_availability:
				std::cout << "UNC Availability, ";
				break;
		}
	}
	std::cout << std::endl;
	std::cout << RESET BOLD << "Log Path: " << RESET BLUE << log_path.remove_filename().string() << RESET << std::endl;
	std::cout << BOLD << "Virtual Terminal Processing: " << RESET YELLOW << (use_vt ? "Requested" : "Not Requested\n\n");
	if (use_vt) {
		std::cout << RESET BOLD << ", Status: " << RESET << (vt_enabled ? GREEN "ON" : YELLOW "OFF") << RESET << "\n\n"; // This will be set in initialize_runtime after attempting to enable VT
	}
	std::cout << "Network:" << std::endl;
	std::cout << "  Check Host: " << BOLD << net.check << RESET << std::endl;
	std::cout << "  DNS Server: " << BOLD << net.dns << RESET << std::endl;
	std::cout << "  Expected Domain Suffix: " << BOLD << net.expected_domain << RESET << std::endl;
	std::cout << "  Expected VPN Hostname: " << BOLD << net.expected_vpn_hostname << RESET << "\n\n";
	std::cout << "Disks:" << std::endl;
	std::cout << "  Local Drives: ";
	for (const auto& drive : disks.locals) {
		std::cout << BOLD << drive << RESET << " ";
	}
	std::cout << "\n  UNC paths:\n";
	for (std::size_t i = 0; i < disks.unc.size(); i++) {
		int importance = disks.unc_imp[i];
		std::cout << (importance == 1 ? RED : YELLOW) << "    " << BOLD << disks.unc[i] << RESET << std::endl;
	}
}

void PrepareTerminal(short rows) {
    constexpr short MIN_WIDTH = 100;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    short currentRows = 0;
    short currentCols = 0;
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(hOut, &info)) {
        currentCols = info.srWindow.Right - info.srWindow.Left + 1;
        currentRows = info.srWindow.Bottom - info.srWindow.Top + 1;
    }
    // If query failed, assume minimums
    if (currentCols <= 0)
        currentCols = MIN_WIDTH;
    if (currentRows <= 0)
        currentRows = 25;
    short targetRows = std::max(currentRows, rows);
    short targetCols = std::max(currentCols, MIN_WIDTH);
    // Nothing to do
    if (targetRows == currentRows && targetCols == currentCols)
        return;
    // First try ANSI (WT, xterm, etc.)
    std::cout << "\x1B[8;" << targetRows << ";" << targetCols << "t";
    std::cout.flush();
    CONSOLE_SCREEN_BUFFER_INFO after{};
    if (!GetConsoleScreenBufferInfo(hOut, &after))
        return;
    short actualRows = after.srWindow.Bottom - after.srWindow.Top + 1;
    short actualCols = after.srWindow.Right - after.srWindow.Left + 1;
    if (actualRows >= targetRows && actualCols >= targetCols)
        return;
    // Try Win32 resize
    COORD buffer{
        targetCols,
        targetRows
    };
    SetConsoleScreenBufferSize(hOut, buffer);
    SMALL_RECT rect{
        0,
        0,
        (SHORT)(targetCols - 1),
        (SHORT)(targetRows - 1)
    };
    SetConsoleWindowInfo(hOut, TRUE, &rect);
}

static void sleep_until_next_tick(std::chrono::steady_clock::time_point loop_start) {
	// Clamp sleep time to 0 <= ... <= 1 seconds VERY defensively 
	// - this keeps the loop timing as synchronized as possible without starting on multithreading
    constexpr auto interval = std::chrono::seconds(1);
    const auto next_tick = loop_start + interval;
    const auto now = std::chrono::steady_clock::now();
    if (now < next_tick)
        std::this_thread::sleep_until(next_tick);
}

int main(int argc, char* argv[]) {
	using namespace std::string_view_literals;

	// Initialize Winsock early since many (CRITICAL) functions depend on it. If this fails, we can exit immediately.
	if (!ensure_wsa_initialized()) {
		std::cerr << RESET << BOLD RED << "WSAStartup failed!\n" << RESET << "Failed to initialize socket, aborting." << RESET;
		std::this_thread::sleep_for(std::chrono::seconds(5));
		abort(); // Very strict exit because we need a socket to do most anything.
		//? This may have to change to depend on detection kinds bc they dont all use the socket, but thats a significant rewrite.
	}

	initialize_runtime();

	// first of: handler arguments, if any.
	if (argc > 2) {
		std::cerr << "Too many arguments provided.\nUsage: " << argv[0] << " [ [-h|--help] | [-c|--config] | [-t|--times:<number>] ]" << std::endl;
		throw std::invalid_argument("Too many arguments provided");
	}
	if (argc == 2) { // 2 arguments since arg 0 is the binary call
		auto arg_strv = std::string_view(argv[1]);
		if (arg_strv == "-h" || arg_strv == "--help") {
			show_help = true;
		}
		else if (arg_strv == "-c" || arg_strv == "--config") {
			show_config = true;
		}
		else {
			size_t count = 0;
			// cpp17 version. avoids cpp20-exclusive starts_with and MSVC _Starts_with
			constexpr std::string_view short_opt = "-t:";
			constexpr std::string_view long_opt  = "--times:";
			std::string_view number;
			if (arg_strv.compare(0, short_opt.size(), short_opt) == 0) {
				number = arg_strv.substr(short_opt.size());
			}
			else if (arg_strv.compare(0, long_opt.size(), long_opt) == 0) {
				number = arg_strv.substr(long_opt.size());
			}
			if (!number.empty()) {
				auto [ptr, ec] = std::from_chars(number.data(), number.data() + number.size(), count);
				if (ec != std::errc{} || ptr != number.data() + number.size()) {
					// Invalid number
					count = 1; //* gonna assume 1
				}
			}
			maxRuns = (uint64_t)count;
		}
	}

	if (show_help) {
		print_help_text(argv[0]);
		return(0);
	}
	if (show_config) {
		print_config_summary(argv[1]);
		return(0);
	}

	// Ensure the beginning status is written to the log, exit on failure (since if this fails, logging will fail silently)
	if (initial_status_write() == 0) {
		std::cerr << RESET << BOLD << "Failed to write initial status to log!\n" << RESET << RED << "Please check permissions or move the executable to a different directory.\n" << RESET;
		std::this_thread::sleep_for(std::chrono::seconds(5));
		return 2;
	}

	// Main monitoring loop 
	try {
		auto timer_start = std::chrono::steady_clock::now();
		while (g_running.load()) { // check if it should even run any more
			countRun(); // register the run
			SHORT linecount = 8; // covers start -> dns resolution - inc from there
			linecount += 14; // for margin
			auto loop_start = std::chrono::steady_clock::now();
			update_status();

			// Just a wall of logic for the status dump
			std::cout << CLEAR NOWRAP;
			std::cout << BOLD << CYAN << "=== [" << get_timestamp() << "] Network & Drive Status ===\n" << RESET;
			std::cout << BOLD << "Internet: " << RESET;
			std::cout << (curr_internet ? GREEN "ONLINE" : RED "OFFLINE") << RESET << std::endl;
			std::cout << BOLD << "DNS Resolution:\n" << RESET;
			std::cout << "  " << BOLD << net.check << ":\n" RESET;
			std::cout << "    Local DNS: " << (curr_resolve_by_dns[0] ? GREEN "RESOLVED" : RED "FAILED") << RESET << std::endl;
			std::cout << "    " << net.dns << ": " << (curr_resolve_by_dns[1] ? GREEN "RESOLVED" : RED "FAILED") << RESET << std::endl;
			std::cout << "  " << BOLD << "www.wikipedia.org" << ":\n" RESET ;
			std::cout << "    Local DNS: " << (curr_resolve_by_dns[2] ? GREEN "RESOLVED" : RED "FAILED") << RESET << std::endl;
			std::cout << "    " << net.dns << ": " << (curr_resolve_by_dns[3] ? GREEN "RESOLVED" : RED "FAILED") << RESET << std::endl;
			// ethernet connections if any
			std::cout << BOLD << "Ethernet Adapter Info:" << RESET << std::endl;
			linecount++;
			if (curr_EthernetInfo.GUID != "N/A") {
				std::cout << "  " << BOLD << "Friendly Name: " << RESET << curr_EthernetInfo.FName.c_str() << std::endl;
				std::cout << "  " << BOLD << "Description:   " << RESET << curr_EthernetInfo.Description.c_str() << std::endl;
				std::cout << "  " << BOLD << "DNS Suffix:    " << RESET << curr_EthernetInfo.DNSSuffix.c_str() << " {" \
					<< (net.expected_domain == curr_EthernetInfo.DNSSuffix.c_str() ? GREEN "MATCH" : RED "NO MATCH") << RESET "}\n";
				std::cout << "  " << BOLD << "MAC Address:   " << RESET << curr_EthernetInfo.MAC.c_str() << std::endl;
				std::cout << "  " << BOLD << "DHCPv4 Server: " << RESET << curr_EthernetInfo.PrimaryDHCPv4.c_str() << std::endl;
				std::cout << "  " << BOLD << "DNS Server:    " << RESET << curr_EthernetInfo.PrimaryDNS.c_str() << std::endl;
				std::cout << "  " << BOLD << "Gateway:       " << RESET << curr_EthernetInfo.PrimaryGateway.c_str() << std::endl;
				linecount += 7;
			} else {
				std::cout << "  " << YELLOW << "No Ethernet connections detected!" << RESET << std::endl ;
				linecount++;
			}
			// wifi connections if any
			std::cout << BOLD << "WLAN Adapter Info:" << RESET << std::endl ;
			linecount++;
			if (curr_WLANInfo.GUID != "N/A") {
				std::cout << "  " << BOLD << "Friendly Name: " << RESET << curr_WLANInfo.FName.c_str() << std::endl;
				std::cout << "  " << BOLD << "Description:   " << RESET << curr_WLANInfo.Description.c_str() << std::endl;
				std::cout << "  " << BOLD << "MAC Address:   " << RESET << curr_WLANInfo.MAC.c_str() << std::endl;
				std::cout << "  " << BOLD << "DHCPv4 Server: " << RESET << curr_WLANInfo.PrimaryDHCPv4.c_str() << std::endl;
				std::cout << "  " << BOLD << "DNS Server:    " << RESET << curr_WLANInfo.PrimaryDNS.c_str() << std::endl;
				std::cout << "  " << BOLD << "Gateway:       " << RESET << curr_WLANInfo.PrimaryGateway.c_str() << std::endl;
				std::cout << "  " << BOLD << "SSID:          " << RESET << curr_WLANInfo.WSSID.c_str() << std::endl;
				std::cout << "  " << BOLD << "BSSID:         " << RESET << curr_WLANInfo.WBSSID.c_str() << std::endl;
				std::cout << "  " << BOLD << "Network Name:  " << RESET << curr_WLANInfo.WName.c_str() << std::endl;
				std::cout << "  " << BOLD << "Signal:        " << RESET << curr_WLANInfo.WSignalQuality << std::endl;
				std::cout << "  " << BOLD << "Auth Algo:     " << RESET << curr_WLANInfo.WAuthAlgo << std::endl;
				std::cout << "  " << BOLD << "Cipher Algo:   " << RESET << curr_WLANInfo.WCipherAlgo << std::endl;
				linecount += 12;
			} else {
				std::cout << "  " << YELLOW << "Not connected!" << RESET << std::endl ;
				linecount++;
			}
			// vpn if present
			std::cout << BOLD << "VPN Info:" << RESET << std::endl ;
			linecount++;
			if (curr_vpn_host.connected) {
				try {
					std::cout << "  " << BOLD << "Name:     " << RESET << curr_vpn_host.name << std::endl;
					std::cout << "  " << BOLD << "Hostname: " << RESET << curr_vpn_host.hostname \
					<< RESET " {" << ((!net.expected_vpn_hostname.empty() && 
						std::regex_match(curr_vpn_host.hostname, std::regex(net.expected_vpn_hostname, std::regex_constants::icase)))
						? GREEN "MATCH" : RED "NO MATCH") << RESET << "}\n";
					std::cout << "  " << BOLD << "Local IP: " << RESET << curr_vpn_host.local_ip << std::endl;
					linecount += 3;
				}
				catch (const std::regex_error& ex) {
					// print a msg about the user being bad at regexes , print context, and then raise the error again to halt execution
					std::cerr << RED BOLD << "ERROR: Invalid regex in config for expected_vpn_hostname: " << net.expected_vpn_hostname << std::endl;
					std::cerr << "- Please fix the regex pattern in conf.toml and restart the program." << RESET << std::endl ;
					std::cerr << "Regex error details: " << ex.what() << std::endl;
					throw ex;
				}
			}
			else {
				std::cout << YELLOW "  No active VPN connection detected\n" RESET << std::endl ;
				linecount++;
			}
			// session info
			// first generate a timestamp
			long long remaining = session.upTimeSec;
			long long d = remaining / 86400;
			remaining %= 86400;
			long long h = remaining / 3600;
			remaining %= 3600;
			long long m = remaining / 60;
			long long s = remaining % 60;
			std::ostringstream t_oss;
			t_oss << std::setfill('0') \
				<< d << "d " \
				<< std::setw(2) << h << ":" \
				<< std::setw(2) << m << ":" \
				<< std::setw(2) << s \
				// add raw second counter
				<< " (" << session.upTimeSec << ")" ;
			std::string uptime_ts = t_oss.str();
			// then print
			std::cout << RESET BOLD << "Session Information:" << RESET << std::endl;
			std::cout << "  " << BOLD << "Active user:     " << RESET << session.user << std::endl;
			std::cout << "  " << BOLD << "Domain:          " << RESET << session.domain << std::endl;
			std::cout << "  " << BOLD << "Session ID:      " << RESET << session.sessionId << std::endl;
			std::cout << "  " << BOLD << "Logged in users: " << RESET << session.loggedInCount << std::endl;
			std::cout << "  " << BOLD << "Hostname:        " << RESET << session.hostname << std::endl;
			std::cout << "  " << BOLD << "Uptime:          " << RESET << uptime_ts << std::endl;
			linecount += 7;
			// local drives
			std::cout << "\n" << BOLD << "Drives:" << RESET << std::endl;
			linecount++;
			size_t max_width_locals = 0;
			for (int st = 0; st < curr_drives.size(); ++st) {
				std::string s = disks.locals[st];
				std::string s_l = disks.locals_labels[st];
				size_t new_size = s.size() + s_l.size() + 2 + (s_l.size() != 0 ? 3 : 0);
				if (new_size > max_width_locals) max_width_locals = new_size;
			}
			for (int st = 0; st < curr_drives.size(); st++) {
				linecount++;
				bool status = curr_drives[st];
				std::cout << (status ? GREEN : (disks.locals_imp[st] == 0 ? YELLOW : RED))
				<< "  " << std::setw(static_cast<int>(max_width_locals)) << std::left \
				<< (disks.locals_labels[st].size() != 0 ? disks.locals_labels[st] + RESET WHITE " - " RESET + (status ? GREEN : \
					(disks.locals_imp[st] == 0 ? YELLOW : RED)) + disks.locals[st] : disks.locals[st] ) + ":\\" \
				<< BOLD WHITE << " : " << (status ? GREEN "OK" : RED "FAIL") << RESET << std::endl;
			}
			// and unc paths
			std::cout << "\n" << BOLD << "UNC:" << RESET << std::endl;
			linecount++;
			size_t max_width_unc = 0;
			for (int st = 0; st < curr_unc.size(); ++st) {
				std::string s = disks.unc[st];
				std::string s_l = disks.unc_labels[st];
				size_t new_size = s.size() + s_l.size() + (s_l.size() != 0 ? 3 : 0);
				if (new_size > max_width_unc) max_width_unc = new_size;
			}
			for (int st = 0; st < curr_unc.size(); st++) {
				linecount++;
				bool status = curr_unc[st];
				std::cout << (status ? GREEN : (disks.unc_imp[st] == 0 ? YELLOW : RED))
				<< "  " << std::setw(static_cast<int>(max_width_unc)) << std::left \
				<< (disks.unc_labels[st].size() != 0 ? disks.unc_labels[st] + RESET WHITE " - " RESET + (status ? GREEN : \
					(disks.unc_imp[st] == 0 ? YELLOW : RED)) + disks.unc[st] : disks.unc[st] ) \
				<< BOLD WHITE << " : " << (status ? GREEN "OK" : RED "FAIL") << RESET << std::endl;
			}
			// extra info (meta)
			std::cout << std::endl << BOLD << MAGENTA << "Monitoring..." << RESET << std::endl;
			std::cout << BLUE << "Log path: " << log_path.string().c_str() << RESET << std::endl << std::endl;
			linecount += 2;

			// update window size
			PrepareTerminal(linecount);

			// Perform the 1s timed loop
			auto time_end = std::chrono::steady_clock::now();
			if (timer_start >= (time_end - std::chrono::seconds(1))) {
				// Grace period to avoid false positives the first second of execution
				status_check(true); // perform a status check without logging
				sleep_until_next_tick(loop_start);
				// do not continue execution, do a new loop
				continue;
			}
			status_check();
			sleep_until_next_tick(loop_start);
		}
		WSACleanup();
		std::cout << WRAP; std::cout.flush();
		return 0;
	}
	// These should force a clean exit on exceptions
	// Including closing the socket to avoid instability in the network stack on next run, since microslop code can be weird about that.
	catch (const std::exception& ex) {
		std::cerr << RED << "Unhandled exception: " << ex.what() << RESET << std::endl;
		WSACleanup();
		std::cout << WRAP; std::cout.flush();
		return 1;
	}
	catch (...) {
		std::cerr << RED << "Unknown exception occurred." << RESET << std::endl;
		WSACleanup();
		std::cout << WRAP; std::cout.flush();
		return 1;
	}
	WSACleanup();
		std::cout << WRAP; std::cout.flush();
	return 0;
}
