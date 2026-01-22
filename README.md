# PAL (Protocol Adaptation Layer) Daemon

## 概要

外部システムからのUDPマルチキャストパケットを受信し、将来的にData BrokerへgRPC経由で送信するデーモン。

## Phase 1: UDP Multicast Receiver

現在の実装：
- UDPマルチキャスト受信（239.255.0.1:12345）
- SO_REUSEADDR + IP_ADD_MEMBERSHIP使用
- syslog経由でログ出力（POSIX標準、logcatに転送される）
- AOSP固有ライブラリに依存しない

## ビルドからテストまでの完全な手順

### 1. ビルド

```bash
# AOSPプロジェクトルートへ移動
cd ~/work/aosp

# ビルドスクリプトでPALデーモンをビルド
./build.sh pal_daemon

# または、コンテナ内で直接ビルド
podman exec -it aosp-build-env bash -c "
    cd /work/src &&
    source build/envsetup.sh > /dev/null 2>&1 &&
    lunch aosp_car_dev-trunk_staging-eng > /dev/null 2>&1 &&
    m pal_daemon
"
```

**ビルド時間**: 約2-3分（初回は依存関係のビルドで長くなる可能性）

**ビルド成果物の確認**:
```bash
ls -lh src/out/target/product/emulator_car64_x86_64/vendor/bin/pal_daemon
# -rwxr-xr-x 1 dev-user dev-user 51K Jan  2 20:20 pal_daemon

file src/out/target/product/emulator_car64_x86_64/vendor/bin/pal_daemon
# ELF 64-bit LSB pie executable, x86-64, dynamically linked
```

### 2. エミュレータの起動（別ターミナルで実行）

```bash
# エミュレータが起動していない場合
cd ~/work/aosp
./run-emulator.sh

# または
cd src
emulator
```

**エミュレータ起動確認**:
```bash
src/out/host/linux-x86/bin/adb devices
# List of devices attached
# emulator-5554	device
```

### 3. デプロイ

```bash
# rootアクセスを有効化
src/out/host/linux-x86/bin/adb root

# ベンダーパーティションを書き込み可能にマウント
src/out/host/linux-x86/bin/adb remount

# バイナリとinit設定をpush
src/out/host/linux-x86/bin/adb push \
    src/out/target/product/emulator_car64_x86_64/vendor/bin/pal_daemon \
    /vendor/bin/

src/out/host/linux-x86/bin/adb push \
    src/vendor/pal/pal_daemon.rc \
    /vendor/etc/init/

# 実行権限を付与
src/out/host/linux-x86/bin/adb shell chmod 755 /vendor/bin/pal_daemon
```

### 4. 起動

**Phase 1では手動起動**（SELinuxポリシー未設定のため）:
```bash
# バックグラウンドで起動
src/out/host/linux-x86/bin/adb shell /vendor/bin/pal_daemon &

# プロセス確認
src/out/host/linux-x86/bin/adb shell ps -A | grep pal_daemon
# root  2891  599  10808052  3704 __skb_wait_for_more_packets 0 S pal_daemon
```

**起動ログの確認**:
```bash
src/out/host/linux-x86/bin/adb logcat -d | grep pal_daemon | tail -10
```

期待される出力:
```
I pal_daemon: PAL Daemon starting...
I pal_daemon: Version: 1.0.0 (Phase 1 - UDP Receiver)
I pal_daemon: Multicast socket created successfully
I pal_daemon:   Group: 239.255.0.1
I pal_daemon:   Port: 12345
I pal_daemon:   SO_REUSEADDR: enabled
I pal_daemon: Starting receive loop...
```

### 5. 動作確認（テストパケット送信）

#### 方法1: エミュレータ内から送信（推奨）

エミュレータはNATネットワークのため、ホストからのマルチキャストが届きません。
エミュレータ内から送信するのが最も確実です。

```bash
# エミュレータ内でnetcatを使用
src/out/host/linux-x86/bin/adb shell "echo 'Test from emulator' | nc -u 239.255.0.1 12345"
```

**受信ログの確認**:
```bash
src/out/host/linux-x86/bin/adb logcat -d | grep "Received UDP multicast packet" -A5
```

期待される出力:
```
I pal_daemon: ========================================
I pal_daemon: Received UDP multicast packet
I pal_daemon:   From: 10.0.2.16:44440
I pal_daemon:   Size: 19 bytes
I pal_daemon:   Data: Test from emulator
I pal_daemon: ========================================
```

#### 方法2: Pythonスクリプトをエミュレータ内で実行

```bash
# テストスクリプトをエミュレータにpush
src/out/host/linux-x86/bin/adb push \
    test_scripts/send_multicast.py \
    /data/local/tmp/

# エミュレータ内で実行
src/out/host/linux-x86/bin/adb shell \
    python3 /data/local/tmp/send_multicast.py --interval 3
```

#### 方法3: 継続的なログモニタリング

別のターミナルでログを監視:
```bash
# リアルタイムでログを監視
src/out/host/linux-x86/bin/adb logcat | grep pal_daemon

# または、受信パケットのみフィルタ
src/out/host/linux-x86/bin/adb logcat | grep "Received UDP"
```

## トラブルシューティング

### PALデーモンが起動しない

**症状**: `ps -A | grep pal_daemon` で何も表示されない

**原因と対処**:

1. **SELinuxポリシー未設定**（Phase 1の既知の問題）
   ```bash
   # エラーログ確認
   src/out/host/linux-x86/bin/adb logcat -d | grep "pal_daemon.*SELinux"

   # 出力例: cannot setexeccon('u:r:pal_daemon:s0')
   ```

   **対処**: 手動起動を使用
   ```bash
   src/out/host/linux-x86/bin/adb shell /vendor/bin/pal_daemon &
   ```

2. **バイナリが存在しない**
   ```bash
   # ファイル確認
   src/out/host/linux-x86/bin/adb shell ls -l /vendor/bin/pal_daemon
   ```

   **対処**: デプロイ手順を再実行

3. **権限不足**
   ```bash
   # 権限確認
   src/out/host/linux-x86/bin/adb shell ls -l /vendor/bin/pal_daemon
   # -rwxr-xr-x であることを確認
   ```

   **対処**:
   ```bash
   src/out/host/linux-x86/bin/adb shell chmod 755 /vendor/bin/pal_daemon
   ```

### パケットが受信できない

**症状**: ログに "Received UDP multicast packet" が表示されない

**原因と対処**:

1. **ホストからの送信が届かない**（NATネットワークの制約）
   - **対処**: エミュレータ内から送信する（方法1を使用）

2. **マルチキャストグループに参加していない**
   ```bash
   # ソケット状態確認
   src/out/host/linux-x86/bin/adb shell netstat -g
   ```

   **対処**: PALデーモンを再起動

3. **ポート番号が異なる**
   - 送信先: 239.255.0.1:12345 を確認
   - PALデーモンのログで "Port: 12345" を確認

### logcatにログが表示されない

**症状**: syslogが表示されない

**確認**:
```bash
# syslogのログレベルを確認
src/out/host/linux-x86/bin/adb logcat -d | grep -i syslog
```

**対処**: Android 10以降、syslogは自動的にlogdに転送されます。
`pal_daemon`というタグでフィルタリングしてください。

## ログ確認

### リアルタイムログ監視

```bash
# すべてのpal_daemonログ
src/out/host/linux-x86/bin/adb logcat | grep pal_daemon

# 受信パケットのみ
src/out/host/linux-x86/bin/adb logcat | grep "Received UDP"
```

### 過去のログ確認

```bash
# 直近20行
src/out/host/linux-x86/bin/adb logcat -d | grep pal_daemon | tail -20

# 特定のキーワードで検索
src/out/host/linux-x86/bin/adb logcat -d | grep -i "multicast"
```

## 次のステップ（Phase 2以降）

### Phase 2: SELinuxポリシー追加
- [ ] `sepolicy/pal_daemon.te` でタイプ定義
- [ ] `sepolicy/file_contexts` でファイルラベル定義
- [ ] init経由での自動起動

### Phase 3: gRPC統合
- [ ] gRPCクライアントライブラリ追加
- [ ] Data Brokerとの通信実装
- [ ] 受信データのパース処理

## 技術仕様

### 制約
- **POSIX標準のみ**: AOSP固有ライブラリ（liblog等）を使用しない
- **syslog**: ログはsyslogで出力（Androidのlogdに自動転送）
- **ネットワーク権限**: `group inet`、`capabilities NET_RAW`が必要

### マルチキャスト設定
- **グループアドレス**: 239.255.0.1
- **ポート**: 12345
- **SO_REUSEADDR**: 有効（複数プロセスでの受信共有用）
- **IP_ADD_MEMBERSHIP**: マルチキャストグループ参加

### プロセス情報
- **実行ユーザー**: system（Phase 1では手動起動時はroot）
- **グループ**: system, inet
- **ケイパビリティ**: NET_RAW
