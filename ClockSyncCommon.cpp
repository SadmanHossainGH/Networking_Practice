#include "ClockSyncCommon.h"
#include "raylib.h"
#include <ctime>
#include <cstdio>
#include <cstdarg>

class FileLogger
{
public:
	FileLogger(const char* logfile)
	{
		fp = fopen(logfile, "w");
	}

	~FileLogger()
	{
		fclose(fp);
	}

	void LogFormatted(int msgType, const char* text, va_list args)
	{
		char timeStr[64] = { 0 };
		time_t now = time(NULL);
		struct tm* tm_info = localtime(&now);

		strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);
		fprintf(fp, "[%s]", timeStr);

		switch (msgType)
		{
		case LOG_INFO: fprintf(fp, "[INFO] : "); break;
		case LOG_ERROR: fprintf(fp, "[ERROR] : "); break;
		case LOG_WARNING: fprintf(fp, "[WARNING] : "); break;
		case LOG_DEBUG: fprintf(fp, "[DEBUG] : "); break;
		default: break;
		}

		vfprintf(fp, text, args);
		fprintf(fp, "\n");

		fflush(fp);
	}

	void SimpleLog(const char* text, va_list args)
	{
		char timeStr[64] = { 0 };
		time_t now = time(NULL);
		struct tm* tm_info = localtime(&now);

		vfprintf(fp, text, args);
		fprintf(fp, "\n");

		fflush(fp);
	}
private:
	FILE* fp{ nullptr };
};

static FileLogger s_LoggerInstance("logfile.txt");
static FileLogger s_SyncLogInstance("synclog.txt");

void LogCustom(int msgType, const char* text, va_list args)
{
	s_LoggerInstance.LogFormatted(msgType, text, args);
}

void NetLog(int msgType, const char* fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	s_SyncLogInstance.SimpleLog(fmt, args);

	va_end(args);
}

void SyncLog(/*int msgType,*/ const char* fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	s_SyncLogInstance.SimpleLog(fmt, args);

	va_end(args);
}