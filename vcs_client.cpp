/**
 * VCS Client Implementation
 * Phase 3.13: PAL→VCS MAC共通鍵管理
 */

#include "vcs_client.h"
#include "keystore_crypto_client.h"  // for bytesToHex, hexToBytes

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <syslog.h>

#include <cstring>
#include <cerrno>

namespace vcs {

VcsClient::VcsClient(const std::string& host, int port)
    : host_(host), port_(port), sock_(-1) {
    memset(&server_addr_, 0, sizeof(server_addr_));
}

VcsClient::~VcsClient() {
    disconnect();
}

bool VcsClient::connect() {
    if (sock_ >= 0) {
        return true;  // Already connected
    }

    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ < 0) {
        syslog(LOG_ERR, "[VcsClient] Failed to create socket: %s", strerror(errno));
        return false;
    }

    // Set receive timeout
    struct timeval tv;
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Setup server address
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(port_);
    if (inet_pton(AF_INET, host_.c_str(), &server_addr_.sin_addr) <= 0) {
        syslog(LOG_ERR, "[VcsClient] Invalid address: %s", host_.c_str());
        close(sock_);
        sock_ = -1;
        return false;
    }

    syslog(LOG_INFO, "[VcsClient] Connected to %s:%d", host_.c_str(), port_);
    return true;
}

void VcsClient::disconnect() {
    if (sock_ >= 0) {
        close(sock_);
        sock_ = -1;
        syslog(LOG_INFO, "[VcsClient] Disconnected");
    }
}

std::string VcsClient::sendReceive(const std::string& message) {
    if (sock_ < 0) {
        syslog(LOG_ERR, "[VcsClient] Not connected");
        return "";
    }

    // Send
    ssize_t sent = sendto(sock_, message.c_str(), message.length(), 0,
                          (struct sockaddr*)&server_addr_, sizeof(server_addr_));
    if (sent < 0) {
        syslog(LOG_ERR, "[VcsClient] sendto failed: %s", strerror(errno));
        return "";
    }

    syslog(LOG_DEBUG, "[VcsClient] Sent: %s...", message.substr(0, 50).c_str());

    // Receive
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

    ssize_t received = recvfrom(sock_, buffer, sizeof(buffer) - 1, 0,
                                 (struct sockaddr*)&from_addr, &from_len);
    if (received < 0) {
        syslog(LOG_ERR, "[VcsClient] recvfrom failed: %s", strerror(errno));
        return "";
    }

    buffer[received] = '\0';
    std::string response(buffer);

    syslog(LOG_DEBUG, "[VcsClient] Received: %s...", response.substr(0, 50).c_str());

    return response;
}

std::vector<uint8_t> VcsClient::keyExchange(const std::vector<uint8_t>& our_public_key) {
    syslog(LOG_INFO, "[VcsClient] Starting key exchange...");

    std::string message = "KEY_EXCHANGE:" + keystore_crypto::bytesToHex(our_public_key);
    std::string response = sendReceive(message);

    if (response.empty()) {
        syslog(LOG_ERR, "[VcsClient] Key exchange: no response");
        return {};
    }

    if (response.substr(0, 13) != "KEY_EXCHANGE:") {
        syslog(LOG_ERR, "[VcsClient] Key exchange failed: %s", response.c_str());
        return {};
    }

    std::string peer_pubkey_hex = response.substr(13);
    auto peer_pubkey = keystore_crypto::hexToBytes(peer_pubkey_hex);

    if (peer_pubkey.size() != 65) {
        syslog(LOG_ERR, "[VcsClient] Invalid peer public key size: %zu", peer_pubkey.size());
        return {};
    }

    syslog(LOG_INFO, "[VcsClient] Key exchange successful, peer key: %s...",
           peer_pubkey_hex.substr(0, 16).c_str());

    return peer_pubkey;
}

bool VcsClient::sendData(const std::vector<uint8_t>& payload, const std::vector<uint8_t>& mac) {
    std::string message = "DATA:" + keystore_crypto::bytesToHex(payload) +
                          ":" + keystore_crypto::bytesToHex(mac);

    syslog(LOG_INFO, "[VcsClient] Sending data with MAC...");
    std::string response = sendReceive(message);

    if (response == "OK") {
        syslog(LOG_INFO, "[VcsClient] Data sent successfully, MAC verified");
        return true;
    } else if (response == "MAC_ERROR") {
        syslog(LOG_ERR, "[VcsClient] MAC verification failed");
        return false;
    } else {
        syslog(LOG_ERR, "[VcsClient] Unexpected response: %s", response.c_str());
        return false;
    }
}

} // namespace vcs
