/**
 * @file    network\iocp_tcpclient.h
 * @brief   Encapsulation for IOCP TCP client
 * @author  Nik Yan
 * @version 1.0     2014-07-21
 */

#ifndef _LITE_IOCP_TCP_CLIENT_H_
#define _LITE_IOCP_TCP_CLIENT_H_

#include "iocp_tcpworkthread.h"

#ifdef OS_WIN

namespace lite {

class IOCP_TCPClient
{
public:
    class IOCP_TCPClient()
        : iocp_handle_(NULL)
        , pool_io_context_(NULL)
        , pool_sock_context_(NULL)
        , user_ptr_(NULL)
        , is_start_(false)
        , ReceivedCallback_(NULL)
        , DisconnectedCallback_(NULL)
    {
    }

    /**
     * @brief   Initializing the TCP client, registering the callback
     * @param   user_ptr                User pointer which will be used in callback
     * @return  true:Success, false:Failed
     */
    bool Init(
              void*                     user_ptr,
              RECEIVEDCALLBACK          ReceivedCallback,
              DISCONNECTEDCALLBACK      DisconnectedCallback);

    /**
     * @brief   Start IOCP
     * @return  true:Success, false:Failed
     */
    bool Start();

    /**
     * @brief   Create a TCP connection
     * @param   sock_id     Socket ID, valid after socket creation is successful
     * @param   dst_ip      Target ip address
     * @param   dst_port    Target port
     * @return  true:Success, false:Failed
     */
    bool Connect(unsigned long& sock_id, const char* dst_ip, UINT16 dst_port);

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
     * @return  true:Success, false:Failed
     */
    bool Send(unsigned long sock_id, const char* data, int data_len);

    /**
     * @brief   Stop IOCP
     */
    void Stop();

    /**
     * @brief   Destroyed TCP client
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
            IOCP_TCPWorkThread* pthread = new IOCP_TCPWorkThread(iocp_handle_, pool_sock_context_, user_ptr_);
            pthread->RegisterCallbackFunc(NULL,
                                          NULL,
                                          NULL,
                                          ReceivedCallback_,
                                          DisconnectedCallback_);
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
    LPFN_ACCEPTEX               AcceptEx_;
    LPFN_GETACCEPTEXSOCKADDRS   GetAcceptExSockAddrs_;
    RECEIVEDCALLBACK            ReceivedCallback_;
    DISCONNECTEDCALLBACK        DisconnectedCallback_;

    bool                        is_start_;
    HANDLE                      iocp_handle_;
    IOCP_SocketContextPool*     pool_sock_context_;
    IOCP_IoContextPool*         pool_io_context_;
    list<IOCP_TCPWorkThread*>   list_work_thread_;
    void*                       user_ptr_;
};

inline
bool IOCP_TCPClient::Init(
                          void*                     user_ptr,
                          RECEIVEDCALLBACK          ReceivedCallback,
                          DISCONNECTEDCALLBACK      DisconnectedCallback)
{
    assert(NULL != ReceivedCallback && NULL != DisconnectedCallback);
    user_ptr_             = user_ptr;
    ReceivedCallback_     = ReceivedCallback;
    DisconnectedCallback_ = DisconnectedCallback;
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
bool IOCP_TCPClient::Start()
{
    if (is_start_)
    {
        return true;
    }
    for (list<IOCP_TCPWorkThread*>::iterator it = list_work_thread_.begin(); it != list_work_thread_.end(); it++)
    {
        (*it)->Start();
    }
    is_start_ = true;
    return true;
}

inline
bool IOCP_TCPClient::Connect(unsigned long& sock_id, const char* dst_ip, UINT16 dst_port)
{
    if (!is_start_)
    {
        return false;
    }

    IOCP_SocketContextPtr sock_context = pool_sock_context_->GetSocketContext();

    sock_context->sock_ = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (INVALID_SOCKET == sock_context->sock_)
    {
        pool_sock_context_->PutSocketContext(sock_context);
        return false;
    }
	
    SOCKADDR_IN* remote_addr     = &sock_context->recv_context_.remote_addr_;
    ZeroMemory(remote_addr, sizeof(SOCKADDR_IN));
    remote_addr->sin_family      = AF_INET;
    remote_addr->sin_addr.s_addr = inet_addr(dst_ip);
    remote_addr->sin_port        = htons(dst_port);
    int ret = WSAConnect(sock_context->sock_,
                         (PSOCKADDR)remote_addr,
                         sizeof(SOCKADDR_IN),
                         NULL, NULL, NULL, NULL);
    if (SOCKET_ERROR == ret && WSAEWOULDBLOCK != WSAGetLastError())
    {
        closesocket(sock_context->sock_);
        sock_context->sock_ = INVALID_SOCKET;
        pool_sock_context_->PutSocketContext(sock_context);
        return false;
    }

    sock_context->sock_id_     = (unsigned long)sock_context->sock_;
    sock_id                    = sock_context->sock_id_;
    pool_sock_context_->AddActiveContext(sock_context);
    IOCP_TCPWorkThread* worker = list_work_thread_.front();
    if (!worker->AssociateWithIOCP(sock_context))
    {
        pool_sock_context_->DelActiveContext(sock_context->sock_id_);
        return false;
    }
    return worker->PostRecv(sock_context);
}

inline
bool IOCP_TCPClient::Send(unsigned long sock_id, const char* data, int data_len)
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
    sock_content->AddContext(io_context);

    IOCP_TCPWorkThread* worker = list_work_thread_.front();
    return worker->PostSend(sock_content, io_context); 
}

inline
void IOCP_TCPClient::Stop()
{
    if (!is_start_)
    {
        return;
    }
    for (list<IOCP_TCPWorkThread*>::iterator it = list_work_thread_.begin(); it != list_work_thread_.end(); it++)
    {
        (*it)->Signal();
    }
    for (list<IOCP_TCPWorkThread*>::iterator it = list_work_thread_.begin(); it != list_work_thread_.end(); it++)
    {
        (*it)->Stop();
    }
    
    pool_sock_context_->ClearActiveContext();
}

inline
void IOCP_TCPClient::DeInit()
{
    for (list<IOCP_TCPWorkThread*>::iterator it = list_work_thread_.begin(); it != list_work_thread_.end(); it++)
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

#endif // ifndef _LITE_IOCP_TCP_CLIENT_H_