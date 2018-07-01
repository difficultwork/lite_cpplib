/**
 * @file    event\mutex.h
 * @brief   Encapsulation for mutually exclusive object
 * @author  Nik Yan
 * @caution Not support multi-processes
 * @version 1.0     2014-06-30
 */

#ifndef _LITE_MUTEX_H_
#define _LITE_MUTEX_H_

#include "base/lite_base.h"
#include "base/noncopyable.h"

#ifdef OS_WIN
#include <windows.h>
#elif defined(OS_LINUX)
#include <pthread.h>
#endif

namespace lite {

class Mutex : private NonCopyable
{
public:

    /**
     * @brief   Constructor
     * @param   szName  互斥对象名称
     */
    Mutex(const string name="");

    virtual ~Mutex();

    /**
     * @brief   Mutex begins locking protection segment
     */
    void Lock();

    /**
     * @brief   Mutually exclusive objects unlock the protection segment
     */
    void Unlock();

private:

    string              name_;
#ifdef OS_WIN
    CRITICAL_SECTION    critical_section_;
#elif defined(OS_LINUX)
    pthread_mutex_t     pthread_mutex_;
#endif 

};

#ifdef OS_WIN

inline
Mutex::Mutex(const string name) : name_(name)
{
    ::InitializeCriticalSection(&critical_section_);
}

inline
Mutex::~Mutex()
{
    ::DeleteCriticalSection(&critical_section_);
}

inline
void Mutex::Lock()
{
    ::EnterCriticalSection(&critical_section_);
}

inline
void Mutex::Unlock()
{
    ::LeaveCriticalSection(&critical_section_);
}

#else defined(OS_LINUX)

inline
Mutex::Mutex(const string name) : name_(name)
{
    pthread_mutexattr_t attr;
    (void)pthread_mutexattr_init(&attr);
    (void)pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    (void)pthread_mutex_init(&pthread_mutex_, &attr);
    (void)pthread_mutexattr_destroy(&attr);
}

inline
Mutex::~Mutex()
{
    (void)pthread_mutex_destroy(&pthread_mutex_);
}

inline
void Mutex::Lock()
{
    pthread_mutex_lock(&pthread_mutex_);
}

inline
void Unlock()
{
    pthread_mutex_unlock(&pthread_mutex_);
}

#endif

}

using namespace lite;

#endif // ifndef _LITE_MUTEX_H_