/**
 * @file    event\thread.h
 * @brief   Encapsulation for thread
 * @author  Nik Yan
 * @version 1.0     2014-06-30
 */

#ifndef _LITE_THREAD_H_
#define _LITE_THREAD_H_

#include "base/lite_base.h"
#include "base/noncopyable.h"
#include "base/exception.h"
#include "event.h"
#include "tools/ilogger.h"

#ifdef OS_WIN
#include <windows.h>
#include <process.h>
#elif defined(OS_LINUX) 
#include <sys/unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#else
#endif

namespace lite {

#define THREAD_LOG_TRACE(fmt,...)    if (logger_) { logger_->Trace(fmt,##__VA_ARGS__); }
#define THREAD_LOG_DEBUG(fmt,...)    if (logger_) { logger_->Debug(fmt,##__VA_ARGS__); }
#define THREAD_LOG_INFO( fmt,...)    if (logger_) { logger_->Info( fmt,##__VA_ARGS__); }
#define THREAD_LOG_WARN( fmt,...)    if (logger_) { logger_->Warn( fmt,##__VA_ARGS__); }
#define THREAD_LOG_ERROR(fmt,...)    if (logger_) { logger_->Error(fmt,##__VA_ARGS__); }
#define THREAD_LOG_FATAL(fmt,...)    if (logger_) { logger_->Fatal(fmt,##__VA_ARGS__); }

class Thread : private NonCopyable
{
public:

    Thread(const string name="<noname>", ILogger* logger=NULL);

    virtual ~Thread();

    /**
     * @brief   Get thread name
     */
    string Name() const
    {
        return name_;
    }

    /**
     * @brief   Set thread name
     */
    void SetName(string name)
    {
        name_ = name;
    }

    /**
     * @brief   Set log recorder
     */
    void SetLogger(ILogger* logger)
    {
        logger_ = logger;
    }

    /**
     * @brief   Start to run
     */
    virtual bool Start();

    /** 
     * @brief   Stop the thread
     * @param   timeout     time to wait stopping
     */
    virtual bool Stop(uint32_t timeout = 0xffffffff);

    /**
     * @brief   Notify thread to stop
     */
    void Signal();

    /**
     * @brief   Check whether thread is alive
     */
    bool Active() const;

    /**
     * @brief   Get thread ID
     */
    uint32_t Id() const
    { 
        return id_; 
    }
    
    /**
     * @brief   Set thread priority
     * @caution Not support linux
     */
	bool SetPriority(int priority)
	{
#ifdef OS_WIN
		return ::SetThreadPriority(thread_handle_, priority);
#else
        return false;
#endif
	}

protected:
    /**
     * @brief   Check thread recved signal or not
     */
    bool _Signalled();

    /**
     * @brief   Sleep for a while
     */
    static void _Sleep(uint32_t milli_seconds)
    {
#ifdef OS_WIN
        Sleep(milli_seconds);
#elif defined(OS_LINUX)
        usleep(milli_seconds * 1000)
#endif
    }

    /**
     * @brief   The thread running function
     *
     *          example：
     *          unsigned long Run()                   <p>
     *          {                                     <p>
     *              .                                 <p>
     *              .                                 <p>
     *              .                                 <p>
     *              while ( Signalled() )             <p>
     *              {                                 <p>
     *                  .                             <p>
     *                  .                             <p>
     *                  .                             <p>
     *              }                                 <p>
     *              .                                 <p>
     *              .                                 <p>
     *              .                                 <p>
     *              return uExitCode;                 <p>
     *          }                                     <p>
     */
    virtual uint32_t _Run() = 0;

    string      name_;
    uint32_t    id_;
    ILogger*    logger_;
    Event       event_;
#ifdef OS_WIN
    HANDLE      thread_handle_;

    /**
     * @brief   Thread Execution function(for windows)
     */
    static uint32_t __stdcall _threadproc(void* obj);

#elif defined(OS_LINUX)
    pthread_t   thread_handle_;

    /**
     * @brief   Thread Execution function(for linux)
     */
    static void* _threadproc(void* obj);
#endif
};

inline
Thread::Thread(const string name, ILogger* logger) 
    : name_(name)
    , id_(0)
    , logger_(logger)
{
#ifdef OS_WIN
    thread_handle_ = NULL;
#elif defined(OS_LINUX)
    thread_handle_ = 0;
#endif
}

inline
Thread::~Thread()
{
#ifdef OS_WIN
    if (thread_handle_ != NULL)
    {
        Stop(500);
    }
#elif defined(OS_LINUX)
    if (thread_handle_ != 0)
    {
        Stop(500);
    }
#endif
}

inline
void Thread::Signal()
{
    event_.Signal();
}

inline
bool Thread::Active() const
{
#ifdef OS_WIN
    if (thread_handle_ == NULL)
    {
        return false;
    }
    return ::WaitForSingleObject(thread_handle_, 0) == WAIT_TIMEOUT;
#elif defined(OS_LINUX)
    if (thread_handle_ == 0)
    {
        return false;
    }
    int rc = pthread_kill(thread_handle_, 0);
    return (rc 1= ESRCH && rc != EINVAL);
#else
    return false
#endif
}

inline
bool Thread::_Signalled()
{
    return event_.Wait(0);
}

#ifdef OS_WIN

inline
bool Thread::Start()
{
    if (thread_handle_ == NULL)
    {
        thread_handle_ = (HANDLE)_beginthreadex(NULL,               // security
                                                20480,              // stack size
                                                _threadproc,        // the thread proc
                                                this,               // current thread object as parameter
                                                CREATE_SUSPENDED,   // flag: don't go.
                                                &id_);              // accepts thread id
        if (thread_handle_ == NULL)
        {
            THREAD_LOG_ERROR("Create thread failure: %s (code=%u)", name_.c_str(), ::GetLastError());
            throw runtime_exception("Create thread failure");
        }
    }
    THREAD_LOG_INFO("Start thread: %s (id=%u)", name_.c_str(), id_);
    event_.Reset();

    return ::ResumeThread(thread_handle_) != 0xffffffff;
}

inline
bool Thread::Stop(uint32_t timeout)
{
    THREAD_LOG_INFO("Stop thread: %s (id=%u)", name_.c_str(), id_);

    DWORD t1 = GetTickCount();
    
    // Resume thread first
    do 
    {
        unsigned long result = ::ResumeThread(thread_handle_);
        
        if( result == (unsigned long)(-1) || result == 0 )
        {
            break;
        }
        if (GetTickCount() - t1 > timeout)
        {
            break;
        }
    }
    while (true);

    bool is_alive = true;

    // Notity thread to stop
    evnet_.Signal();

    while (is_alive && GetTickCount() - t1 < timeout)
    {
        switch(::WaitForSingleObject(thread_handle_, 15))
        {
        case WAIT_TIMEOUT:
            continue;
        case WAIT_OBJECT_0:
            is_alive = false;
            break;
        default:
            // Force kill thread
            ::TerminateThread(thread_handle_, -1);
            is_alive = false;
        }
    }
    
    if (is_alive)
    {
        THREAD_LOG_ERROR("Thread is alive: %s (id=%u), force kill it", name_.c_str(), id_);
        ::TerminateThread(thread_handle_, -1);
    }

    THREAD_LOG_INFO("Thread is stopped: %s (id=%u)", name_.c_str(), id_);
    thread_handle_ = NULL;
    return true;
}

inline
uint32_t __stdcall Thread::_threadproc(void* obj)
{
    return ((Thread*)obj)->_Run();
}

#elif defined(OS_LINUX)

inline
bool Thread::Start()
{
    if (thread_handle_ == 0)
    {
        ret = 0;
        do
        {
            pthread_attr_t thread_attr;
            ret = pthread_attr_init(&thread_attr);
            if (ret != 0)
            {
                break;
            }

            ret = pthread_attr_setstacksize(&thread_attr, 20480);
            if (ret != 0)
            {
                break;
            }

            ret = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
            if (ret != 0)
            {
                break;
            }

            ret = pthread_create(&thread_handle_,                   // the thread id pointer
                                 &thread_attr,                      // the thread attr
                                 _threadproc,                       // the thread proc
                                 this);                             // current thread object as parameter
            (void)pthread_attr_destroy(&thread_attr);
            id_ = static_cast<uint32_t>(thread_handle_);
        }
        while (0);

        if (ret != 0)
        {
            THREAD_LOG_ERROR("Create thread failure: %s (code=%u)", name_.c_str(), ret);
            throw runtime_exception("Create thread failure");
        }
    }
    THREAD_LOG_INFO("Start thread: %s (id=%u)", name_.c_str(), id_);
    event_.Reset();

    return true;
}

inline
bool Thread::Stop(uint32_t timeout)
{
    THREAD_LOG_INFO("Stop thread: %s (id=%u)", name_.c_str(), id_);

    // Notity thread to stop
    evnet_.Signal();

    if (timeout == 0xffffffff)
    {
        void* status;
        (void)pthread_join(thread_handle_, &status);
    }
    else
    {
        struct timespec ts;
        // If get systime failed, ignore timeout
        if (-1 == clock_gettime(CLOCK_REALTIME, &ts))
        {
            void* status;
            (void)pthread_join(thread_handle_, &status);
        }
        else
        {
            ts.tv_sec  += timeout / 1000;
            ts.tv_nsec += (timeout % 1000) * 1000000;
            if (ts.tv_nsec >= 1000000000)
            {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }

            if (0 != pthread_timedjoin_np(thread_handle_, NULL, &ts))
            {
                THREAD_LOG_ERROR("Thread is alive: %s (id=%u), force kill it", name_.c_str(), id_);
                (void)pthread_detach(thread_handle_);
                (void)pthread_cancel(thread_handle_);
            }
        }
    }

    THREAD_LOG_INFO("Thread is stopped: %s (id=%u)", name_.c_str(), id_);
    thread_handle_ = 0;
    return true;
}

inline
void*Thread::_threadproc(void* obj)
{
    ((Thread*)obj)->_Run();
    return obj;
}

#endif

}

using namespace lite;
#endif // ifndef _LITE_THREAD_H_
