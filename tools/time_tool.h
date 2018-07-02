/**
 * @file    tools\time_tool.h
 * @brief   Encapsulation that operations on time
 * @author  Nik Yan
 * @version 1.0     2014-07-01
 */

#ifndef _LITE_TIME_TOOL_H_
#define _LITE_TIME_TOOL_H_

#include "base/lite_base.h"

#ifdef OS_WIN
#include <windows.h>
#elif defined(OS_LINUX)
#include <time.h>
#endif

namespace lite {

struct Time
{
    uint16_t year_;
    uint16_t month_;
    uint16_t day_;
    uint16_t hour_;
    uint16_t minute_;
    uint16_t second_;
    uint16_t milli_second_;

    Time() : year_(0), month_(0), day_(0), hour_(0), minute_(0), second_(0), milli_second_(0)
    {
    }

    virtual ~Time()
    {
    }

    //  String format: yyyy-mm-dd hh:mi:ss
    Time(const char* fmt_time_str)
    {
        Set(fmt_time_str);
    }
    
    //  String format: yyyy-mm-dd hh:mi:ss
    Time(string& fmt_time_str)
    {
        Set(fmt_time_str.c_str());
    }

    Time(Time const& src)
    {
        *this = src;
    }

    Time& operator = (Time const& src)
    {
        this->year_         = src.year_;
        this->month_        = src.month_;
        this->day_          = src.day_;
        this->hour_         = src.hour_;
        this->minute_       = src.minute_;
        this->second_       = src.second_;
        this->milli_second_ = src.milli_second_;

        return *this;
    }

    bool operator < (Time const& right)
    {
        if ( this->year_         != right.year_ )         { return this->year_         < right.year_;         }
        if ( this->month_        != right.month_ )        { return this->month_        < right.month_;        }
        if ( this->day_          != right.day_ )          { return this->day_          < right.day_;          }
        if ( this->hour_         != right.hour_ )         { return this->hour_         < right.hour_;         }
        if ( this->minute_       != right.minute_ )       { return this->minute_       < right.minute_;       }
        if ( this->second_       != right.second_ )       { return this->second_       < right.second_;       }
        if ( this->milli_second_ != right.milli_second_ ) { return this->milli_second_ < right.milli_second_; }
        return false;
    }

    bool operator > ( Time const& right )
    {
        if ( this->year_         != right.year_ )         { return this->year_         > right.year_;         }
        if ( this->month_        != right.month_ )        { return this->month_        > right.month_;        }
        if ( this->day_          != right.day_ )          { return this->day_          > right.day_;          }
        if ( this->hour_         != right.hour_ )         { return this->hour_         > right.hour_;         }
        if ( this->minute_       != right.minute_ )       { return this->minute_       > right.minute_;       }
        if ( this->second_       != right.second_ )       { return this->second_       > right.second_;       }
        if ( this->milli_second_ != right.milli_second_ ) { return this->milli_second_ > right.milli_second_; }
        return false;
    }

    void Set(const char* time_str)
    {
        if (time_str == NULL)
        {
            return;
        }
        else if (strlen(time_str) != 19)
        {
            return;
        }
        char s[20];
        memcpy(s,time_str,20);
        s[19] = 0;
        s[4]  = 0;
        s[7]  = 0;
        s[13] = 0;
        s[16] = 0;

        this->year_         = atoi(s+0);
        this->month_        = atoi(s+5);
        this->day_          = atoi(s+8);
        this->hour_         = atoi(s+11);
        this->minute_       = atoi(s+14);
        this->second_       = atoi(s+17);
        this->milli_second_ = 0;
    }
};

/**
 * @brief   Get current date Time
 */
inline
Time GetCurDataTime()
{
    Time cur_time;

#ifdef OS_WIN
    SYSTEMTIME t;
    GetLocalTime(&t);
    
    cur_time.year_         = t.wYear;
    cur_time.month_        = t.wMonth;
    cur_time.day_          = t.wDay;
    cur_time.hour_         = t.wHour;
    cur_time.minute_       = t.wMinute;
    cur_time.second_       = t.wSecond;
    cur_time.milli_second_ = t.wMilliseconds;
#elif defined(OS_LINUX)
    time_t    p = time(NULL);
    struct tm t = *localtime(&p);
    
    cur_time.year_         = t.tm_year + 1900;
    cur_time.month_        = t.tm_mon + 1;
    cur_time.day_          = t.tm_mday;
    cur_time.uHou          = t.tm_hour;
    cur_time.minute_       = t.tm_min;
    cur_time.second_       = t.tm_sec;
    cur_time.milli_second_ = 0;
#endif

    return cur_time;
}

/**
 * @brief   Get Date Time string(Format: yyyy-mm-dd hh-MM-ss)
 */
inline
string GetDataTimeString1(Time& date_time)
{
    char str[32];
    sprintf(str,
            "%04d-%02d-%02d %02d:%02d:%02d",
            date_time.year_,
            date_time.month_,
            date_time.day_,
            date_time.hour_,
            date_time.minute_,
            date_time.second_);
    return str;
}

/**
 * @brief   Get Date Time string(Format: yyyymmddhhMMss)
 */
inline
string GetDataTimeString2(Time& date_time)
{
    char str[32];
    sprintf(str,
            "%04d%02d%02d%02d%02d%02d",
            date_time.year_,
            date_time.month_,
            date_time.day_,
            date_time.hour_,
            date_time.minute_,
            date_time.second_);
    return str;
}

/**
 * @brief   Get Date Time string(Format: yyyy-mm-dd hh-MM-ss.ms)
 */
inline
string GetDataTimeString3(Time& date_time)
{
    char str[32];
    sprintf(str,
            "%04d-%02d-%02d %02d:%02d:%02d.%03d",
            date_time.year_,
            date_time.month_,
            date_time.day_,
            date_time.hour_,
            date_time.minute_,
            date_time.second_,
            date_time.milli_second_);
    return str;
}

}

using namespace lite;

#endif // ifndef _LITE_TIME_TOOL_H_