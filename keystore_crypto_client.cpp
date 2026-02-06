/**
 * KeystoreCrypto Client Implementation
 * Phase 3.13: PAL→VCS MAC共通鍵管理
 */

#include "keystore_crypto_client.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <syslog.h>

#include <cstring>
#include <cerrno>
#include <sstream>
#include <iomanip>

namespace keystore_crypto {

KeystoreCryptoClient::KeystoreCryptoClient() {
    syslog(LOG_DEBUG, "[KeystoreCryptoClient] Initialized");
}

KeystoreCryptoClient::~KeystoreCryptoClient() {
}

std::string KeystoreCryptoClient::sendCommand(const std::string& request) {
    // Create abstract namespace socket
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        syslog(LOG_ERR, "[KeystoreCryptoClient] Failed to create socket: %s", strerror(errno));
        return "";
    }

    // Connect to KeystoreCrypto service
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    // Abstract namespace: first byte is '\0'
    addr.sun_path[0] = '\0';
    strncpy(addr.sun_path + 1, SOCKET_NAME, sizeof(addr.sun_path) - 2);

    socklen_t addr_len = offsetof(struct sockaddr_un, sun_path) + 1 + strlen(SOCKET_NAME);

    if (connect(sock, (struct sockaddr*)&addr, addr_len) < 0) {
        syslog(LOG_ERR, "[KeystoreCryptoClient] Failed to connect to @%s: %s",
               SOCKET_NAME, strerror(errno));
        close(sock);
        return "";
    }

    // Send request with newline
    std::string request_with_newline = request + "\n";
    ssize_t sent = send(sock, request_with_newline.c_str(), request_with_newline.length(), 0);
    if (sent < 0) {
        syslog(LOG_ERR, "[KeystoreCryptoClient] Failed to send: %s", strerror(errno));
        close(sock);
        return "";
    }

    // Receive response
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    ssize_t received = recv(sock, buffer, sizeof(buffer) - 1, 0);

    close(sock);

    if (received <= 0) {
        syslog(LOG_ERR, "[KeystoreCryptoClient] Failed to receive: %s", strerror(errno));
        return "";
    }

    // Remove trailing newline
    std::string response(buffer);
    if (!response.empty() && response.back() == '\n') {
        response.pop_back();
    }

    return response;
}

std::vector<uint8_t> KeystoreCryptoClient::generateKey() {
    syslog(LOG_INFO, "[KeystoreCryptoClient] Generating ECDH key pair...");

    std::string response = sendCommand("GENERATE_KEY");
    if (response.empty()) {
        return {};
    }

    if (response.substr(0, 3) != "OK:") {
        syslog(LOG_ERR, "[KeystoreCryptoClient] generateKey failed: %s", response.c_str());
        return {};
    }

    std::string pubkey_hex = response.substr(3);
    auto pubkey = hexToBytes(pubkey_hex);

    syslog(LOG_INFO, "[KeystoreCryptoClient] Key generated, public key: %s...",
           pubkey_hex.substr(0, 16).c_str());

    return pubkey;
}

std::vector<uint8_t> KeystoreCryptoClient::getPublicKey() {
    std::string response = sendCommand("GET_PUBLIC_KEY");
    if (response.empty()) {
        return {};
    }

    if (response.substr(0, 3) != "OK:") {
        syslog(LOG_ERR, "[KeystoreCryptoClient] getPublicKey failed: %s", response.c_str());
        return {};
    }

    return hexToBytes(response.substr(3));
}

bool KeystoreCryptoClient::deriveKey(const std::vector<uint8_t>& peer_public_key) {
    syslog(LOG_INFO, "[KeystoreCryptoClient] Deriving shared key...");

    std::string request = "DERIVE_KEY:" + bytesToHex(peer_public_key);
    std::string response = sendCommand(request);

    if (response != "OK") {
        syslog(LOG_ERR, "[KeystoreCryptoClient] deriveKey failed: %s", response.c_str());
        return false;
    }

    syslog(LOG_INFO, "[KeystoreCryptoClient] Shared key derived successfully");
    return true;
}

std::vector<uint8_t> KeystoreCryptoClient::computeMac(const std::vector<uint8_t>& payload) {
    std::string request = "COMPUTE_MAC:" + bytesToHex(payload);
    std::string response = sendCommand(request);

    if (response.empty()) {
        return {};
    }

    if (response.substr(0, 3) != "OK:") {
        syslog(LOG_ERR, "[KeystoreCryptoClient] computeMac failed: %s", response.c_str());
        return {};
    }

    return hexToBytes(response.substr(3));
}

// Utility functions

std::string bytesToHex(const std::vector<uint8_t>& bytes) {
    std::ostringstream oss;
    for (uint8_t b : bytes) {
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    bytes.reserve(hex.length() / 2);
    for (size_t i = 0; i + 1 < hex.length(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        bytes.push_back(static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16)));
    }
    return bytes;
}

} // namespace keystore_crypto
