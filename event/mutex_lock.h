/**
 * @file    event\mutex_lock.h
 * @brief   Encapsulation for mutually exclusive object
 *          To lock and unlock through construction and destructor.
 * @author  Nik Yan
 * @version 1.0     2014-06-30
 */

#ifndef _LITE_MUTEX_LOCK_H
#define _LITE_MUTEX_LOCK_H

#include "base/lite_base.h"
#include "base/noncopyable.h"
#include "mutex.h"

namespace lite {

class MutexLock : public NonCopyable
{
public:

    MutexLock(Mutex& mutex) : mutex_(mutex)
    {
        mutex_.Lock();
    }

    virtual ~MutexLock()
    {
        mutex_.Unlock();
    }

private:
    Mutex& mutex_;
};

}

using namespace lite;

#endif // ifndef _LITE_MUTEX_LOCK_H