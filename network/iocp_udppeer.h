/**
 * @file    network\iocp_udppeer.h
 * @brief   Encapsulation for IOCP UDP peer(server or client)
 * @author  Nik Yan
 * @version 1.0     2014-07-21      Only support windows
 */

#ifndef _LITE_IOCP_UDP_PEER_H_
#define _LITE_IOCP_UDP_PEER_H_

#include "iocp_udpworkthread.h"

#ifdef OS_WIN

namespace lite {

class IOCP_UDPNode
{
public:
    class IOCP_UDPNode()
        : iocp_handle_(NULL)
        , pool_io_context_(NULL)
        , pool_sock_context_(NULL)
        , user_ptr_(NULL)
        , is_start_(false)
        , ReceiveFromCallback_(NULL)
    {
    }

    /**
     * @brief   Initializing the UDP peer, registering the callback
     * @param   user_ptr                User pointer which will be used in callback
     * @return  true:Success, false:Failed
     */
    bool Init(
              void*                     user_ptr,
              RECEIVEFROMCALLBACK       ReceiveFromCallback);

    /**
     * @brief   Start IOCP
     * @return  true:Success, false:Failed
     */
    bool Start();

    /**
     * @brief   Create a UDP socket
     * @param   sock_id     Socket ID, valid after socket creation is successful
     * @param   dst_ip      Local ip address
     * @param   dst_port    Bind port
     * @return  true:Success, false:Failed
     */
    bool Create(unsigned long& sock_id, const char* bind_ip, UINT16& bind_port);

    /**
     * @brief   Close the socket
     * @param   sock_id     Socket ID
     */
    void CloseSocket(unsigned long sock_id)
    {
        pool_sock_context_->DelActiveContext(sock_id);
    }

    /**
     * @brief   Send msg(asynchronous delivery, not block)
     * @param   sock_id     Socket ID
     * @param   data        Msg data pointer
     * @param   data_len    Msg length
     * @param   dst_ip      Target ip address
     * @param   dst_port    Target port
     * @return  true:Success, false:Failed
     */
    bool SendTo(unsigned long sock_id, const char* data, int data_len, const char* dst_ip, UINT16 dst_port);

    /**
     * @brief   Send msg(asynchronous delivery, not block)
     * @param   sock_id     Socket ID
     * @param   data        Msg data pointer
     * @param   data_len    Msg length
     * @param   sock_addr   Target address
     * @return  true:Success, false:Failed
     */
    bool SendTo(unsigned long sock_id, const char* data, int data_len, SOCKADDR_IN& sock_addr);

    /**
     * @brief   Stop IOCP
     */
    void Stop();

    /**
     * @brief   Destroyed UDP peer
     */
    void DeInit();
protected:
    bool _InitializeIOCP()
    {
        return (NULL != (iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)));
    }

    void _InitializeWorkThread()
    {
        int num_thread = WORKER_THREADS_PER_PROCESSOR * _GetNoOfProcessors();
        for (int i = 0; i < num_thread; i++)
        {
            IOCP_UDPWorkThread* pthread = new IOCP_UDPWorkThread(iocp_handle_, pool_sock_context_, user_ptr_);
            pthread->RegisterCallbackFunc(ReceiveFromCallback_);
            list_work_thread_.push_back(pthread);
        }
    }

    int _GetNoOfProcessors()
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return (int)si.dwNumberOfProcessors;
    }
private:
    RECEIVEFROMCALLBACK         ReceiveFromCallback_;

    bool                        is_start_;
    HANDLE                      iocp_handle_;
    IOCP_SocketContextPool*     pool_sock_context_;
    IOCP_IoContextPool*         pool_io_context_;
    list<IOCP_UDPWorkThread*>   list_work_thread_;
    void*                       user_ptr_;
};

inline
bool IOCP_UDPNode::Init(
                        void*                     user_ptr,
                        RECEIVEFROMCALLBACK       ReceiveFromCallback)
{
    assert(NULL != ReceiveFromCallback);
    user_ptr_             = user_ptr;
    ReceiveFromCallback_  = ReceiveFromCallback;
    pool_io_context_      = new IOCP_IoContextPool(MEM_POOL_SIZE);
    pool_sock_context_    = new IOCP_SocketContextPool(pool_io_context_, 2*MEM_POOL_SIZE);

    if (!_InitializeIOCP())
    {
        return false;
    }

    _InitializeWorkThread();
    return true;
}

inline
bool IOCP_UDPNode::Start()
{
    if (is_start_)
    {
        return true;
    }
    for (list<IOCP_UDPWorkThread*>::iterator it = list_work_thread_.begin(); it != list_work_thread_.end(); it++)
    {
        (*it)->Start();
    }
    is_start_ = true;
    return true;
}

inline
bool IOCP_UDPNode::Create(unsigned long& sock_id, const char* bind_ip, UINT16& bind_port)
{
    if (!is_start_)
    {
        return false;
    }

    IOCP_SocketContextPtr sock_context = pool_sock_context_->GetSocketContext();

    sock_context->sock_ = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (INVALID_SOCKET == sock_context->sock_)
    {
        pool_sock_context_->PutSocketContext(sock_context);
        return false;
    }
    string ip = bind_ip;
    ZeroMemory(&sock_context->local_addr_, sizeof(SOCKADDR_IN));
    sock_context->local_addr_.sin_family = AF_INET;
    if ("*" == ip)
    {
        sock_context->local_addr_.sin_addr.s_addr = htonl(INADDR_ANY);
    } 
    else
    {
        sock_context->local_addr_.sin_addr.s_addr = inet_addr(bind_ip);
    }    
    sock_context->local_addr_.sin_port   = htons(bind_port);

    int ret = bind(sock_context->sock_, (SOCKADDR*)&sock_context->local_addr_, sizeof(SOCKADDR));
    if (0 != ret)
    {
        closesocket(sock_context->sock_);
        pool_sock_context_->PutSocketContext(sock_context);
        return false;
    }
    // if bind_port equals 0, will choose random port number to bind
    if (0 == bind_port)
    {
        ZeroMemory(&sock_context->local_addr_, sizeof(SOCKADDR_IN));
        int addr_len = sizeof(SOCKADDR_IN);
        if (0 != getsockname(sock_context->sock_, (SOCKADDR*)&sock_context->local_addr_, &addr_len))
        {
            closesocket(sock_context->sock_);
            pool_sock_context_->PutSocketContext(sock_context);
            return false;
        }
        bind_port = ntohs(sock_context->local_addr_.sin_port);
    }

    sock_context->sock_id_     = (unsigned long)sock_context->sock_;
    sock_id                    = sock_context->sock_id_;
    pool_sock_context_->AddActiveContext(sock_context);
    IOCP_UDPWorkThread* worker = list_work_thread_.front();
    if (!worker->AssociateWithIOCP(sock_context))
    {
        pool_sock_context_->DelActiveContext(sock_context->sock_id_);
        return false;
    }
    
    return worker->PostRecv(sock_context);
}

inline
bool IOCP_UDPNode::SendTo(unsigned long sock_id, const char* data, int data_len, const char* dst_ip, UINT16 dst_port)
{
    if (!is_start_)
    {
        return false;
    }
    IOCP_SocketContextPtr sock_content = pool_sock_context_->GetActiveContext(sock_id);
    if (NULL == sock_content.get())
    {
        return false;
    }

    IOCP_IoContext* io_context = pool_io_context_->GetIoContext();
    memcpy(io_context->buf_, data, data_len);
    io_context->wsa_buf_.len   = data_len;

    ZeroMemory(&io_context->remote_addr_, sizeof(SOCKADDR_IN));
    io_context->remote_addr_.sin_family      = AF_INET;
    io_context->remote_addr_.sin_addr.s_addr = inet_addr(dst_ip);
    io_context->remote_addr_.sin_port        = htons(dst_port);
    sock_content->AddContext(io_context);

    IOCP_UDPWorkThread* worker = list_work_thread_.front();
    return worker->PostSend(sock_content, io_context); 
}

inline
bool IOCP_UDPNode::SendTo(unsigned long sock_id, const char* data, int data_len, SOCKADDR_IN& sock_addr)
{
    if (!is_start_)
    {
        return false;
    }
    IOCP_SocketContextPtr sock_content = pool_sock_context_->GetActiveContext(sock_id);
    if (NULL == sock_content.get())
    {
        return false;
    }

    IOCP_IoContext* io_context = pool_io_context_->GetIoContext();
    memcpy(io_context->buf_, data, data_len);
    io_context->wsa_buf_.len   = data_len;

    memcpy(&io_context->remote_addr_, &sock_addr, sizeof(SOCKADDR_IN));
    sock_content->AddContext(io_context);

    IOCP_UDPWorkThread* worker = list_work_thread_.front();
    return worker->PostSend(sock_content, io_context); 
}

inline
void IOCP_UDPNode::Stop()
{
    if (!is_start_)
    {
        return;
    }
    for (list<IOCP_UDPWorkThread*>::iterator it = list_work_thread_.begin(); it != list_work_thread_.end(); it++)
    {
        (*it)->Signal();
    }
    for (list<IOCP_UDPWorkThread*>::iterator it = list_work_thread_.begin(); it != list_work_thread_.end(); it++)
    {
        (*it)->Stop();
    }

    pool_sock_context_->ClearActiveContext();
}

inline
void IOCP_UDPNode::DeInit()
{
    for (list<IOCP_UDPWorkThread*>::iterator it = list_work_thread_.begin(); it != list_work_thread_.end(); it++)
    {
        delete (*it);
    }
    list_work_thread_.clear();
    if (NULL != iocp_handle_)
    {
        CloseHandle(iocp_handle_);
        iocp_handle_ = NULL;
    }
    delete pool_sock_context_;
    delete pool_io_context_;
}
} // end of namespace
using namespace lite;

#endif // ifdef OS_WIN

#endif // ifndef _LITE_IOCP_UDP_PEER_H_