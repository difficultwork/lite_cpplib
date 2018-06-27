/**
 * @file    base\noncopyable.h
 * @brief   Definition of basic concept class noncopyable
 * @author  Nik Yan
 * @version 1.0     2014-05-10
 */

#ifndef _LITE_NONCOPYABLE_H_
#define _LITE_NONCOPYABLE_H_

#include "lite_base.h"

namespace lite {

/**
 * @brief   To prevent copying of instance objects of a custom class, derive from the Nocopyable class
 */
class NonCopyable 
{
protected:
    NonCopyable() 
    {
    }
    
    virtual ~NonCopyable() 
    {
    }

private:
    NonCopyable( NonCopyable const& );
    void operator = ( NonCopyable const& );
};

} // end of namespace lite

using namespace lite;

#endif // ifndef _LITE_NONCOPYABLE_H_

