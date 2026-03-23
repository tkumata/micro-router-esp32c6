# Security Memo

優先度が高い順に挙げます。

1. WebUIManager.cpp (line 85)
   Web UI の全エンドポイントに認証がありません。/save、/save_ap_password、/dns-filter-toggle、/upload-blocklist、/download-blocklist がそのまま公開されており、AP に接続できた端末なら誰でも上流 Wi-Fi 情報の変更、AP パスワード変更、DNS 設定改変、ブロックリスト取得ができます。設定系 API に認可がないため、侵入後の被害が大きいです。
2. Config.cpp (line 27), docs/usage.md (line 1)
   初期 AP パスワードが固定値 esp32c6router です。SSID も固定なので、近距離の第三者が初期状態のまま接続しやすく、そのまま 1 の無認証 Web UI を操作できます。初期セットアップ時の実質的な防御が弱いです。
3. WebUIManager.cpp (line 138), WebUIManager.cpp (line 168), WebUIManager.cpp (line 206)
   対応済み。状態変更系 POST に対してサーバー起動時生成の CSRF トークン検証を追加し、あわせて Origin / Referer が AP 自身 (`http://192.168.4.1`) かを確認するようにしたため、外部ページからのクロスサイト送信では設定変更や DNS フィルタ改変が成立しにくくなりました。
5. WebUIManager.cpp (line 223), DNSFilterManager.cpp (line 123)
   可用性面の弱さがあります。ブロックリスト upload にサイズ上限や送信元制限がなく、Flash 容量を圧迫できます。加えて DNS 転送は同期的に最大 2 秒待つため、細かい DNS リクエストを連打されると loop() が詰まり、Web UI やルーティング全体の応答低下を起こしやすいです。遠隔コード実行ではありませんが、DoS には弱い構造です。

## 補足

docs/TASK.md (line 1) にも HTTPS / TLS Support が将来課題として残っており、通信保護が未実装であることと整合しています。ただし、今回の本質は TLS 未対応そのものより、認証なしで設定変更できる点です。

次に進めるなら、まずは Web UI 認証追加、初期パスワードのランダム化、CSRF 対策 の順で対処するのが妥当です。必要ならこの順で修正案まで出します。
