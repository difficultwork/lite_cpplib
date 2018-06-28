/**
 * @file    event\event.h
 * @brief   Encapsulation for event
 * @author  Nik Yan
 * @version 1.0     2014-06-29
 */

#ifndef _LITE_EVENT_H_
#define _LITE_EVENT_H_

#include "base/lite_base.h"
#include "base/noncopyable.h"

#ifdef OS_WIN
#include <windows.h>

#elif defined(OS_LINUX)
#include <pthread.h>
#include <time.h>

#endif

namespace lite {

class Event : private NonCopyable
{
public:
    Event();

    virtual ~Event();

    void Reset(void);

    void Signal(void);

    bool Wait(uint32_t timeout = 0xffffffff);

private:
#ifdef OS_WIN
    HANDLE          event_handle_;

#elif defined(OS_LINUX)
    pthread_mutex_t mutex_handle_;
    pthread_coud_t  cond_;
    volatile bool   event_state_;

#else
#endif
};

#ifdef OS_WIN

inline
Event::Event()
{
    event_handle_ = ::CreateEvent(NULL, true, false, NULL);
    if (event_handle_ == NULL)
    {
        throw event_handle_;
    }
}

inline
Event::~Event()
{
    if (event_handle_ != NULL)
    {
        ::CloseHandle(event_handle_);
    }
}

inline
void Event::Reset()
{
    if (event_handle_ != NULL)
    {
        ::ResetEvent(event_handle_);
    }
}

inline
void Event::Signal()
{
    if (event_handle_ != NULL)
    {
        ::SetEvent(event_handle_);
    }
}

inline
bool Event::Wait(uint32_t timeout)
{
    if (event_handle_ == NULL)
    {
        return false;
    }
    return ::WaitForSingleObject(event_handle_, timeout) == WAIT_OBJECT_0;
}

#elif defined(OS_LINUX)

inline
Event::Event() : event_state_(false)
{
    (void)pthread_mutex_init(&mutex_handle_, NULL);
    (void)pthread_cond_init(&cond_, NULL);
}

inline
Event::~Event()
{
    (void)pthread_cond_destroy(&cond_);
    (void)pthread_mutex_destroy(&mutex_handle_);
}

inline
void Event::Reset()
{
    if (pthread_mutex_lock(&mutex_handle_) != 0)
    {
        throw -1;
    }
    event_state_ = false;
    (void)pthread_mutex_unlock(&mutex_handle_);
}

inline
void Event::Signal()
{
    if (pthread_mutex_lock(&mutex_handle_) != 0)
    {
        throw -1;
    }
    event_state_ = true;
    (void)pthread_cond_broadcast(&cond_);
    (void)pthread_mutex_unlock(&mutex_handle_);
}

inline
bool Event::Wait(uint32_t timeout)
{
    if (timeout == 0)
    {
        if (pthread_mutex_trylock(&mutex_handle_) != 0)
        {
            return false;
        }
    }
    else if (pthread_mutex_lock(&mutex_handle_) != 0)
    {
        return false;
    }

    if (timeout == 0xffffffff)
    {
        while (!event_state_)
        {
            if (pthread_cond_wait(&cond_, &mutex_handle_) != 0)
            {
                (void)pthread_mutex_unlock(&mutex_handle_);
                return false;
            }
        }
    }
    else
    {
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
        {
            return false;
        }
        ts.tv_sec  += timeout / 1000;
        ts.tv_nsec += (timeout%1000) * 1000000;
        if (ts.tv_nsec >= 1000000000)
        {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }

        while (event_state_)
        {
            if (pthread_cond_timewait(&cond_, &mutex_handle_, &ts) != 0)
            {
                (void)pthread_mutex_unlock(&mutex_handle_);
                return false;
            }
        }
    }

    (void)pthread_mutex_unlock(&mutex_handle_);
    return true;
}

#else
#endif

}

using namespace lite;

#endif // ifndef _LITE_EVENT_H_