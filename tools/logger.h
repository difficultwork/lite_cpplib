/**
 * @file    tools\ilogger.h
 * @brief   Encapsulation log operations
 * @author  Nik Yan
 * @version 1.0     2014-07-01
 */

#ifndef _LITE_LOGGER_H_
#define _LITE_LOGGER_H_

#include "base/lite_base.h"
#include "ilogger.h"
#include "byte_stream.h"
#include "event/mutex.h"
#include "event/mutex_lock.h"
#include "time_tool.h"
#include "thread.h"

namespace lite {
/**
 * @brief   Define the maximum log buffer length(for internal)
 */
#define MAX_LOG_BUFFER_SIZE (4096)

/**
 * @brief   Define the maximum length of supported log information(for caller)
 */
#define MAX_LOG_INFO_SIZE   (MAX_LOG_BUFFER_SIZE-36)

const char* level_name_list[6] = { "Trace", "Debug", "Info", "Warn", "Error", "Fatal" };

class Logger : public ILogger, private Thread
{
public:
    
    /**
     * @brief   Private constructor, not allow the caller to instantiate the class
     */
    Logger() 
        : log_level_(LOGLEVEL_Info)
        , filesize_limit_(10)
        , output_to_file_(false)
        , output_to_screen_(true)
        , asyn_(false)
    {
    }

    virtual ~Logger()
    {
    }

    virtual void SetModule(const string module_name)
    {
        module_name_ = module_name;
    }

    virtual void SetPath(const string path_name)
    {
        path_name_ = path_name;
    }

    virtual void SetLimit(const uint32_t file_size)
    {
        if (file_size > 2048 || file_size == 0 )
        {
            return;
        }
        filesize_limit_ = file_size;
    }

    virtual void SetOutputToFile(const bool out_to_file)
    {
        output_to_file_ = out_to_file;
    }

    virtual void SetOutputToScreen(const bool out_to_screen)
    {
        output_to_screen_ = out_to_screen;
    }

    virtual void SetBackgroundRunning(const bool asyn)
    {
        if (asyn_ == asyn)
        {
            return;
        }
        else if (asyn)
        {
            Stop();
        }
        else
        {
            Start();
        }
        asyn_ = asyn;
    }

    virtual void SetLogLevel(const LOGLEVEL log_level)
    {
        log_level_ = log_level;
    }

    virtual void SetLogInfo(LOGID id, LOGLEVEL level, bool has_param, char const* log_text)
    {
        log_map_[id] = LogInfo(id, level, has_param, log_text);
    }

    virtual void Trace(const string& text)
    {
        _Write(LOGLEVEL_Trace, text);
    }

    virtual void Trace(const char* fmt_text, ...)
    {
        va_list va;

        va_start(va, fmt_text);
        _Write(LOGLEVEL_Trace, fmt_text, va);
        va_end(va);
    }

    virtual void Debug(const string& text)
    {
        _Write(LOGLEVEL_Debug, text);
    }

    virtual void Debug(const char* fmt_text, ...)
    {
        va_list va;

        va_start(va, fmt_text);
        _Write(LOGLEVEL_Debug, fmt_text, va);
        va_end(va);
    }

    virtual void Info(const string& text)
    {
        _Write(LOGLEVEL_Info, text);
    }

    virtual void Info(const char* fmt_text, ...)
    {
        va_list va;

        va_start(va, fmt_text);
        _Write(LOGLEVEL_Info, fmt_text, va);
        va_end(va);
    }

    virtual void Warn(const string& text)
    {
        _Write(LOGLEVEL_Warn, text);
    }

    virtual void Warn(const char* fmt_text, ...)
    {
        va_list va;

        va_start(va, fmt_text);
        _Write(LOGLEVEL_Warn, fmt_text, va);
        va_end(va);
    }

    virtual void Error(const string& text)
    {
        _Write(LOGLEVEL_Error, text);
    }

    virtual void Error(const char* fmt_text, ...)
    {
        va_list va;

        va_start(va, fmt_text);
        _Write(LOGLEVEL_Error, fmt_text, va);
        va_end(va);
    }

    virtual void Fatal(const string& text)
    {
        _Write(LOGLEVEL_Fatal, text);
    }

    virtual void Fatal(const char* fmt_text, ...)
    {
        va_list va;

        va_start(va, fmt_text);
        _Write(LOGLEVEL_Fatal, fmt_text, va);
        va_end(va);
    }

    /**
     * @brief   Output a stream of bytes to the debug level log(hexadecimal)
     * @param   buf             Bytestream
     * @param   size            Size of output bytestream(not more than MAX_LOG_INFO_SIZE/3.1)
     * @param   bytes_per_line  Output bytes per line
     * @param   space_gap       Whether bytes are separated by spaces
     */
    void DebugHexString(const char* buf, uint32_t size, uint32_t bytes_per_line = 16, bool space_gap = true);

    /**
     * @brief   Wait for background log to complete output
     */
    void Flush()
    {
        while (!_Signalled())
        {
            {
                MutexLock lock(mutex_log_text_);
                if ( log_input_->size() == 0 )
                {
                    break;
                }
            }

            _Sleep(100);
        }
    }

private:

    /**
     * @brief   Data structure for internal preservation of log-related information
     */
    struct  LogInfo
    {
        LOGID       log_id_;        ///< Log ID
        LOGLEVEL    log_level_;     ///< Log level
        bool        has_param_;     ///< Whether needs parameters
        const char* log_text_;      ///< Log text

        LogInfo()
            : log_id_(0)
            , log_level_(LOGLEVEL_Info)
            , has_param_(false)
            , log_text_("")
        {
        }

        LogInfo(LOGID id, LOGLEVEL level, bool has_param, char const* log_text)
            : log_id_(id), log_level_(level), has_param_(has_param), log_text_(log_text)
        {
        }
    };

    LOGLEVEL                log_level_;
    string                  module_name_;
    string                  path_name_;
    string                  log_filename_;
    uint32_t                filesize_limit_;
    bool                    output_to_file_;
    bool                    output_to_screen_;
    bool                    asyn_;
    map<LOGID,LogInfo>      log_map_;
    list<string>            log_text_lista_;
    list<string>            log_text_listb_;
    list<string>*           log_input_;
    list<string>*           log_output_;
    Mutex                   mutex_file_;
    Mutex                   mutex_log_text_;

    /**
     * @brief   Construct a new log file name
     */
    string _NewLogFileName()
    {
        string file_name;
        string cur_datetime = GetDataTimeString2(GetCurDataTime());

        if (path_name_ == "")
        {
            file_name = string("log\\") + module_name_ + cur_datetime + ".log";
        }
        else
        {
            file_name = path_name_ + "\\" + module_name_ + cur_datetime+".log";
        }

        return file_name;
    }

    /**
     * @brief   Output log
     */
    void _Write(const char* text);

    /**
     * @brief   Variable parameter output log
     */
    void _Write(LOGLEVEL level, const char* text, va_list va);

    /**
     * @brief   Output log(support very long text)
     */
    void _Write(LOGLEVEL level, const string& text);

    /**
     * @brief   Log service background thread function
     */
    uint32_t _Run();
};// end Logger

inline
void DebugHexString(const char* buf, uint32_t size, uint32_t bytes_per_line, bool space_gap)
{
    if (LOGLEVEL_Debug < log_level_)
    {
        return;
    }

    if (buf == NULL || size ==0)
    {
        return;
    }

    if ((size / 2) > MAX_LOG_INFO_SIZE)
    {
        return;
    }

    uint32_t line_cnt = (size + bytes_per_line - 1) / bytes_per_line;
    uint32_t str_len  = line_cnt * (bytes_per_line * (space_gap ? 3 : 2) + 1);
    if (str_len > MAX_LOG_INFO_SIZE || str_len == 0)
    {
        return;
    }

    char* dump = new char[str_len];

    uint32_t p = 0;
    for (uint32_t i = 0; i < size; i++)
    {
        if (i > 0 && i % bytes_per_line == 0)
        {
            dump[p++] = '\n';
        }
        char c1 = (buf[i] & 0xF0) >> 4;
        char c2 = (buf[i] & 0x0F);
        dump[p++] = c1 < 10 ? ('0' + c1) : ('A' + c1 - 10);
        dump[p++] = c2 < 10 ? ('0' + c2) : ('A' + c2 - 10);
        if (space_gap)
        {
            dump[p++] = ' ';
        }
    }
    dump[p] = '\0';

    _Write(LOGLEVEL_Debug, dump, NULL);
}

inline
void Logger::_Write(const char* text)
{
    char* log_text = const_cast<char*>(text);

    if (output_to_screen_)
    {
        char* newline = NULL;
        while (NULL != (newline = (char*)strstr(text, "\n")))
        {
            *newline = 0;
            printf("%s\n", text);
            *newline = '\n';
            text     = newline + 1;
            if (text[0] != 0)
            {
                printf("                                  ");
            }
        }
        if (text[0] != 0)
        {
            printf("%s\n", text);
        }
    }

    if (output_to_file_)
    {
        if (log_filename_ == "")
        {
            log_filename_ = _NewLogFileName();
        }

        FILE* fp = fopen(log_filename_.c_str(), "at");
        if (fp != NULL)
        {
            fseek(fp, 0 , 2);
            if (ftell(fp) / (1024 * 1024) >= (long)filesize_limit_)
            {
                log_filename_ = _NewLogFileName();
                fclose(fp);
                fp = fopen(log_filename_.c_str(), "at");
            }
        }

        if (fp != NULL)
        {
            char* newline = NULL;
            while ((newline = (char*)strstr(log_text, "\n")) != NULL)
            {
                *newline = 0;
                fprintf(fp, "%s\n", log_text);
                *newline = '\n';
                log_text = newline + 1;
                if (log_text[0] != 0)
                {
                    fprintf(fp, "                                  ");
                }
            }
            if (text[0] != 0)
            {
                fprintf(fp, "%s\n", log_text);
            }
        }
    }
}

inline
void Logger::_Write(LOGLEVEL level, const char* fmt_text, va_list va)
{
    if (level < log_level_)
    {
        return;
    }

    ByteStream bs_text(MAX_LOG_BUFFER_SIZE);
    char*      log_text = (char*)bs_text.GetBuffer();

    if (va != NULL)
    {
        ByteStream bs_tmp(MAX_LOG_BUFFER_SIZE * 10);
        char*      log_tmp = (char*)bs_tmp.GetBuffer();

        if (-1 == vsprintf(log_tmp, fmt_text, va))
        {
            return;
        }

        if (strlen(log_tmp) > MAX_LOG_INFO_SIZE)
        {
            return;
        }
        sprintf(log_text, "[%s] [%s] %s", GetDataTimeString3(GetCurDataTime()).c_str(), level_name_list[level], log_tmp);
    }
    else
    {
        sprintf(log_text, "[%s] [%s] %s", GetDataTimeString3(GetCurDataTime()).c_str(), level_name_list[level], fmt_text);
    }

    if (asyn_)
    {
        MutexLock lock(mutex_log_text_);
        log_input_->push_back(log_text);
    }
    else
    {
        MutexLock lock(mutex_log_text_);
        _Write(log_text);
    }
}


inline
void Logger::_Write(LOGLEVEL level, const string& text)
{
    _Write(level, text.c_str(), NULL);
}

inline
uint32_t Logger::_Run()
{
    while (!_Signalled())
    {    
        _Sleep(100);

        {
            MutexLock lock(mutex_log_text_);

            // 读取日志信息
            if ( log_input_->size() == 0 )
            {
                continue;
            }

            list<string>* tmp_logs = log_output_;
            log_output_ = log_input_;
            log_input_  = tmp_logs;
        }
        {
            for (list<string>::const_iterator it = log_output_->begin(); it != log_output_->end(); it++)
            {
                _Write( (*it).c_str() );
            }
        }
    }

    return 0;
}

}

using namespace lite;

#endif // ifndef _LITE_LOGGER_H_