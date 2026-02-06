/**
 * PAL-VCS MAC Test
 * Phase 3.13: PAL→VCS MAC共通鍵管理
 *
 * KeystoreCrypto サービスと VCS モック間の E2E テスト
 *
 * Usage:
 *   pal_vcs_mac_test <vcs_host> <vcs_port>
 *
 * Example:
 *   pal_vcs_mac_test 192.168.1.100 5000
 */

#include "keystore_crypto_client.h"
#include "vcs_client.h"

#include <syslog.h>
#include <cstdlib>
#include <cstring>
#include <iostream>

void printUsage(const char* program) {
    std::cerr << "Usage: " << program << " <vcs_host> <vcs_port>" << std::endl;
    std::cerr << "Example: " << program << " 192.168.1.100 5000" << std::endl;
}

int main(int argc, char* argv[]) {
    // Parse arguments
    if (argc != 3) {
        printUsage(argv[0]);
        return 1;
    }

    const char* vcs_host = argv[1];
    int vcs_port = std::atoi(argv[2]);

    if (vcs_port <= 0 || vcs_port > 65535) {
        std::cerr << "Invalid port: " << argv[2] << std::endl;
        return 1;
    }

    // Initialize syslog
    openlog("pal_vcs_mac_test", LOG_PID | LOG_CONS | LOG_PERROR, LOG_USER);

    syslog(LOG_INFO, "========================================");
    syslog(LOG_INFO, "PAL-VCS MAC Test");
    syslog(LOG_INFO, "Phase 3.13: PAL→VCS MAC共通鍵管理");
    syslog(LOG_INFO, "VCS: %s:%d", vcs_host, vcs_port);
    syslog(LOG_INFO, "========================================");

    // Step 1: Initialize KeystoreCrypto client
    syslog(LOG_INFO, "");
    syslog(LOG_INFO, "[Step 1] Initialize KeystoreCrypto client");
    keystore_crypto::KeystoreCryptoClient crypto;

    // Step 2: Generate ECDH key pair
    syslog(LOG_INFO, "");
    syslog(LOG_INFO, "[Step 2] Generate ECDH key pair");
    auto our_public_key = crypto.generateKey();
    if (our_public_key.empty()) {
        syslog(LOG_ERR, "FAILED: Could not generate key pair");
        syslog(LOG_ERR, "Make sure KeystoreCrypto service is running");
        closelog();
        return 1;
    }
    syslog(LOG_INFO, "OK: Public key generated (%zu bytes)", our_public_key.size());

    // Step 3: Connect to VCS
    syslog(LOG_INFO, "");
    syslog(LOG_INFO, "[Step 3] Connect to VCS");
    vcs::VcsClient vcs(vcs_host, vcs_port);
    if (!vcs.connect()) {
        syslog(LOG_ERR, "FAILED: Could not connect to VCS");
        closelog();
        return 1;
    }
    syslog(LOG_INFO, "OK: Connected to VCS");

    // Step 4: Key exchange
    syslog(LOG_INFO, "");
    syslog(LOG_INFO, "[Step 4] Key exchange with VCS");
    auto vcs_public_key = vcs.keyExchange(our_public_key);
    if (vcs_public_key.empty()) {
        syslog(LOG_ERR, "FAILED: Key exchange failed");
        closelog();
        return 1;
    }
    syslog(LOG_INFO, "OK: Received VCS public key (%zu bytes)", vcs_public_key.size());

    // Step 5: Derive shared key
    syslog(LOG_INFO, "");
    syslog(LOG_INFO, "[Step 5] Derive shared key (ECDH + HKDF)");
    if (!crypto.deriveKey(vcs_public_key)) {
        syslog(LOG_ERR, "FAILED: Could not derive shared key");
        closelog();
        return 1;
    }
    syslog(LOG_INFO, "OK: Shared key derived");

    // Step 6: Send test data with MAC
    syslog(LOG_INFO, "");
    syslog(LOG_INFO, "[Step 6] Send data with MAC");

    const char* test_message = "HELLO_FROM_PAL";
    std::vector<uint8_t> payload(test_message, test_message + strlen(test_message));

    auto mac = crypto.computeMac(payload);
    if (mac.empty()) {
        syslog(LOG_ERR, "FAILED: Could not compute MAC");
        closelog();
        return 1;
    }
    syslog(LOG_INFO, "MAC computed (%zu bytes)", mac.size());

    if (!vcs.sendData(payload, mac)) {
        syslog(LOG_ERR, "FAILED: VCS rejected the data");
        closelog();
        return 1;
    }
    syslog(LOG_INFO, "OK: Data accepted by VCS");

    // Step 7: Test with tampered data (should fail)
    syslog(LOG_INFO, "");
    syslog(LOG_INFO, "[Step 7] Test tampered data (expect failure)");

    const char* tampered_message = "TAMPERED_DATA";
    std::vector<uint8_t> tampered_payload(tampered_message, tampered_message + strlen(tampered_message));

    // Use the same MAC (wrong for this payload)
    if (vcs.sendData(tampered_payload, mac)) {
        syslog(LOG_ERR, "FAILED: VCS accepted tampered data (security issue!)");
        closelog();
        return 1;
    }
    syslog(LOG_INFO, "OK: VCS correctly rejected tampered data");

    // Summary
    syslog(LOG_INFO, "");
    syslog(LOG_INFO, "========================================");
    syslog(LOG_INFO, "All tests PASSED!");
    syslog(LOG_INFO, "========================================");

    closelog();
    return 0;
}
