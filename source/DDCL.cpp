// Copyright (c) DVP-F/Carnx00 2026 <carnx@duck.com>
// License : GNU GPL 3.0 (https://www.gnu.org/licenses/gpl-3.0.html) supplied with the package under `LICENSES`
// Source code hosts:
// - GitHub: https://github.com/DVP-F/DDCL 
// See `NOTICE.txt` for further Licensing information

#include <cstddef> // apparently my distro doesnt have cstddef, but im compiling on windows for now so idrc
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

// and this is intended for MinGW (MSYS2) cross-compilation, defines some stuff that not all non-native toolchains have in their Windows headers
#ifdef __MINGW32__ 

// MinGW windns.h missing Windows 10+ DNS Ex API
typedef IP4_ARRAY *PIP4_ARRAY;

typedef struct _DNS_ADDR {
  CHAR MaxSa[DNS_MAX_SOCKADDR_LENGTH];  // sockaddr_in or sockaddr_in6
  DWORD DnsAddrUserDword[8];            // Reserved, must be 0
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

#endif  // __MINGW32__

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

// config message flag
static bool show_config = false;

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
locals = [
	"C",
]
# unc paths are network resources to check eg. smb shares etc. (Including NAS storage) 
# quad backslashes per single backslash in the final path due to parsing by C++ string literal and toml parser.
unc = [
	# define as high importance by prefixing UNC paths with `#!`
	# localhosts can be used to test the virtual loopback adapter too - if the path is a shared folder or otherwise available. 
	"#!\\\\localhost\\C",
	# add `$username` to use the current user's username in a path, for example to check the user's home directory 
	#  !!  (UNC only; intended for AD/AAD where a home folder is set up on a file server)  !!
	# obligatory reminder that this is string expansion, not parametrized queries. ensure the envvar %USERNAME% is safe.
	"\\\\localhost\\Users\\$username",
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
	std::cout << "No conf.toml found, creating default at " << path << "\n";
	std::ofstream out(path, std::ios::binary);
	if (!out) {
		std::cerr << "Failed to create " << path << "\n";
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
	std::vector<std::string> unc;
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
		std::string nameUtf8 = wstring_to_utf8_string(p->FriendlyName);
		std::string descUtf8 = wstring_to_utf8_string(p->Description);
		std::string hostUtf8 = (p->DnsSuffix != nullptr) ? wstring_to_utf8_string(p->DnsSuffix) : "";
		std::string nameLower = nameUtf8;
		std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
		std::string descLower = descUtf8;
		std::transform(descLower.begin(), descLower.end(), descLower.begin(), ::tolower);
		if (p->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue; // explicitly skip loopback interfaces
		if ((nameLower.find("vpn") != std::string::npos ||
			descLower.find("vpn") != std::string::npos) &&
			p->OperStatus == IfOperStatusUp) {
			// Get first IPv4
			PIP_ADAPTER_UNICAST_ADDRESS ua = p->FirstUnicastAddress;
			if (ua && ua->Address.lpSockaddr->sa_family == AF_INET) {
				sockaddr_in* ip = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
				char ipStr[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &ip->sin_addr, ipStr, sizeof(ipStr));
				VpnConnection vpn;
				vpn.connected = true;
				vpn.name = nameUtf8.empty() ? p->AdapterName : nameUtf8;
				if      (!hostUtf8.empty()) { vpn.hostname = hostUtf8; }
				else if (!descUtf8.empty()) { vpn.hostname = descUtf8; }
				else                        { vpn.hostname = "VPN_Interface"; }
				vpn.local_ip = ipStr;
				return vpn;
			}
		}
	}
	return std::nullopt;
}

static std::optional<VpnConnection> get_active_vpn() {
	DWORD dwConnections = 0; DWORD dwSize = 0;
	// First call to get required buffer size
	DWORD firstResult = RasEnumConnectionsA(NULL, &dwSize, &dwConnections);
	// ignore errors. we ball. 
	// Allocate buffer and set dwSize for first struct
	std::vector<BYTE> buffer(dwSize);
	auto* connections = reinterpret_cast<RASCONNA*>(buffer.data());
	if (connections != nullptr && dwSize >= sizeof(RASCONNA)) {
		connections[0].dwSize = sizeof(RASCONNA);  // REQUIRED!
		// Second call to actually enumerate
		DWORD actualCount = 0;
		DWORD result = RasEnumConnectionsA(connections, &dwSize, &actualCount);
		if (result == 0) {
			HRASCONN hRasConn = connections[0].hrasconn;
			// Get connection status
			RASCONNSTATUSA status;
			status.dwSize = sizeof(RASCONNSTATUS);
			DWORD cbStatus = sizeof(status);
			if (RasGetConnectStatusA(hRasConn, &status) <= 0) { // check for specific problems instead of exiting
				if (status.rasconnstate <= RASCS_Connected) {
					// Get entry name - USE RasGetEntryProperties directly
					char entryName[256] = { 0 };
					DWORD entrySize = sizeof(entryName);
					// First enum entries to find matching name
					DWORD dwEntries = 0, dwEntrySize = 0;
					RasEnumEntriesA(NULL, NULL, NULL, &dwEntrySize, &dwEntries);
					std::vector<RASENTRYNAMEA> entries(dwEntrySize / sizeof(RASENTRYNAMEA));
					RasEnumEntriesA(NULL, NULL, entries.data(), &dwEntrySize, &dwEntries);
					for (DWORD i = 0; i < dwEntries; i++) {
						RASENTRYA entry;
						entry.dwSize = sizeof(RASENTRYA);
						if (RasGetEntryPropertiesA(NULL, entries[i].szEntryName, &entry, &entry.dwSize, NULL, NULL) == 0) {
							VpnConnection vpn;
							vpn.connected = true;
							vpn.name = entries[i].szEntryName;
							// TRY these fields for hostname (ProtonVPN puts it here):
							vpn.hostname = entry.szLocalPhoneNumber;  // Often hostname
							if (vpn.hostname.empty() || vpn.hostname == "0") {
								vpn.hostname = entry.szDeviceName;     // Fallback
							}
							// IP from status
							vpn.local_ip = status.szPhoneNumber; // IP is often held in phonenumber field for some reason
							return vpn;
						}
					}
				}
			}
		}
	}
	// NOW check network interfaces for IKEv2 VPNs (RasSstp, AgileVPN, etc.)
	// This wont be fun to make a technical description for.
	// The walk goes through RAS detections and ends up walking network interfaces bruh
	//// And stupidly enough its needed for some VPNs. I know one thats PPP/IKEv2, Intune managed as a WAN Miniport.
	//// Cannot find it any other way, and i dont know any way to get the hostname field, thus support for regex 
	//// (since the hostname get falls back to desc/name, you can match that instead.)
	ULONG bufSize = 0;
	GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_ALL_INTERFACES | GAA_FLAG_SKIP_DNS_SERVER, NULL, NULL, &bufSize);
	PIP_ADAPTER_ADDRESSES pAddrs = (PIP_ADAPTER_ADDRESSES)malloc(bufSize);
	if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_ALL_INTERFACES | GAA_FLAG_SKIP_DNS_SERVER, NULL, pAddrs, &bufSize) == NO_ERROR) {
		for (PIP_ADAPTER_ADDRESSES p = pAddrs; p; p = p->Next) {
			// Check for active VPN interface types 
			if (p->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
			// skip loopback interfaces - some VPNs use loopback type but we want to avoid normal loopback adapters 
			// id rather the other checks detect the loopback vpns through other means 
			if (p->OperStatus == IfOperStatusUp && (
				p->IfType == IF_TYPE_PPP ||                    // PPP (traditional VPN)
				//p->IfType == IF_TYPE_SOFTWARE_LOOPBACK ||      // RasMan virtual //! detects normal loopback adapters too
				p->IfType == 131 ||                            // IF_TYPE_IKEV2_CORRELATOR
				strstr(p->AdapterName, "RasSstp") ||           // SSTP
				strstr(p->AdapterName, "AgileVPN") ||          // IKEv2
				strstr(p->AdapterName, "wanarp") ||            // WAN ARP miniport
				strstr(p->AdapterName, "RasAgileVpn")          // IKEv2 miniport
				)) {
				// Get first IPv4 address
				PIP_ADAPTER_UNICAST_ADDRESS pUnicast = p->FirstUnicastAddress;
				if (pUnicast) {
					sockaddr_in* ip = (sockaddr_in*)pUnicast->Address.lpSockaddr;
					char ipStr[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &ip->sin_addr, ipStr, sizeof(ipStr));
					VpnConnection vpn;
					vpn.connected = true;
					vpn.name = p->FriendlyName ? wstring_to_utf8_string(p->FriendlyName) : p->AdapterName;
					vpn.local_ip = ipStr;
					// Try to get hostname from description or name
					vpn.hostname = p->Description ? wstring_to_utf8_string(p->Description) : "IKEv2_VPN";
					// Clean up hostname - often contains server info
					if (vpn.hostname.find("Proton") != std::string::npos ||
						vpn.hostname.find("nlfree") != std::string::npos ||
						vpn.hostname.find("VPN") != std::string::npos) {
						// Keep as-is, likely contains server info
					}
					free(pAddrs);
					pAddrs = NULL;
					return vpn;
				}
			}
		}
	}
	if (pAddrs != NULL) {
		free(pAddrs);
		pAddrs = NULL;
	}
	// Then fallback to interface scan
	auto vpn = find_vpn_by_interface_name();
	if (vpn != std::nullopt) return vpn;
	return std::nullopt; // or fail, in which case theres probably no VPN online. (the other checks shouldve caught 99%+ of VPNs)
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
	uint8_t query[] = { 0xAB,0xCD, 0x01,0x00, 0x00,0x01, 0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x01 };
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
static bool http_head_probe(const char* url_cstr, DWORD timeout_ms = 250) {
	if (!url_cstr || !*url_cstr) return false;
	std::wstring wurl = to_wide(url_cstr);
	// Crack URL into components
	URL_COMPONENTSW uc{};
	uc.dwStructSize = sizeof(uc);
	uc.dwHostNameLength = uc.dwUrlPathLength = 1; // request lengths
	if (!InternetCrackUrlW(wurl.c_str(), 0, 0, &uc)) return false;
	// Copy host and path from cracked URL (points into wurl)
	std::wstring host(uc.lpszHostName ? uc.lpszHostName : L"", uc.dwHostNameLength);
	std::wstring path(uc.lpszUrlPath ? uc.lpszUrlPath : L"/", uc.dwUrlPathLength ? uc.dwUrlPathLength : 1);
	unsigned short port = uc.nPort ? static_cast<unsigned short>(uc.nPort) : (uc.nScheme == INTERNET_SCHEME_HTTPS ? 443 : 80);
	bool secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);
	HINTERNET hInternet = InternetOpenW(L"DDCLProbe", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (!hInternet) return false;
	HINTERNET hConnect = InternetConnectW(hInternet, host.c_str(), port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
	if (!hConnect) { InternetCloseHandle(hInternet); return false; }
	DWORD flags = INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD;
	if (secure) flags |= INTERNET_FLAG_SECURE;
	HINTERNET hRequest = HttpOpenRequestW(hConnect, L"HEAD", path.c_str(), NULL, NULL, NULL, flags, 0);
	if (!hRequest) { InternetCloseHandle(hConnect); InternetCloseHandle(hInternet); return false; }
	// Set timeouts
	DWORD to = timeout_ms;
	InternetSetOptionW(hRequest, INTERNET_OPTION_CONNECT_TIMEOUT, &to, sizeof(to));
	InternetSetOptionW(hRequest, INTERNET_OPTION_SEND_TIMEOUT, &to, sizeof(to));
	InternetSetOptionW(hRequest, INTERNET_OPTION_RECEIVE_TIMEOUT, &to, sizeof(to));
	BOOL sent = HttpSendRequestW(hRequest, NULL, 0, NULL, 0);
	bool ok = false;
	if (sent) {
		DWORD status = 0;
		DWORD sz = sizeof(status);
		if (HttpQueryInfoW(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &sz, NULL)) {
			ok = (status >= 200 && status < 400);
		}
	}
	InternetCloseHandle(hRequest); InternetCloseHandle(hConnect); InternetCloseHandle(hInternet);
	return ok;
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

static std::vector<bool> resolve_hostname(const std::string& hostname, const std::string& dns_server_ip) {
	// in total ~500ms timeout if both queries time out or are deferred
	std::vector<bool> results(2, false);
	// FIRST: Custom DNS server
	{
		DNS_QUERY_REQUEST req = {};
		req.Version = DNS_QUERY_REQUEST_VERSION1;
		std::wstring wname = to_wide(const_cast<char*>(hostname.c_str()));
		req.QueryName = wname.c_str();
		req.QueryType = DNS_TYPE_A;
		sockaddr_in dns_addr = {};
		dns_addr.sin_family = AF_INET;
		inet_pton(AF_INET, dns_server_ip.c_str(), &dns_addr.sin_addr);
		DNS_ADDR_ARRAY dns_servers = {};
		dns_servers.MaxCount = 1;
		dns_servers.AddrCount = 1;
		dns_servers.Tag = 0;
		dns_servers.Family = AF_INET;
		dns_servers.WordReserved = 0;
		dns_servers.Flags = 0;
		dns_servers.MatchFlag = 0;
		dns_servers.Reserved1 = 0;
		dns_servers.Reserved2 = 0;
		std::memset(&dns_servers.AddrArray[0], 0, sizeof(dns_servers.AddrArray[0]));
		std::memcpy(dns_servers.AddrArray[0].MaxSa, &dns_addr, sizeof(dns_addr));
		req.pDnsServerList = &dns_servers;
		auto future = std::async(std::launch::async, [&]() -> std::pair<bool, DNS_RECORD*> {
			DNS_RECORD* results_custom_local = nullptr;
			DNS_STATUS status_local = DnsQuery_A(hostname.c_str(), DNS_TYPE_A, DNS_QUERY_STANDARD, &req, &results_custom_local, nullptr);
			bool success = (status_local == ERROR_SUCCESS && results_custom_local != nullptr);
			return { success, results_custom_local };
			});
		auto future_status = future.wait_for(std::chrono::milliseconds(250));
		if (future_status == std::future_status::ready) {
			auto [success, results_custom] = future.get();
			results[0] = success;
			if (results_custom) {
				DnsRecordListFree(results_custom, DnsFreeRecordList);
			}
		}
		else {
			results[0] = false;  // Timeout or deferred
		}
	}
	// SECOND: System DNS (local)
	{
		addrinfo hints = {};
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		auto future = std::async(std::launch::async, [&]() -> std::pair<bool, addrinfo*> {
			addrinfo* result_local = nullptr;
			int gai_status_local = getaddrinfo(hostname.c_str(), nullptr, &hints, &result_local);
			bool success = (gai_status_local == 0 && result_local != nullptr);
			return { success, result_local };
			});
		auto future_status = future.wait_for(std::chrono::milliseconds(250));
		if (future_status == std::future_status::ready) {
			auto [success, result] = future.get();
			results[1] = success;
			if (result) {
				freeaddrinfo(result);
			}
		}
		else {
			results[1] = false;  // Timeout or deferred
		}
	}
	return results;
}

static std::array<const char*, 2> GetEthernetNetworkInfo() {
	static std::string storage[2];  // Static lifetime for returned pointers
	std::array<const char*, 2> result = { nullptr, nullptr };
	ULONG bufLen = 15 * 1024;
	PIP_ADAPTER_ADDRESSES pAddrs = (PIP_ADAPTER_ADDRESSES)malloc(bufLen);
	DWORD ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX,
		nullptr, pAddrs, &bufLen);
	if (ret == ERROR_BUFFER_OVERFLOW) {
		free(pAddrs);
		pAddrs = (PIP_ADAPTER_ADDRESSES)malloc(bufLen);
		ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX,
			nullptr, pAddrs, &bufLen);
	}
	if (ret == NO_ERROR) {
		for (PIP_ADAPTER_ADDRESSES p = pAddrs; p; p = p->Next) {
			if (p->IfType == IF_TYPE_ETHERNET_CSMACD &&
				p->OperStatus == IfOperStatusUp &&
				p->FriendlyName) {
				storage[0] = wstring_to_utf8_string(p->FriendlyName);
				result[0] = storage[0].c_str();
				if (p->DnsSuffix && *p->DnsSuffix) {
					storage[1] = wstring_to_utf8_string(p->DnsSuffix);
					result[1] = storage[1].c_str();
				}
				break;
			}
		}
	}
	free(pAddrs);
	return result;
}

static bool is_unc_available(const char* unc) {
	auto future = std::async(std::launch::async, [&]() {
		DWORD attr = GetFileAttributesA(unc);
		return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
		});
	return (future.wait_for(std::chrono::milliseconds(250)) == std::future_status::ready ? future.get() : false);
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

// Global initalized string storage
struct Store {
	std::vector<std::string> strings;
};
Store storage;

// status vars
bool prev_internet = false;
std::vector<bool> prev_resolve_by_dns(4, false);
std::string prev_dns_suffix;
VpnConnection prev_vpn_host;
std::vector<bool> prev_drives(false);
std::vector<bool> prev_unc;
std::array<const char*, 2> prev_eth_info;

bool curr_internet = false;
std::vector<bool> curr_resolve_by_dns(4, false);
std::string curr_dns_suffix;
VpnConnection curr_vpn_host;
std::vector<bool> curr_drives(false);
std::vector<bool> curr_unc;
std::array<const char*, 2> curr_eth_info;

static std::filesystem::path conf_path;
static std::filesystem::path log_path;
static NetworkConfig net;
static DiskConfig disks;
const char* eth_friendly_name = nullptr;
static bool use_vt = true;
static bool vt_enabled = false;

const enum class status_change_type {
	internet_connectivity,
	ethernet,
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
#define _STRING__VOIDP(strp) reinterpret_cast<void*>(strp)
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
		std::cerr << "Permission denied when creating log directory: " << dir.string() << "\n";
		std::cerr << "Please check permissions or specify a different log path in conf.toml\n";
	}
	else if (ec == std::errc::no_such_file_or_directory) {
		std::cerr << "Invalid path when creating log directory: " << dir.string() << "\n";
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
			std::cerr << "FLUSH FAILED after write! path=[" << log_path.string() << "]\n";
			DWORD err = GetLastError();
			std::cerr << "  GetLastError()=" << err << "\n";
			return;
		}
		out.close();  // Explicit close
		//std::cerr << "SUCCESS wrote to [" << log_path.string() << "] size=" << std::filesystem::file_size(log_path) << "\n";
		if (!out) {
			std::cerr << "Failed to open log file: " << log_path.string() << "\n";
			return;
		}
	}
	catch (const std::exception& ex) {
		std::cerr << "write_to_log exception: " << ex.what() << "\n";
	}
};

static void log_change(const status_change_type diff, std::string time, void* info = nullptr) {
	// This function converts the given signals into a string passed to write_to_log in a CSV format.
	// called with type of change, timestamp, and optional info (used for drive letter, UNC path, etc.) specific to the change type

	// Expectation: when `info` is provided for textual info it points to a `std::string`.
	// We make an internal copy into `storage.strings`
	void* info_copy = info;

	// These are going to be a bit monolithic but relatively straightforward
	// The given status_change_type determines the formatting of the log entry, 
	// for which the relevant info is already known - this just translates a lot of information into simple strings for logging.
	switch (diff) {
		case status_change_type::internet_connectivity: {
			std::ostringstream oss;
			oss << time << ",internet_connectivity," << (curr_internet ? "online" : "offline");
			write_to_log(oss.str());
			break;
		}

		case status_change_type::ethernet: {
			if (info_copy == nullptr || !info_copy) {
				std::ostringstream oss;
				const char* friendly = curr_eth_info[0] ? curr_eth_info[0] : "N/A";
				const char* suffix = curr_eth_info[1] ? curr_eth_info[1] : "";
				oss << time << ",ethernet," << friendly << ";" << suffix << ",";
				if (curr_dns_suffix != prev_dns_suffix) {
					oss << "dns_suffix_changed;(" << (!prev_dns_suffix.empty() ? prev_dns_suffix : "N/A") << ":" \
						<< (!curr_dns_suffix.empty() ? curr_dns_suffix : "N/A") << ")";
				}
				else {
					oss << "connection_status_changed";
				}
				write_to_log(oss.str());
			}
			else {
				const std::string* incoming = _VOIDP__STRING(info_copy);
				if (incoming) {
					storage.strings.push_back(*incoming); // borrow some global memory
					std::ostringstream oss;
					const char* friendly = curr_eth_info[0] ? curr_eth_info[0] : "N/A";
					const char* suffix = curr_eth_info[1] ? curr_eth_info[1] : "";
					oss << time << ",ethernet," << friendly << ";" << suffix << "," << storage.strings.back();
					write_to_log(oss.str());
				}
				else {
					std::ostringstream oss;
					oss << time << ",ethernet,UNKNOWN,info_unavailable";
					write_to_log(oss.str());
				}
			}
			break;
		}

		case status_change_type::dns_resolution: {
			std::ostringstream oss;
			oss << time << ",dns_resolution,"
				<< net.check << ":sys_dns" << (curr_resolve_by_dns[0] ? ";True " : ";False ")
				<< net.check << ":" << net.dns << (curr_resolve_by_dns[1] ? ";True " : ";False ")
				<< "www.wikipedia.org" << ":sys_dns" << (curr_resolve_by_dns[2] ? ";True " : ";False ")
				<< "www.wikipedia.org" << ":" << net.dns << (curr_resolve_by_dns[3] ? ";True" : ";False");
			write_to_log(oss.str());
			break;
		}

		case status_change_type::vpn_connection: {
			std::ostringstream oss;
			oss << time << ",vpn_connection,"
				<< (curr_vpn_host.connected ? "connected:" + curr_vpn_host.name + ";" + curr_vpn_host.hostname : "not_connected")
				<< " l_ip:" << (curr_vpn_host.connected ? curr_vpn_host.local_ip : "N/A");
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
					std::cerr << "Conversion exception: " << ex.what() << "\n";
					idx = 0;
				}
			}
			if (idx >= curr_drives.size()) {
				std::cerr << "Index out of range for curr_drives";
				return;
			}
			try {
				char driveLetter = _STRING__CHAR(&disks.locals[idx]);
				std::ostringstream oss;
				oss << time << ",drive_availability,"
					<< (curr_drives[idx] ? "available," : "unavailable,")
					<< driveLetter;
				write_to_log(oss.str());
			}
			catch (const std::exception& ex) {
				std::cerr << "\nError extracting drive letter in log_change: " << ex.what() << "\n";
				std::ostringstream oss;
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
			std::ostringstream oss;
			oss << time << ",unc_availability,"
				<< (curr_unc[idx] ? "available," : "unavailable,")
				<< uncPath;
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
				if (!cstr_equal(curr_eth_info[0], prev_eth_info[0]) || curr_dns_suffix != prev_dns_suffix) {
					if (!nowrite) {
						log_change(status_change_type::ethernet, c_time);
					}
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

	// store prev_*
	prev_internet = curr_internet;
	prev_resolve_by_dns = curr_resolve_by_dns;
	prev_eth_info = curr_eth_info;
	prev_dns_suffix = curr_dns_suffix;
	prev_vpn_host = curr_vpn_host;
	prev_drives = curr_drives;
	prev_unc = curr_unc;
}

static void update_status() {
	// This function is responsible for updating the curr_ status variables with the latest values from the system every second. 
	// Assign new values to curr_* - theyre saved to prev in status_check() after comparison and logging. 
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
				{
					curr_eth_info = GetEthernetNetworkInfo();
					if (curr_eth_info.size() != 2) {
						std::cerr << "GetEthernetNetworkInfo returned unexpected size: " << curr_eth_info.size() << "\n";
						curr_eth_info = { nullptr, nullptr };
					}
					else {
						eth_friendly_name = curr_eth_info[0];
						curr_dns_suffix = curr_eth_info[1] ? curr_eth_info[1] : std::string();
					}
					continue;
				}
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
							std::cerr << "Invalid drive letter in config: " << driveStr << "\n";
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
}

static std::size_t initial_status_write() {
	// Startup function to write initial status of all monitored items to log, called once after initalization and before entering monitoring loop
	update_status();
	const std::string c_time = get_timestamp();
	write_to_log("timestamp,kind,value,info"); // CSV header
	Sleep(250); // Ensure log file is created before writing initial status (and avoid potential race conditions) 

	// Log the registered detection kinds!
	std::string det_str = "";
	det_str.append(c_time);
	det_str.append(",registered_detections,");
	for (const auto& kind : detection_kinds) {
		switch (kind) {
			case status_change_type::internet_connectivity:
				det_str.append("internet_connectivity");
				break;
			case status_change_type::ethernet:
				det_str.append("ethernet");
				break;
			case status_change_type::dns_resolution:
				det_str.append("dns_resolution");
				break;
			case status_change_type::vpn_connection:
				det_str.append("vpn_connection");
				break;
			case status_change_type::drive_availability:
				det_str.append("drive_availability");
				break;
			case status_change_type::unc_availability:
				det_str.append("unc_availability");
				break;
		}
		det_str.append(";");
	}
	if (det_str.back()==';') {det_str.pop_back();}; det_str.append(",initial_status");
	write_to_log(det_str);

	for (const auto& kind : detection_kinds) {
		switch (kind) {
			case status_change_type::internet_connectivity:
				log_change(status_change_type::internet_connectivity, c_time);
				break;
			case status_change_type::dns_resolution:
				log_change(status_change_type::dns_resolution, c_time);
				break;
			case status_change_type::ethernet:
				log_change(status_change_type::ethernet, c_time);
				break;
			case status_change_type::vpn_connection:
				log_change(status_change_type::vpn_connection, c_time);
				break;
			case status_change_type::drive_availability:
				for (std::size_t i = 0; i < curr_drives.size(); i++) {
					log_change(status_change_type::drive_availability, c_time, _SIZE_T__VOIDP(i));
				}
				break;
			case status_change_type::unc_availability:
				for (std::size_t i = 0; i < curr_unc.size(); i++) {
					log_change(status_change_type::unc_availability, c_time, _SIZE_T__VOIDP(i));
				}
				break;
		}
	}
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
		std::cerr << "Error parsing conf.toml: " << err.description() << "\n";
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
				status_change_type::dns_resolution,
				status_change_type::vpn_connection,
				status_change_type::drive_availability,
				status_change_type::unc_availability
			};
			// TODO: add a trigger for defaults application and a cerr warn with user confirmation that defaults should be used
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
		if (auto arr = disks_tbl->operator[]("locals").as_array()) {
			for (auto& v : *arr)
				if (auto s = v.value<std::string_view>())
					disks.locals.emplace_back(s->data(), s->size());
		}
		if (auto arr = disks_tbl->operator[]("unc").as_array()) {
			for (auto& v : *arr)
				if (auto s = v.value<std::string_view>()) {
					// Expand $username placeholder at config-load time
					std::string unc = std::string(s->data(), s->size());
	#pragma warning(disable:4996)
					const char* userEnvCfg = getenv("USERNAME");
	#pragma warning(default:4996)
					std::string username = userEnvCfg ? std::string(userEnvCfg) : std::string();
					const std::string placeholder = "$username";
					std::size_t pos = 0;
					while ((pos = unc.find(placeholder, pos)) != std::string::npos) {
						unc.replace(pos, placeholder.size(), username);
						pos += username.size();
					}
					disks.unc.emplace_back(std::move(unc));
					// update unc_imp based off s[0:1]
					if (s->size() > 2 && s->substr(0, 2) == "#!") {
						disks.unc_imp.push_back(1); // high importance
						disks.unc.back().erase(0, 2); // remove the importance marker from the actual path
					}
					else {
						disks.unc_imp.push_back(0); // normal importance
					}
				}
		}
	}

	// Initialize curr_ vars to safe defaults to avoid undefined behavior on first update_status() 
	curr_resolve_by_dns = std::vector<bool>{ false, false, false, false };
	curr_eth_info		= std::array<const char*, 2>();
	curr_dns_suffix.clear();
	curr_vpn_host		= VpnConnection();
	curr_drives			= std::vector<bool>(disks.locals.size(), false);
	curr_unc			= std::vector<bool>(disks.unc.size(), false);

	// Initialize prev_ vars
	prev_internet		= false;
	prev_resolve_by_dns = std::vector<bool>{ false, false, false, false };
	prev_eth_info		= std::array<const char*, 2>();
	prev_dns_suffix.clear();
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

static void print_config_summary() {
	// This handles the -c --config option. Just prints the effective configuration after parsing conf.toml
	std::cout << BOLD << "Disk Drive Connection Logger (DDCL) - Startup Summary\n" << RESET;
	std::cout << "Commandline arguments: " << BOLD << "--config";
	std::cout << "\nEnabled Detections:\n  " << RESET GREEN;
	for (const auto& kind : detection_kinds) {
		switch (kind) {
			case status_change_type::internet_connectivity:
				std::cout << "Internet Connectivity, ";
				break;
			case status_change_type::ethernet:
				std::cout << "Ethernet, ";
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
	std::cout << "\n";
	std::cout << RESET BOLD << "Log Path: " << RESET BLUE << log_path.string() << RESET << "\n";
	std::cout << BOLD << "Virtual Terminal Processing: " << RESET YELLOW << (use_vt ? "Requested" : "Not Requested");
	if (use_vt) {
		std::cout << RESET BOLD << ", Status: " << (vt_enabled ? GREEN "ON" : YELLOW "OFF") << RESET << "\n\n"; // This will be set in initialize_runtime after attempting to enable VT
	}
	std::cout << "Network:" << "\n";
	std::cout << "  Check Host: " << BOLD << net.check << RESET << "\n";
	std::cout << "  DNS Server: " << BOLD << net.dns << RESET << "\n";
	std::cout << "  Expected Domain Suffix: " << BOLD << net.expected_domain << RESET << "\n";
	std::cout << "  Expected VPN Hostname: " << BOLD << net.expected_vpn_hostname << RESET << "\n\n";
	std::cout << "Disks:" << "\n";
	std::cout << "  Local Drives: ";
	for (const auto& drive : disks.locals) {
		std::cout << BOLD << drive << RESET << " ";
	}
	std::cout << "\n  UNC paths:\n";
	for (std::size_t i = 0; i < disks.unc.size(); i++) {
		int importance = disks.unc_imp[i];
		std::cout << (importance == 1 ? RED : YELLOW) << "    " << BOLD << disks.unc[i] << RESET << "\n";
	}
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
		std::cerr << "Too many arguments provided.\nUsage: DDCL.exe [-c|--config]\n";
		throw std::invalid_argument("Too many arguments provided");
	}
	if (argc == 2) { // 2 arguments since arg 0 is the binary call
		if (std::string_view(argv[1]) == "-c" || std::string_view(argv[1]) == "--config") {
			show_config = true;
		}
	}

	if (show_config) {
		print_config_summary();
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
		while (g_running.load()) {
			auto loop_start = std::chrono::steady_clock::now();
			update_status();

			// Just a wall of logic for the status dump
			std::cout << CLEAR;
			std::cout << BOLD << CYAN << "=== [" << get_timestamp() << "] Network & Drive Status ===\n" << RESET;
			std::cout << BOLD << "Internet: " << RESET;
			std::cout << (curr_internet ? GREEN "ONLINE" : RED "OFFLINE") << RESET << "\n";
			std::cout << BOLD << "DNS Resolution:\n" << RESET;
			std::cout << "  " << BOLD << net.check << ":\n" << RESET << "    Local DNS: " << \
				(curr_resolve_by_dns[0] ? GREEN "RESOLVED" : RED "FAILED") << RESET << "\n";
			std::cout << "    " << net.dns << ": " << \
				(curr_resolve_by_dns[1] ? GREEN "RESOLVED" : RED "FAILED") << RESET << "\n";
			std::cout << "  " << BOLD << "www.wikipedia.org" << ":\n" << RESET << "    Local DNS: " << \
				(curr_resolve_by_dns[2] ? GREEN "RESOLVED" : RED "FAILED") << RESET << "\n";
			std::cout << "    " << net.dns << ": " << \
				(curr_resolve_by_dns[3] ? GREEN "RESOLVED" : RED "FAILED") << RESET << "\n";
			std::cout << BOLD << "Ethernet Adapter Info:\n" << RESET;
			std::cout << "  " << BOLD << "Friendly Name: " << RESET << (eth_friendly_name ? eth_friendly_name : "N/A") << "\n";
			std::cout << "  " << BOLD << "DNS Suffix:    " << RESET << (!curr_dns_suffix.empty() ? curr_dns_suffix : "N/A") << "\n";
			std::cout << "  " << BOLD << "Matches Expected Domain: " << RESET << \
				(!curr_dns_suffix.empty() && net.expected_domain == curr_dns_suffix ? GREEN "YES" : RED "NO") << RESET << "\n";
			std::cout << BOLD << "VPN Info:\n" << RESET;
			if (curr_vpn_host.connected) {
				try {
					std::cout << "  " << BOLD << "VPN Name: " << RESET << curr_vpn_host.name << "\n";
					std::cout << "  " << BOLD << "VPN Hostname: " << RESET << curr_vpn_host.hostname << "\n";
					std::cout << "  " << BOLD << "VPN Local IP: " << RESET << curr_vpn_host.local_ip << "\n";
					std::cout << "  " << BOLD << "VPN Matches Expected Hostname: " << RESET << \
						((!net.expected_vpn_hostname.empty() && std::regex_match(curr_vpn_host.hostname, std::regex(net.expected_vpn_hostname, std::regex_constants::icase))) ? GREEN "YES" : RED "NO") << RESET << "\n";
				}
				catch (const std::regex_error& ex) {
					// print a msg about the user being bad at regexes , print context, and then raise the error again to halt execution
					std::cerr << RED BOLD << "ERROR: Invalid regex in config for expected_vpn_hostname: " << net.expected_vpn_hostname << "\n";
					std::cerr << "- Please fix the regex pattern in conf.toml and restart the program.\n" << RESET;
					std::cerr << "Regex error details: " << ex.what() << "\n";
					throw ex;
				}
			}
			else {
				std::cout << YELLOW "  No active VPN connection detected\n" RESET;
			}
			std::cout << "\n" << BOLD << "Drives:\n" << RESET;
			for (int st = 0; st < curr_drives.size(); st++) {
				bool status = curr_drives[st];				// actually fine bc they're synced
				std::cout << (status ? GREEN : RED) << "  " << disks.locals[st] << " " << (status ? "OK" : "FAIL") << RESET << "\n";
			}
			std::cout << "\n" << BOLD << "UNC:\n" << RESET;
			for (int st = 0; st < curr_unc.size(); st++) {
				bool status = curr_unc[st];
				std::cout << (status ? GREEN : (disks.unc_imp[st] == 0 ? YELLOW : RED)) << "  " << disks.unc[st] << " " << (status ? "OK" : "FAIL") << RESET << "\n";
			}
			std::cout << "\n" << BOLD << MAGENTA << "Monitoring...\n" << RESET;
			std::cout << BLUE << "Log path: " << log_path.string().c_str() << "\n" << RESET << std::endl;

			// Perform the 1s timed loop
			auto time_end = std::chrono::steady_clock::now();
			if (timer_start >= (time_end - std::chrono::seconds(1))) {
				// Grace period to avoid false positives the first second
				status_check(true); // perform a status check without logging
				// Also clamp sleep time to 0 <= ... <= 1 seconds VERY defensively 
				// - this keeps the loop timing as synchronized as possible without starting on multithreading
				std::this_thread::sleep_for(max(std::chrono::seconds{ 0 }, std::chrono::seconds(1) - min(std::chrono::seconds{ 1 }, time_end - loop_start)));
				// do not continue execution, do a new loop
				continue;
			}
			status_check();
			std::this_thread::sleep_for(max(std::chrono::seconds{ 0 }, std::chrono::seconds(1) - min(std::chrono::seconds{ 1 }, time_end - loop_start)));
		}
		WSACleanup();
		return 0;
	}
	// These should force a clean exit on exceptions
	// Including closing the socket to avoid instability in the network stack on next run, since microslop code can be weird about that.
	catch (const std::exception& ex) {
		std::cerr << RED << "Unhandled exception: " << ex.what() << RESET << std::endl;
		WSACleanup();
		return 1;
	}
	catch (...) {
		std::cerr << RED << "Unknown exception occurred." << RESET << std::endl;
		WSACleanup();
		return 1;
	}
	WSACleanup();
	return 0;
}
