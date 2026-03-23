# Web UI 認証方式変更 仕様

## 追加エンドポイント

- `GET /login`
  - 初期 AP パスワード時は変更専用画面を返す。
  - それ以外はログイン画面を返す。
- `POST /login`
  - `csrf_token` と `ap_password` を受け取る。
  - 成功時はセッション cookie を発行してリダイレクトする。
- `POST /logout`
  - `csrf_token` を検証後、セッションを破棄する。

## 既存エンドポイントの挙動変更

- `GET /`
  - 初期 AP パスワード時は変更専用画面を返す。
  - それ以外はセッション必須。
- `POST /save`
  - セッション必須。
- `POST /save_ap_password`
  - 初期 AP パスワード時は未ログインでも実行可。
  - それ以外はセッション必須。
  - 成功時はセッション破棄後に再起動する。
- `GET /dns-filter`
  - セッション必須。
- `POST /dns-filter-toggle`
  - セッション必須。
- `POST /upload-blocklist`
  - セッション必須。
- `GET /download-blocklist`
  - セッション必須。

## セキュリティ要件

- 状態変更系 POST はすべて CSRF トークン検証を行う。
- `Origin` または `Referer` が AP 自身でない場合は拒否する。
- セッション cookie 不一致、未送信、期限切れは未認証として扱う。
