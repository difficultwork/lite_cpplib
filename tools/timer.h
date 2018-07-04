/**
 * @file    tools\timer.h
 * @brief   Encapsulation timer class
 * @author  Nik Yan
 * @version 1.0     2014-07-02
 *                  Only support windows
 */

#ifndef _LITE_TIMER_H_
#define _LITE_TIMER_H_

#include "base/lite_base.h"
#include "base/exception.h"
#include "event/event.h"
#include "event/mutex.h"
#include "event/mutex_lock.h"
#include "event/thread.h"

#ifdef OS_WIN
#include "mmsystem.h"
#endif

namespace lite {

#ifdef OS_WIN
class Timer
{
public:

    enum TIMERTYPE
    {
        TIMER_Default = 0,
        TIMER_HighResolution,
    };

    /**
     * @brief   Constructor
     * @param   time_span   Time interval of timer(ms)
     * @param   type        Timer type
     */
    Timer(long time_span = 1000, TIMERTYPE type = TIMER_Default)
        : time_span_(time_span)
        , type_(type)
        , is_started_(false)
        , timer_handle_(NULL)
        , timer_id_(0)
    {
        if (time_span == 0)
        {
            time_span_ = 1000;
        }

        not_running_.Reset();
    }

    virtual ~Timer()
    {
        Activate(false);
    }

    /**
     * @brief   Start timer or turn off timer
     * @param   is_open Whether to start the timer
     */
    bool Activate(bool is_open = true);

    /**
     * @brief   Start timer by specified parameter
     */
    bool Activate(uint32_t time_span, TIMERTYPE type);

    /**
     * @brief   This function is run when the timer is activated,
     *          implemented by the derived class(Anti-reentrant processing has been done)
     */
    virtual void OnTimer() = 0;

protected:

    TIMERTYPE   type_;
    long        time_span_;
    Event       not_running_;
    bool        is_started_;

    static Mutex& GetMutex()
    {
        static Mutex* mutex = NULL;
        
        if ( mutex == NULL )
        {
            mutex = new Mutex();
        }
        return *mutex;
    }

private:

    friend class TimerHostThread;

    HANDLE   timer_handle_;
    uint32_t timer_id_;
    
#if defined(_MSC_VER) || defined(__BORLANDC__)
#   pragma comment(lib,"winmm.lib")
#endif

    /*
     * @brief   Timer execution function(low precision)
     */
    static void CALLBACK timerproc_default(
                                           LPVOID pUser,
                                           DWORD dwTimerLowValue,
                                           DWORD dwTimerHighValue)
    {
        Timer* timer = (Timer*)pUser;

        timer->not_running_.Reset();
        timer->OnTimer();
        timer->not_running_.Signal();
    }

    /*
     * @brief   Timer execution function(high precision)
     */
    static void CALLBACK timerproc_highresolution(
                                                  UINT  uID, 
                                                  UINT  uMsg, 
                                                  DWORD dwUser, 
                                                  DWORD dw1, 
                                                  DWORD dw2)
    {
        Timer* timer = (Timer*)dwUser;

        timer->not_running_.Reset();
        timer->OnTimer();
        timer->not_running_.Signal();
    }
};

class TimerHostThread : public Thread
{
    friend class Timer;

public:
    TimerHostThread() : timer_(NULL),timespan_(0),user_data_(NULL),ret_(false)
    {
    }
    virtual ~TimerHostThread()
    {
    }

    bool StartTimer(HANDLE timer, uint32_t timespan, void* user_data)
    {
        MutexLock lock(mt_);

        timer_     = timer;
        timespan_  = timespan;
        user_data_ = user_data;
        ret_       = false;
        event_done_.Reset();
        event_action_.Signal();
        if (Active())
        {
            event_done_.Wait();
        }

        return ret_;
    }

private:    

    uint32_t _Run()
    {
        while (!_Signalled())
        {
            if (event_action_.Wait(0))
            {
                event_action_.Reset();
                if (timer_ != NULL && timespan_ > 0 && user_data_ != NULL)
                {
                    LARGE_INTEGER due_time; // uint is 100 ns
                    due_time.QuadPart = -10*1000;

                    ret_ = SetWaitableTimer(timer_, 
                                            &due_time, 
                                            timespan_, 
                                            (PTIMERAPCROUTINE)Timer::timerproc_default, 
                                            user_data_, 
                                            FALSE) != FALSE;
                }
                else
                {
                    throw invalid_param_exception();
                }
                event_done_.Signal();
            }
            SleepEx(100, TRUE);
        }

        return 0;
    }

    Mutex    mt_;
    Event    event_action_;
    Event    event_done_;
    HANDLE   timer_;
    uint32_t timespan_;
    bool     ret_;
    void*    user_data_;
};

inline
bool Timer::Activate(bool is_open)
{
    if (is_open == is_started_)
    {
        return true;
    }

    static int              default_timer_counter = 0;
    static TimerHostThread* timer_host_thread     = NULL;

    if (is_open)
    {
        not_running_.Signal();

        switch (type_)
        {
        case TIMER_Default:
            timer_handle_ = CreateWaitableTimer(NULL, FALSE, NULL);
            if (timer_handle_ != NULL)
            {
                MutexLock lock(GetMutex());

                if (timer_host_thread == NULL)
                {
                    timer_host_thread = new TimerHostThread();
                    timer_host_thread->Start();
                }
                default_timer_counter++;

                if (!timer_host_thread->StartTimer(timer_handle_ time_span_, this))
                {
                    if (--default_timer_counter == 0)
                    {
                        timer_host_thread->Stop();
                        delete timer_host_thread;
                        timer_host_thread = NULL;
                    }
                    CloseHandle(timer_handle_);
                    timer_handle_ = NULL;
                }
            }
            is_started_ = timer_handle_ != NULL;
            break;
        case TIMER_HighResolution:
            timer_id_ = timeSetEvent(time_span_, 
                                     0, 
                                     (LPTIMECALLBACK)timerproc_highresolution, 
                                     (DWORD_PTR)this, 
                                     TIME_PERIODIC);
            is_started_ = timer_id_ != 0;
            break;
        default:
            return false;
        }

        return is_started_;
    }
    else // is_open == false
    {
        switch (type_)
        {
        case TIMER_Default:
            if (timer_handle_ != NULL)
            {
                CancelWaitableTimer(timer_handle_);
                CloseHandle(timer_handle_);
                timer_handle_ = NULL;

                MutexLock lock(GetMutex());                    
                if (--default_timer_counter == 0)
                {
                    timer_host_thread->Stop();
                    delete timer_host_thread;
                    timer_host_thread = NULL;
                }
            }
            not_running_.Wait();
            break;
        case TIMER_HighResolution:
            if (timer_id_ != 0)
            {
                timeKillEvent(timer_id_);
                timer_id_ = 0;
            }
            not_running_.Wait();
            break;
        default:
            return false;
        }
        is_started_ = false;

        return true;
    }
}

inline
bool Timer::Activate(long time_span, TIMERTYPE type)
{
    if (is_started_)
    {
        return true;
    }

    time_span_ = time_span; 
    type_     = type;

    return Activate(true);
}
#endif
}

using namespace lite;

#endif // ifndef _LITE_TIMER_H_
