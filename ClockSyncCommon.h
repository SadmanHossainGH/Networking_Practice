#include <cstdarg>

#pragma once
void LogCustom(int msgType, const char* text, va_list args);

void NetLog(int msgType, const char* fmt, ...);

void SyncLog(/*int msgType,*/ const char* fmt, ...);