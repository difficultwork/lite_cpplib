/**
 * @file    network\iocp_udpworkthread.h
 * @brief   Encapsulation for IOCP UDP work thread
 * @author  Nik Yan
 * @version 1.0     2014-07-21      Only support windows
 */

#ifndef _LITE_IOCP_UDP_WORK_THREAD_H_
#define _LITE_IOCP_UDP_WORK_THREAD_H_

#include "iocp_base.h"
#include "event/thread.h"

#ifdef OS_WIN
#include <MSWSock.h>

namespace lite {

/**
 * @brief   Callback function when udp socket recv msg
 * @param   sock_id     Socket ID which recv msg
 * @param   data        Msg data
 * @param   data_len    Msg length
 * @param   user_ptr    User pointer
 * @param   src_addr    Sender address
 * @note    This function needs to return quickly, or it will block the work thread of IOCP.
 *          It is recommended that processing messages be placed in the asynchronous queue first
 */
typedef void (*RECEIVEFROMCALLBACK)(
                                    unsigned long   sock_id,
                                    const char*     data,
                                    int             data_len,
                                    SOCKADDR_IN     src_addr,
                                    void*           user_ptr);

class IOCP_UDPWorkThread : public Thread
{
public:
    IOCP_UDPWorkThread(HANDLE iocp_handle, IOCP_SocketContextPool* pool_sock_context, void* user_ptr)
        : iocp_handle_(iocp_handle)
        , pool_sock_context_(pool_sock_context)
        , user_ptr_(user_ptr)
        , ReceiveFromCallback_(NULL)
    {
    }

    void RegisterCallbackFunc(RECEIVEFROMCALLBACK ReceiveFromCallback)
    {
        ReceiveFromCallback_ = ReceiveFromCallback;
    }

    /**
     * @brief   Bind Sockets to IOCP
     */
    bool AssociateWithIOCP(IOCP_SocketContextPtr sock_context);

    /**
     * @brief   Delivery recv
     */
    bool PostRecv(IOCP_SocketContextPtr sock_context);

    /**
     * @brief   Delivery send
     */
    bool PostSend(IOCP_SocketContextPtr sock_context, IOCP_IoContext* io_context);
protected:
    virtual uint32_t _Run();
    /**
     * @brief   Processing IO results
     */
    bool _HandleError(IOCP_SocketContextPtr sock_context, DWORD err);

    bool _DoRecv(IOCP_SocketContextPtr sock_context);

    void _DoSend(IOCP_SocketContextPtr sock_context, IOCP_IoContext* io_context)
    {
        sock_context->RemoveContext(io_context);
    }

private:

    RECEIVEFROMCALLBACK         ReceiveFromCallback_;

    HANDLE                      iocp_handle_;           ///> IOCP handle
    IOCP_SocketContextPool*     pool_sock_context_;     ///> Socket pool
    void*                       user_ptr_;              ///> User Pointer

};

inline
uint32_t IOCP_UDPWorkThread::_Run()
{
    DWORD                   bytes_transfered = 0;
    unsigned long           sock_id          = 0;
    LPOVERLAPPED            overlapped       = NULL;
    IOCP_IoContext*         io_data          = NULL;
    while (!_Signalled())
    {
        BOOL ret = GetQueuedCompletionStatus(iocp_handle_,
                                             &bytes_transfered,
                                             (PULONG_PTR)&sock_id,
                                             &overlapped,
                                             50);
        IOCP_SocketContextPtr sock_context = pool_sock_context_->GetActiveContext(sock_id);
        if (!ret)
        {
            if (_HandleError(sock_context, GetLastError()))
            {
                continue;
            }
            else
            {
                break;
            }
        }

        io_data = CONTAINING_RECORD(overlapped, IOCP_IoContext, overlapped_);
        if (NULL == io_data)
        {
            continue;
        }

        switch(io_data->operation_)
        {
        case RECV_POSTED:
            {
                sock_context->recv_context_.trans_len_ = (int)bytes_transfered;
                _DoRecv(sock_context);
            }
            break;

        case SEND_POSTED:
            {
                _DoSend(sock_context, io_data);
            }
            break;
        default:
            break;
        }
    }
    return 0;
}

inline
bool IOCP_UDPWorkThread::_HandleError(IOCP_SocketContextPtr sock_context, DWORD err)
{
    if (NULL == sock_context.get())
    {
        return true;
    }
    unsigned long sock_id = sock_context->sock_id_;
    if (WAIT_TIMEOUT == err)                // timeout
    {
        return true;
    }
    else if (ERROR_NETNAME_DELETED == err)
    {
        return true;
    }
    else                                    // IO exception
    {
        return false;
    }
}


inline
bool IOCP_UDPWorkThread::PostRecv(IOCP_SocketContextPtr sock_context)
{
    DWORD dw_flags = 0;
    int   ret      = 0;

    sock_context->recv_context_.ResetBuffer();
    ZeroMemory(&sock_context->recv_context_.remote_addr_, sizeof(SOCKADDR_IN));
    sock_context->recv_context_.operation_ = RECV_POSTED;

    ret = WSARecvFrom(sock_context->sock_,
					  &sock_context->recv_context_.wsa_buf_,
					  1,
					  (DWORD*)&sock_context->recv_context_.trans_len_,
					  &dw_flags,
                      reinterpret_cast<sockaddr*>(&sock_context->recv_context_.remote_addr_),
                      &sock_context->recv_context_.addr_size_,
					  &sock_context->recv_context_.overlapped_,
					  NULL);

    if ((0 != ret) && (WSA_IO_PENDING != WSAGetLastError()))
    {
        return false;
    }    

    return true;
}

inline
bool IOCP_UDPWorkThread::PostSend(IOCP_SocketContextPtr sock_context, IOCP_IoContext* io_context)
{
    int ret = 0;
    io_context->operation_ = SEND_POSTED;
    ret = WSASendTo(sock_context->sock_,
                    &io_context->wsa_buf_,
                    1,
                    (DWORD*)&io_context->trans_len_,
                    0,
                    reinterpret_cast<sockaddr*>(&io_context->remote_addr_),
                    sizeof(sockaddr),
                    &io_context->overlapped_,
                    NULL);
    if ((SOCKET_ERROR == ret) && (WSA_IO_PENDING != WSAGetLastError()))
    {
        return false;
    }
    return true;
}

inline
bool IOCP_UDPWorkThread::_DoRecv(IOCP_SocketContextPtr sock_context)
{
    ReceiveFromCallback_(sock_context->sock_id_,
                         sock_context->recv_context_.buf_,
                         sock_context->recv_context_.trans_len_,
                         sock_context->recv_context_.remote_addr_,
                         user_ptr_);
    // Delivery next WSARecv request
    return PostRecv(sock_context);
}

inline
bool IOCP_UDPWorkThread::AssociateWithIOCP(IOCP_SocketContextPtr sock_context)
{
    HANDLE handle = CreateIoCompletionPort((HANDLE)sock_context->sock_,
                                           iocp_handle_,
                                           (DWORD)sock_context->sock_id_,
                                           0);
    if (NULL == handle)
    {
        // Bind failed
        return false;
    }
    return true;
}

} // end of namespace
using namespace lite;

#endif // ifdef OS_WIN

#endif // ifndef _LITE_IOCP_UDP_WORK_THREAD_H_