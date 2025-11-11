/*
 * ConfigManager.h - 設定管理
 *
 * NVS (Non-Volatile Storage) を使った設定の永続化を管理します。
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Preferences.h>
#include "Types.h"

// グローバル変数の extern 宣言
extern Preferences preferences;
extern WifiConfig config;

// 設定管理関数
void loadConfig();
void saveConfig(const char* ssid, const char* password);
void saveAPPassword(const char* password);

#endif // CONFIG_MANAGER_H
