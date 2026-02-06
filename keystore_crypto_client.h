/**
 * KeystoreCrypto Client
 * Phase 3.13: PAL→VCS MAC共通鍵管理
 *
 * KeystoreCrypto サービスと UDS 通信して暗号操作を行う
 */

#ifndef KEYSTORE_CRYPTO_CLIENT_H
#define KEYSTORE_CRYPTO_CLIENT_H

#include <string>
#include <vector>

namespace keystore_crypto {

/**
 * KeystoreCrypto サービスとの通信クラス
 */
class KeystoreCryptoClient {
public:
    KeystoreCryptoClient();
    ~KeystoreCryptoClient();

    /**
     * ECDH鍵ペアを生成
     * @return 公開鍵 (65 bytes uncompressed format)、失敗時は空
     */
    std::vector<uint8_t> generateKey();

    /**
     * 現在の公開鍵を取得
     * @return 公開鍵 (65 bytes)、失敗時は空
     */
    std::vector<uint8_t> getPublicKey();

    /**
     * 相手の公開鍵からECDH共通鍵を導出
     * @param peer_public_key 相手の公開鍵 (65 bytes)
     * @return 成功: true, 失敗: false
     */
    bool deriveKey(const std::vector<uint8_t>& peer_public_key);

    /**
     * MAC (HMAC-SHA256) を計算
     * @param payload ペイロード
     * @return MAC値 (32 bytes)、失敗時は空
     */
    std::vector<uint8_t> computeMac(const std::vector<uint8_t>& payload);

private:
    /**
     * KeystoreCrypto サービスにコマンドを送信し、応答を受信
     * @param request リクエスト文字列
     * @return レスポンス文字列
     */
    std::string sendCommand(const std::string& request);

    // Socket name (abstract namespace)
    static constexpr const char* SOCKET_NAME = "keystore_crypto";
};

// Utility functions
std::string bytesToHex(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> hexToBytes(const std::string& hex);

} // namespace keystore_crypto

#endif // KEYSTORE_CRYPTO_CLIENT_H
