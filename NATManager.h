/*
 * NATManager.h - NAT/NAPT 機能管理
 *
 * ESP-IDF の esp_netif API を使った NAT 機能の管理を行います。
 */

#ifndef NAT_MANAGER_H
#define NAT_MANAGER_H

// NAT 状態管理変数の extern 宣言
extern bool natEnabled;
extern bool needEnableNAT;

// NAT 機能
void enableNAT();
void processNATEnableRequest();

#endif // NAT_MANAGER_H
