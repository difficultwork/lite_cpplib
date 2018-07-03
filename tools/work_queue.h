/**
 * @file    tools\work_queue.h
 * @brief   Encapsulation for work queue
 * @author  Nik Yan
 * @version 1.0     2014-07-02
 */

#ifndef _LITE_WORK_QUEUE_H_
#define _LITE_WORK_QUEUE_H_

#include "event/event.h"
#include "event/mutex_lock.h"
#include "event/thread.h"
#include "byte_stream.h"

namespace lite {

#ifdef OS_WIN

struct Work;

/**
 * @brief   Define work function
 */
typedef void (*work_func_t)(Work* work);

/**
 * @brief   Define data structures for work task
 */
struct Work
{
    void*       user_ptr_;
    void*       user_data_;
    ByteStream  user_buffer_;
    work_func_t work_func_;
    Thread*     thread_;

    Work() 
        : user_ptr_(NULL)
        , user_data_(NULL)
        , work_func_(NULL)
        , thread_(NULL)
    {
    }
    Work(void* user_data) 
        : user_ptr_(NULL)
        , user_data_(user_data)
        , work_func_(NULL)
        , thread_(NULL)
    {
    }
    Work(uint8_t* buffer, uint32_t size) 
        : user_ptr_(NULL)
        , user_data_(NULL)
        , work_func_(NULL)
        , thread_(NULL)
    {
        user_buffer_.Add(buffer,size);
    }

    Work(const Work& src)
    {
        *this = src;
    }

    virtual ~Work()
    {
    }

    Work& operator = (const Work & src)
    {
        user_ptr_      = src.user_ptr_;
        user_data_     = src.user_data_;
        user_buffer_   = src.user_buffer_;
        work_func_     = src.work_func_;
        thread_        = src.thread_;
        return *this;
    }
};

class WorkQueue : public Thread
{
public:

    WorkQueue(const string name="<work_queue>", ILogger* logger=NULL)
        : default_work_func_(NULL)
        , is_working_(false)
        , current_work_(NULL)
    {
    }

    virtual ~WorkQueue()
    {
        Activate(false);
    }

    /**
     * @brief   To determine if pending work exists
     */
    bool Empty()
    {
        MutexLock lock(list_mutex_);
        return work_list_.empty();
    }

    /**
     * @brief  To determine if exist work processing or waiting
     */
    bool Idle()
    {
        MutexLock lock(list_mutex_);
        return work_list_.empty() && !is_working_;
    }

    /**
     * @brief   Add work to work queue
     */
    void QueueWork(Work* work)
    {
        MutexLock lock(list_mutex_);
        work->thread_ = this;
        work_list_.push_back(work);
        list<Work*>::iterator it = work_list.end();
        work_map_.insert(std::make_pair(work, --it));
        queue_event_.Signal();
    }

    /**
     * @brief   Remove work from work queue
     */
    void DequeueWork(Work* work)
    {
        MutexLock lock(list_mutex_);
        map<Work*, list<Work*>::iterator>::iterator it = work_map_.find(work);
        if (it != work_map_.end())
        {
            work_list_.erase(it->second);
            work_map_.erase(it);
        }
    }

    uint32_t PendingCount()
    {
        MutexLock lock(list_mutex_);
        return static_cast<uint32_t>(work_list_.size());
    }

    /**
     * @brief   Delete all unfinished work
     * @param   delete_work_flag    Release work memery
     */
    void Flush(bool delete_work_flag = true)
    {        
        {
            MutexLock lock(list_mutex_);

            if (delete_work_flag)
            {
                for (list<Work*>::iterator it = work_list_.begin(); it != work_list_.end(); ++it)
                {
                    delete (*it);
                }
            }
            work_list_.clear();
            work_map_.clear();
        }
    }

    void SetDefaultWorkFunc(work_func_t work_func)
    {
        default_work_func_ = work_func;
    }

    work_func_t GetDefaultWorkFunc()
    {
        return default_work_func_;
    }

protected:

    uint32_t _Run();

    Mutex                               list_mutex_;
    Event                               queue_event_;
    list<Work*>                         work_list_;
    map<Work*, list<Work*>::iterator>   work_map_;
    work_func_t                         default_work_func_;
    bool                                is_working_;
    Work*                               current_work_;
};

inline
uint32_t WorkQueue::_Run()
{
    while (!_Signalled())
    {
        if (!queue_event_.Wait(200))
        {
            continue;
        }
        queue_event_.Reset();

        while (!_Signalled())
        {
            // Get work from queue
            {
                MutexLock lock(list_mutex_);
                if (work_list_.empty())
                {
                    break;
                }

                is_working_   = true;
                current_work_ = work_list_.front();
                work_list_.pop_front();
                // Remove from work map
                map<Work*, list<Work*>::iterator>::iterator it = work_map_.find(current_work_);
                if (it != work_map_.end())
                {
                    work_map_.erase(it);
                }
            }
            
            // Execute work function
            if (current_work_->work_func_ != NULL)
            {
                current_work_->work_func_(current_work_);
            }
            else if (default_work_func_ != NULL)
            {
                default_work_func_(current_work_);
            }

            {
                MutexLock lock(list_mutex_);
                is_working_   = false;
                current_work_ = NULL;
            }
        }
    }

    return 0;
}

#endif
}

#endif // ifndef _LITE_WORK_QUEUE_H_