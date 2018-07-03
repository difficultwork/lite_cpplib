/**
 * @file    network\iocp_base.h
 * @brief   Encapsulation for IOCP overlap data struct
 * @author  Nik Yan
 * @version 1.0     2014-07-20      Only support windows
 * @update          2014-09-16      Add addr_size_, bug free for UDP
 */

#ifndef _LITE_IOCP_BASE_H_
#define _LITE_IOCP_BASE_H_

#include "base/lite_base.h"
#include "event/mutex_lock.h"

#ifdef OS_WIN

#include <WinSock2.h>

#pragma comment(lib, "WS2_32.lib")

#define MAX_IO_BUFFER_SIZE              (4096)
#define WORKER_THREADS_PER_PROCESSOR    (2)
#define MEM_POOL_SIZE                   (1000)

namespace lite {

typedef enum _IO_OPERATION
{
    ACCEPT_POSTED = 0,
    RECV_POSTED,
    SEND_POSTED,
    NULL_POSTED
}IO_OPERATION;

/**
 * @brief   IO overlap data struct
 */ 
typedef struct _IOCP_IoContext
{
    OVERLAPPED      overlapped_;                ///> Overlapping structures for network operations(each socket needs one)
    SOCKET          sock_accept_;               ///> Socket used by accept network operation
    WSABUF          wsa_buf_;                   ///> WSA buffer that used to pass arguments to overlapping operations
    char            buf_[MAX_IO_BUFFER_SIZE];   ///> WSA buffer
    int             trans_len_;                 ///> Length of data transferred
    IO_OPERATION    operation_;                 ///> Network operation type
    SOCKADDR_IN     remote_addr_;               ///> Remote address
    int             addr_size_;                 ///> Remote address length(for UDP)

    _IOCP_IoContext()
    {
        ZeroMemory(&overlapped_,  sizeof(overlapped_));
        ZeroMemory(buf_,          MAX_IO_BUFFER_SIZE);
        ZeroMemory(&remote_addr_, sizeof(SOCKADDR_IN));
        wsa_buf_.buf = buf_;
        wsa_buf_.len = MAX_IO_BUFFER_SIZE;
        operation_   = NULL_POSTED;
        addr_size_   = sizeof(SOCKADDR_IN);
        trans_len_   = 0;
    }

    /**
     * @brief   Reset IO
     */
    void Reset()
    {
        if(INVALID_SOCKET == sock_accept_)
        {
            closesocket(sock_accept_);
            sock_accept_ = INVALID_SOCKET;
        }
        ZeroMemory(&overlapped_,  sizeof(overlapped_));
        ZeroMemory(buf_,          MAX_IO_BUFFER_SIZE);
        ZeroMemory(&remote_addr_, sizeof(SOCKADDR_IN));
        wsa_buf_.buf = buf_;
        wsa_buf_.len = MAX_IO_BUFFER_SIZE;
        operation_   = NULL_POSTED;
        addr_size_   = sizeof(SOCKADDR_IN);
        trans_len_   = 0;
    }

    /**
     * @brief   Clear buffer content
     */
    void ResetBuffer()
    {
        ZeroMemory(&overlapped_,  sizeof(overlapped_));
        ZeroMemory(buf_,          MAX_IO_BUFFER_SIZE);
        wsa_buf_.buf = buf_;
        wsa_buf_.len = MAX_IO_BUFFER_SIZE;
        addr_size_   = sizeof(SOCKADDR_IN);
        trans_len_   = 0;
    }
}IOCP_IoContext;

class IOCP_IoContextPool
{
public:
    IOCP_IoContextPool(uint32_t pool_size)
    {
        pool_size_ = pool_size;
    }

    ~IOCP_IoContextPool()
    {
        MutexLock lock(mt_);
        for (list<IOCP_IoContext*>::iterator it = list_io_context_.begin();
             it != list_io_context_.end(); it++)
        {
            delete (*it);
        }
        list_io_context_.clear();
    }

    IOCP_IoContext* GetIoContext()
    {
        IOCP_IoContext* context = NULL;
        MutexLock lock(mt_);
        if (0 == list_io_context_.size())
        {
            context = new IOCP_IoContext;
        }
        else
        {
            context = list_io_context_.front();
            list_io_context_.pop_front();
        }
        return context;
    }

    void PutIoContext(IOCP_IoContext* context)
    {
        context->Reset();
        MutexLock lock(mt_);
        if (pool_size_ <= list_io_context_.size())
        {
            delete context;
        }
        else
        {
            list_io_context_.push_back(context);
        }
    }

private:
    list<IOCP_IoContext*>   list_io_context_;
    uint32_t                pool_size_;
    Mutex                   mt_;
};

/**
 * @brief   Single handle data structure(for each IOCP)
 */
typedef struct _IOCP_SocketContext
{
    IOCP_IoContextPool*     pool_io_context_;
    SOCKET                  sock_;
    Mutex                   mt_io_list_;
    list<IOCP_IoContext*>   list_io_context_;
    IOCP_IoContext          recv_context_;
    SOCKADDR_IN             local_addr_;
    unsigned long           sock_id_;               ///> Socket ID
    bool                    is_listen_sock_;

    _IOCP_SocketContext(IOCP_IoContextPool* pool_io_context)
        : pool_io_context_(pool_io_context)
        , sock_(INVALID_SOCKET)
        , sock_id_(0)
        , is_listen_sock_(false)
    {
    }

    virtual ~_IOCP_SocketContext(void)
    {
        Reset();
    }

    /**
     * @brief   Reset connection
     */
    void Reset()
    {
        sock_id_        = 0;
        is_listen_sock_ = false;
        if (INVALID_SOCKET != sock_)
        {
            shutdown(sock_, SD_SEND);
            closesocket(sock_);
            sock_ = INVALID_SOCKET;
        }

        ZeroMemory(&local_addr_, sizeof(SOCKADDR_IN));
        recv_context_.Reset();

        MutexLock lock(mt_io_list_);
        for (list<IOCP_IoContext*>::iterator it = list_io_context_.begin(); it != list_io_context_.end(); it++)
        {
            pool_io_context_->PutIoContext((*it));
        }
        list_io_context_.clear();
    }

    void AddContext(IOCP_IoContext* context)
    {
        assert(NULL != context);
        {
            MutexLock lock(mt_io_list_);
            list_io_context_.push_back(context);
        }
    }

    void RemoveContext(IOCP_IoContext* context)
    {
        assert(NULL != context);
        {
            MutexLock lock(mt_io_list_);
            for (list<IOCP_IoContext*>::iterator it = list_io_context_.begin();
                 it != list_io_context_.end(); it++)
            {
                if (context == (*it))
                {
                    list_io_context_.erase(it);
                    break;
                }
            }
        }
        pool_io_context_->PutIoContext(context);
    }
}IOCP_SocketContext;

typedef std::tr1::shared_ptr<IOCP_SocketContext> IOCP_SocketContextPtr;

class IOCP_SocketContextPool
{
public:
    IOCP_SocketContextPool(IOCP_IoContextPool* pool_io_context, uint32_t pool_size)
        : pool_size_(pool_size)
        , pool_io_context_(pool_io_context)
    {
    }

    ~IOCP_SocketContextPool()
    {
        {
            MutexLock lock(mt_idle_);
            list_idle_.clear();
        }
        {
            MutexLock lock(mt_active_);
            map_active_.clear();
        }
    }

    /**
     * @brief   Get a handle
     */
    IOCP_SocketContextPtr GetSocketContext()
    {
        IOCP_SocketContextPtr context_ptr;
        MutexLock lock(mt_idle_);
        if (0 == list_idle_.size())
        {
            context_ptr = IOCP_SocketContextPtr(new IOCP_SocketContext(pool_io_context_));
        }
        else
        {
            context_ptr = list_idle_.front();
            list_idle_.pop_front();
        }
        return context_ptr;
    }

    /**
     * @brief   Release a handle
     */
    void PutSocketContext(IOCP_SocketContextPtr context_ptr)
    {
        MutexLock lock(mt_idle_);
        if (list_idle_.size() < pool_size_)
        {
            list_idle_.push_back(context_ptr);
        }
    }

    void AddActiveContext(IOCP_SocketContextPtr context_ptr)
    {
        MutexLock lock(mt_active_);
        map_active_[context_ptr->sock_id_] = context_ptr;
    }


    /**
     * @brief   Delete socket connection
     */
    void DelActiveContext(unsigned long sock_id)
    {
        IOCP_SocketContextPtr context_ptr;
        {
            MutexLock lock(mt_active_);
            map<unsigned long, IOCP_SocketContextPtr>::iterator it = map_active_.find(sock_id);
            if (it == map_active_.end())
            {
                return;
            } 
            else
            {
                context_ptr = it->second;
                map_active_.erase(it);
            }
        }
        context_ptr->Reset();
        {
            MutexLock lock(mt_idle_);
            if (list_idle_.size() < pool_size_)
            {
                list_idle_.push_back(context_ptr);
            }
        }
    }

    /**
     * @brief   Clear socket connections
     */
    void ClearActiveContext()
    {
        MutexLock lock(mt_active_);
        MutexLock lock(mt_idle_);
        for (map<unsigned long, IOCP_SocketContextPtr>::iterator it = map_active_.begin();
             it != map_active_.end(); it++)
        {
            it->second->Reset();
            if (list_idle_.size() < pool_size_)
            {
                list_idle_.push_back(it->second);
            }
        }
        map_active_.clear();
    }

    /**
     * @brief   Get an active socket connection by socket id
     */
    IOCP_SocketContextPtr GetActiveContext(unsigned long sock_id)
    {
        IOCP_SocketContextPtr context_ptr;
        MutexLock lock(mt_active_);
        map<unsigned long, IOCP_SocketContextPtr>::iterator it = map_active_.find(sock_id);
        if (it != map_active_.end())
        {
            context_ptr = it->second;
        }
        return context_ptr;
    }

private:
    IOCP_IoContextPool*                         pool_io_context_;
    list<IOCP_SocketContextPtr>                 list_idle_;
    Mutex                                       mt_idle_;
    uint32_t                                    pool_size_;
    map<unsigned long, IOCP_SocketContextPtr>   map_active_;
    Mutex                                       mt_active_;
};

} // end of namespace

using namespace lite;

#endif // ifdef OS_WIN

#endif // ifndef _LITE_IOCP_BASE_H_