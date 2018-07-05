/**
 * @file    network\iocp_tcpworkthread.h
 * @brief   Encapsulation for IOCP TCP work thread
 * @author  Nik Yan
 * @version 1.0     2014-07-20      Only support windows
 */

#ifndef _LITE_IOCP_TCP_WORK_THREAD_H_
#define _LITE_IOCP_TCP_WORK_THREAD_H_

#include "iocp_base.h"
#include "event/thread.h"

#ifdef OS_WIN

#include <MSWSock.h>

namespace lite {

/**
 * @brief   Callback function when tcp server recv new tcp connection
 * @param   sock_id     Socket ID
 * @param   user_ptr    User pointer
 * @caution This function needs to return quickly
 */
typedef void (*CONNECTEDCALLBACK)(unsigned long sock_id, void* user_ptr);

/**
 * @brief   Callback function when tcp socket recv msg
 * @param   sock_id     Socket ID which recv msg
 * @param   data        Msg data
 * @param   data_len    Msg length
 * @param   user_ptr    User pointer
 * @note    This function needs to return quickly, or it will block the work thread of IOCP.
 *          It is recommended that processing messages be placed in the asynchronous queue first
 */
typedef void (*RECEIVEDCALLBACK)(unsigned long sock_id, const char* data, int data_len, void* user_ptr);

/**
 * @brief   Callback function when tcp disconnect
 * @param   sock_id     Socket ID which disconnect
 * @param   user_ptr    User pointer
 * @note    This function needs to return quickly
 */
typedef void (*DISCONNECTEDCALLBACK)(unsigned long sock_id, void* user_ptr);

class IOCP_TCPWorkThread : public Thread
{
public:
    IOCP_TCPWorkThread(HANDLE iocp_handle, IOCP_SocketContextPool* pool_sock_context, void* user_ptr)
        : iocp_handle_(iocp_handle)
        , pool_sock_context_(pool_sock_context)
        , user_ptr_(user_ptr)
        , AcceptEx_(NULL)
        , GetAcceptExSockAddrs_(NULL)
        , ReceivedCallback_(NULL)
        , DisconnectedCallback_(NULL)
    {
    }

    void RegisterCallbackFunc(
                              LPFN_ACCEPTEX             AcceptEx,
                              LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockAddrs,
                              CONNECTEDCALLBACK         ConnectedCallback,
                              RECEIVEDCALLBACK          ReceivedCallback,
                              DISCONNECTEDCALLBACK      DisconnectedCallback)
    {
        AcceptEx_             = AcceptEx;
        GetAcceptExSockAddrs_ = GetAcceptExSockAddrs;
        ConnectedCallback_    = ConnectedCallback;
        ReceivedCallback_     = ReceivedCallback;
        DisconnectedCallback_ = DisconnectedCallback;
    }

    /**
     * @brief   Bind Sockets to IOCP
     */
	bool AssociateWithIOCP(IOCP_SocketContextPtr sock_context);

    /**
     * @brief   Delivery acccept
     */
    bool PostAccept(IOCP_SocketContextPtr sock_context, IOCP_IoContext* io_context);

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

    bool _DoAccept(IOCP_SocketContextPtr sock_context, IOCP_IoContext* io_context);

    bool _DoRecv(IOCP_SocketContextPtr sock_context);

    void _DoSend(IOCP_SocketContextPtr sock_context, IOCP_IoContext* io_context)
    {
        sock_context->RemoveContext(io_context);
    }
    
private:
    LPFN_ACCEPTEX               AcceptEx_;              ///> AcceptEx function pointer
    LPFN_GETACCEPTEXSOCKADDRS   GetAcceptExSockAddrs_;  ///> Function pointers prepared for the AcceptEx function
                                                        ///> Returns the address of the local and remote machines
                                                        ///> in the first piece of data received by AcceptEx to the use

    CONNECTEDCALLBACK           ConnectedCallback_;
    RECEIVEDCALLBACK            ReceivedCallback_;
    DISCONNECTEDCALLBACK        DisconnectedCallback_;

    HANDLE                      iocp_handle_;           ///> IOCP handle
    IOCP_SocketContextPool*     pool_sock_context_;     ///> Socket pool
    void*                       user_ptr_;              ///> User Pointer

};

inline
uint32_t IOCP_TCPWorkThread::_Run()
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
                                             500);
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

        // If IO data has a length of 0 && (is a receive or send operation), connection needs to be closed
        if (0 == bytes_transfered && (RECV_POSTED == io_data->operation_ || SEND_POSTED == io_data->operation_))
        {
            // Unbind, free connection
            pool_sock_context_->DelActiveContext(sock_id);
            DisconnectedCallback_(sock_id, user_ptr_);
            continue;
        }

        switch(io_data->operation_)
        {
        case ACCEPT_POSTED:
            {
                _DoAccept(sock_context, io_data);
            }
            break;

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
bool IOCP_TCPWorkThread::AssociateWithIOCP(IOCP_SocketContextPtr sock_context)
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

inline
bool IOCP_TCPWorkThread::_HandleError(IOCP_SocketContextPtr sock_context, DWORD err)
{
    if (NULL == sock_context.get())
    {
        return true;
    }
    unsigned long sock_id = sock_context->sock_id_;
    if (!sock_context->is_listen_sock_ && WAIT_TIMEOUT == err) ///> timeout
    {  	
        // To confirm client is alive...
        int byte_send = send(sock_context->sock_, "", 0, 0);
        // Disconnect
        if (-1 == byte_send)
        {
            pool_sock_context_->DelActiveContext(sock_id);
            DisconnectedCallback_(sock_id, user_ptr_);
        }
        return true;
    }
    else if (!sock_context->is_listen_sock_ && ERROR_NETNAME_DELETED == err)  ///> Peer has been disconnected
    {
        pool_sock_context_->DelActiveContext(sock_id);
        DisconnectedCallback_(sock_id, user_ptr_);
        return true;
    }
    else ///> IO exception
    {
        return false;
    }
}

inline
bool IOCP_TCPWorkThread::PostAccept(IOCP_SocketContextPtr sock_context, IOCP_IoContext* io_context)
{
    assert(INVALID_SOCKET != sock_context->sock_);
    // Prepare params
    DWORD   bytes_transfer   = 0;  
    io_context->operation_   = ACCEPT_POSTED;
    io_context->sock_accept_ = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if(INVALID_SOCKET == io_context->sock_accept_)
    {
        return false;
    } 

    // Delivery AcceptEx
    if(!AcceptEx_(sock_context->sock_,
                  io_context->sock_accept_,
                  io_context->wsa_buf_.buf,
                  0,
                  sizeof(SOCKADDR_IN)+16,
                  sizeof(SOCKADDR_IN)+16,
                  &bytes_transfer,
                  &io_context->overlapped_))
    {
        if(WSA_IO_PENDING != WSAGetLastError())
        {
            // Delivery failed
            return false;
        }
    }
    return true;
}

inline
bool IOCP_TCPWorkThread::_DoAccept(IOCP_SocketContextPtr sock_context, IOCP_IoContext* io_context)
{
    SOCKADDR_IN* remote_addr = NULL;
    SOCKADDR_IN* local_addr  = NULL;
    int remote_len = sizeof(SOCKADDR_IN), local_len = sizeof(SOCKADDR_IN);

    GetAcceptExSockAddrs_(io_context->wsa_buf_.buf,
                          0,
                          sizeof(SOCKADDR_IN)+16,
                          sizeof(SOCKADDR_IN)+16,
                          (LPSOCKADDR*)&local_addr,
                          &local_len,
                          (LPSOCKADDR*)&remote_addr,
                          &remote_len);

    IOCP_SocketContextPtr new_sock_context = pool_sock_context_->GetSocketContext();
    new_sock_context->sock_                = io_context->sock_accept_;
    new_sock_context->sock_id_             = (unsigned long)io_context->sock_accept_;
    memcpy(&new_sock_context->local_addr_, remote_addr, sizeof(SOCKADDR_IN));

    pool_sock_context_->AddActiveContext(new_sock_context);
    if(!AssociateWithIOCP(new_sock_context))
    {
        pool_sock_context_->DelActiveContext(new_sock_context->sock_id_);
        return false;
    }

    ConnectedCallback_(new_sock_context->sock_id_, user_ptr_);
    if(!PostRecv(new_sock_context))
    {
        pool_sock_context_->DelActiveContext(new_sock_context->sock_id_);
        DisconnectedCallback_(new_sock_context->sock_id_, user_ptr_);
        return false;
    }

    io_context->ResetBuffer();
    return PostAccept(sock_context, io_context);
}

inline
bool IOCP_TCPWorkThread::PostRecv(IOCP_SocketContextPtr sock_context)
{
    DWORD dw_flags = 0;
    int   ret      = 0;

    sock_context->recv_context_.ResetBuffer();
    sock_context->recv_context_.operation_ = RECV_POSTED;
    ret = WSARecv(sock_context->sock_,
                  &sock_context->recv_context_.wsa_buf_,
                  1,
                  (DWORD*)&sock_context->recv_context_.trans_len_,
                  &dw_flags,
                  &sock_context->recv_context_.overlapped_,
                  NULL);
    if ((0 != ret) && (WSA_IO_PENDING != WSAGetLastError()))
    {
        pool_sock_context_->DelActiveContext(sock_context->sock_id_);
        DisconnectedCallback_(sock_context->sock_id_, user_ptr_);
        return false;
    }
    return true;
}

inline
bool IOCP_TCPWorkThread::PostSend(IOCP_SocketContextPtr sock_context, IOCP_IoContext* io_context)
{
    DWORD dw_flags = 0;
    int   ret      = 0;

    io_context->operation_ = SEND_POSTED;
    ret = WSASend(sock_context->sock_,
                  &io_context->wsa_buf_,
                  1,
                  (DWORD*)&io_context->trans_len_,
                  dw_flags,
                  &io_context->overlapped_,
                  NULL);
    if ((0 != ret) && (WSA_IO_PENDING != WSAGetLastError()))
    {
        pool_sock_context_->DelActiveContext(sock_context->sock_id_);
        DisconnectedCallback_(sock_context->sock_id_, user_ptr_);
        return false;
    }
    return true;
}

inline
bool IOCP_TCPWorkThread::_DoRecv(IOCP_SocketContextPtr sock_context)
{
    // First show the last data, then reset the status, issue the next recv request
    ReceivedCallback_(sock_context->sock_id_,
                      sock_context->recv_context_.buf_,
                      sock_context->recv_context_.trans_len_,
                      user_ptr_);
    // Delivery next WSARecv request
    return PostRecv(sock_context);
}

} // end of namespace
using namespace lite;

#endif // ifdef OS_WIN

#endif // ifndef _LITE_IOCP_TCP_WORK_THREAD_H_