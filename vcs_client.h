/**
 * VCS Client
 * Phase 3.13: PAL→VCS MAC共通鍵管理
 *
 * VCS との UDP 通信を行う
 */

#ifndef VCS_CLIENT_H
#define VCS_CLIENT_H

#include <string>
#include <vector>
#include <netinet/in.h>

namespace vcs {

/**
 * VCS との UDP 通信クラス
 */
class VcsClient {
public:
    VcsClient(const std::string& host, int port);
    ~VcsClient();

    /**
     * VCSに接続（ソケット作成）
     * @return 成功: true
     */
    bool connect();

    /**
     * 切断
     */
    void disconnect();

    /**
     * 鍵交換を実行
     * @param our_public_key 自分の公開鍵 (65 bytes)
     * @return 相手の公開鍵 (65 bytes)、失敗時は空
     */
    std::vector<uint8_t> keyExchange(const std::vector<uint8_t>& our_public_key);

    /**
     * データを送信（MAC付き）
     * @param payload ペイロード
     * @param mac MAC値 (32 bytes)
     * @return 成功: true (VCSからOK応答)
     */
    bool sendData(const std::vector<uint8_t>& payload, const std::vector<uint8_t>& mac);

private:
    /**
     * メッセージを送信し、応答を受信
     */
    std::string sendReceive(const std::string& message);

    std::string host_;
    int port_;
    int sock_;
    struct sockaddr_in server_addr_;

    static constexpr int BUFFER_SIZE = 4096;
    static constexpr int TIMEOUT_SEC = 5;
};

} // namespace vcs

#endif // VCS_CLIENT_H
