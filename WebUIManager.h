/*
 * WebUIManager.h - Web UI 管理
 *
 * HTTP サーバーと Web UI の管理を行います。
 */

#ifndef WEBUI_MANAGER_H
#define WEBUI_MANAGER_H

#include <WebServer.h>

// グローバル変数の extern 宣言
extern WebServer server;

// Web UI セットアップ
void setupWebServer();

// Web UI ハンドラ
void handleRoot();
void handleSave();
void handleSaveAPPassword();

#endif // WEBUI_MANAGER_H
