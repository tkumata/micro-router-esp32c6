/*
 * WebUIRenderer.h - Web UI 描画補助
 *
 * HTML テンプレートの組み立てと、共通ページ生成を担当します。
 */

#ifndef WEB_UI_RENDERER_H
#define WEB_UI_RENDERER_H

#include <Arduino.h>
#include "DNSFilterManager.h"

String buildSimpleMessagePage(const String& title, const String& message, const String& backPath = "/");
String buildRestartNoticePage(const String& title, const String& message);
String buildUploadSuccessPage();
String renderRootPage();
String renderDNSFilterPage(const DNSStats& stats, int blocklistCount, bool enabled);

#endif // WEB_UI_RENDERER_H
