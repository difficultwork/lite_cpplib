/**
 * @file    tools\ilogger.h
 * @brief   Declare log operations interface
 * @author  Nik Yan
 * @version 1.0     2014-07-01
 */

#ifndef _LITE_ILOGGER_H_
#define _LITE_ILOGGER_H_

#include "base/lite_base.h"

namespace lite {
/**
 * @brief   Define log level
 */
enum LOGLEVEL
{
    LOGLEVEL_Trace,
    LOGLEVEL_Debug,
    LOGLEVEL_Info,
    LOGLEVEL_Warn,
    LOGLEVEL_Error,
    LOGLEVEL_Fatal
};

/**
 * @brief   Log ID
 */
typedef uint32_t LOGID;

class ILogger
{
public:

    /**
     * @brief   Set module name
     */
    virtual void SetModule(const string module_name) = 0;

    /**
     * @brief   Set log file output path(default is current directory)
     */
    virtual void SetPath(const string path_name) = 0;

    /**
     * @brief   Set log file size limit(default is 10MB)
     * @param   file_size   Limit size (in MB)
     */
    virtual void SetLimit(const uint32_t file_size) = 0;

    /**
     * @brief   Set whether the log is output to file(default is false)
     */
    virtual void SetOutputToFile(const bool out_to_file) = 0;

    /**
     * @brief   Set whether the log is output to screen(default is false)
     */
    virtual void SetOutputToScreen(const bool out_to_screen) = 0;

    /**
     * @brief   Set whether logging is asynchronous(default is synchronous)
     */
    virtual void SetBackgroundRunning(const bool asyn) = 0;

    /**
     * @brief   Modify log level(default is Info)
     */
    virtual void SetLogLevel(const LOGLEVEL log_level) = 0;

    /**
     * @brief   Set formatted log string(必须调用者将格式化日志字符串设置到日志系统中)
     * @param   id          Log ID
     * @param   level       Log level
     * @param   has_param   Whether the log text contains parameters
     * @param   log_text    Log text (format string, format indicates "{format indicates character}",
     *                      such as: s= string, d= integer, f= float number)
     * @caution The caller must set the formatted log string to the log system
     */
    virtual void SetLogInfo(LOGID id, LOGLEVEL level, bool has_param, char const* log_text) = 0;

   
    /**
     * @brief   Write Trace-level logs(support very long string)
     */
    virtual void Trace(const string& text) = 0;
    
    /**
     * @brief   Write Trace-level logs(string length less than MAX_LOG_INFO_SIZE)
     * @param   fmt_text    formatted string
     * @param   ...         param list
     */
    virtual void Trace(const char* fmt_text, ...) = 0;
    
    /**
     * @brief   Write Debug-level logs(support very long string)
     */
    virtual void Debug(const string& text) = 0;
    
    /**
     * @brief   Write Debug-level logs(string length less than MAX_LOG_INFO_SIZE)
     * @param   fmt_text    formatted string
     * @param   ...         param list
     */
    virtual void Debug(const char* fmt_text, ...) = 0;
    
    /**
     * @brief   Write Info-level logs(support very long string)
     */
    virtual void Info(const string& text) = 0;
    
    /**
     * @brief   Write Info-level logs(string length less than MAX_LOG_INFO_SIZE)
     * @param   fmt_text    formatted string
     * @param   ...         param list
     */
    virtual void Info(const char* fmt_text, ...) = 0;
    
    /**
     * @brief   Write Warn-level logs(support very long string)
     */
    virtual void Warn(const string& text) = 0;
    
    /**
     * @brief   Write Warn-level logs(string length less than MAX_LOG_INFO_SIZE)
     * @param   fmt_text    formatted string
     * @param   ...         param list
     */
    virtual void Warn(const char* fmt_text, ...) = 0;
    
    /**
     * @brief   Write Error-level logs(support very long string)
     */
    virtual void Error(const string& text) = 0;
    
    /**
     * @brief   Write Error-level logs(string length less than MAX_LOG_INFO_SIZE)
     * @param   fmt_text    formatted string
     * @param   ...         param list
     */
    virtual void Error(const char* fmt_text, ...) = 0;
    
    /**
     * @brief   Write Fatal-level logs(support very long string)
     */
    virtual void Fatal(const string& text) = 0;
    
    /**
     * @brief   Write Fatal-level logs(string length less than MAX_LOG_INFO_SIZE)
     * @param   fmt_text    formatted string
     * @param   ...         param list
     */
    virtual void Fatal(const char* fmt_text, ...) = 0;
};// end ILogger

}

using namespace lite;

#endif // ifndef _LITE_ILOGGER_H_