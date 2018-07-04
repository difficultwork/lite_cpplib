/**
 * @file    network\iocp_tcpsever.h
 * @brief   Encapsulation for IOCP TCP Server
 * @author  Nik Yan
 * @version 1.0     2014-07-21      Only support windows
 */

#ifndef _LITE_IOCP_TCP_SERVER_H_
#define _LITE_IOCP_TCP_SERVER_H_

#include "iocp_tcpworkthread.h"

#ifdef OS_WIN

namespace lite {

class IOCP_TCPServer
{
public:
    class IOCP_TCPServer()
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
     * @param   listen_port             Port number for server listening
     * @param   host_ip                 Ip address string for server, and if NULL,
     *                                  the program automatically binds the network card
     * @return  true:Success, false:Failed
     */
    bool Init(
              void*                     user_ptr,
              CONNECTEDCALLBACK         ConnectedCallback,
              RECEIVEDCALLBACK          ReceivedCallback,
              DISCONNECTEDCALLBACK      DisconnectedCallback,
              UINT16                    listen_port,
              const char*               host_ip = NULL);

    /**
     * @brief   Start IOCP
     * @return  true:Success, false:Failed
     */
    bool Start();

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
     * @brief   Destroyed TCP server
     */
    void DeInit();
protected:
    string _GetLocalIP()
    {
        // Get local host name
        char host_name[MAX_PATH] = {0};
        gethostname(host_name, MAX_PATH);                
        struct hostent FAR* host_ent = gethostbyname(host_name);
        if(NULL == host_ent)
        {
            return string("127.0.0.1");
        }

        LPSTR lp_addr = host_ent->h_addr_list[0];      
        struct in_addr inAddr;
        memmove(&inAddr, lp_addr, 4);
        return string(inet_ntoa(inAddr));
    }

    bool _InitializeIOCP()
    {
        return (NULL != (iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)));
    }

    bool _InitializeListenSocket();

    void _InitializeWorkThread()
    {
        int num_thread = WORKER_THREADS_PER_PROCESSOR * _GetNoOfProcessors();
        for (int i = 0; i < num_thread; i++)
        {
            IOCP_TCPWorkThread* pthread = new IOCP_TCPWorkThread(iocp_handle_, pool_sock_context_, user_ptr_);
            pthread->RegisterCallbackFunc(AcceptEx_,
                                          GetAcceptExSockAddrs_,
                                          ConnectedCallback_,
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
    CONNECTEDCALLBACK           ConnectedCallback_;
    RECEIVEDCALLBACK            ReceivedCallback_;
    DISCONNECTEDCALLBACK        DisconnectedCallback_;

    bool                        is_start_;
    HANDLE                      iocp_handle_;
    IOCP_SocketContextPool*     pool_sock_context_;
    IOCP_IoContextPool*         pool_io_context_;
    UINT16                      listen_port_;
    string                      host_ip_;
    list<IOCP_TCPWorkThread*>   list_work_thread_;
    IOCP_SocketContextPtr       listen_sock_context_;
    void*                       user_ptr_;
};

inline
bool IOCP_TCPServer::Init(
                          void*                     user_ptr,
                          CONNECTEDCALLBACK         ConnectedCallback,
                          RECEIVEDCALLBACK          ReceivedCallback,
                          DISCONNECTEDCALLBACK      DisconnectedCallback,
                          UINT16                    listen_port,
                          const char*               host_ip)
{
    assert(NULL != ReceivedCallback && NULL != DisconnectedCallback);
    user_ptr_             = user_ptr;
    ConnectedCallback_    = ConnectedCallback;
    ReceivedCallback_     = ReceivedCallback;
    DisconnectedCallback_ = DisconnectedCallback;
    listen_port_          = listen_port;
    pool_io_context_      = new IOCP_IoContextPool(MEM_POOL_SIZE);
    pool_sock_context_    = new IOCP_SocketContextPool(pool_io_context_, 2*MEM_POOL_SIZE);
    if (NULL == host_ip)
    {
        host_ip_ = _GetLocalIP();
    }
    else
    {
        host_ip_ = host_ip;
    }

    if (!_InitializeIOCP())
    {
        return false;
    }

    if (!_InitializeListenSocket())
    {
        return false;
    }

    _InitializeWorkThread();
    return true;
}

inline
bool IOCP_TCPServer::Start()
{
    if (is_start_)
    {
        return true;
    }
    for (list<IOCP_TCPWorkThread*>::iterator it = list_work_thread_.begin(); it != list_work_thread_.end(); it++)
    {
        (*it)->Start();
        // Delivery listen socket
        IOCP_IoContext* io_context = pool_io_context_->GetIoContext();
        listen_sock_context_->list_io_context_.push_back(io_context);
        (*it)->PostAccept(listen_sock_context_, io_context);
    }
    is_start_ = true;
    return true;
}

inline
bool IOCP_TCPServer::Send(unsigned long sock_id, const char* data, int data_len)
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
void IOCP_TCPServer::Stop()
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

    for (list<IOCP_IoContext*>::iterator it = listen_sock_context_->list_io_context_.begin();
         it != listen_sock_context_->list_io_context_.end(); it++)
    {
        pool_io_context_->PutIoContext((*it));
    }
    listen_sock_context_->list_io_context_.clear();
}

inline
void IOCP_TCPServer::DeInit()
{
    if (INVALID_SOCKET != listen_sock_context_->sock_)
    {
        closesocket(listen_sock_context_->sock_);
        listen_sock_context_->sock_ = INVALID_SOCKET;
    }
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

inline
bool IOCP_TCPServer::_InitializeListenSocket()
{
    // GUID of AcceptEx and GetAcceptExSockaddrs, used to export function
    GUID guid_acceptex             = WSAID_ACCEPTEX;  
    GUID guid_getacceptexsockaddrs = WSAID_GETACCEPTEXSOCKADDRS; 

    // Overlap IO should be use WSASocket to create socket
    listen_sock_context_                  = pool_sock_context_->GetSocketContext();
    listen_sock_context_->is_listen_sock_ = true;
    listen_sock_context_->sock_           = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (INVALID_SOCKET == listen_sock_context_->sock_) 
    {
        return false;
    }
    listen_sock_context_->sock_id_ = (unsigned long)listen_sock_context_->sock_;
    pool_sock_context_->AddActiveContext(listen_sock_context_);

    do 
    {
        if(NULL== CreateIoCompletionPort((HANDLE)listen_sock_context_->sock_,
                                         iocp_handle_,
                                         (DWORD)listen_sock_context_->sock_id_,
                                         0))
        {
            break;
        }

        // Bind socket
        struct sockaddr_in host_addr;
        ZeroMemory((char*)&host_addr, sizeof(host_addr));
        host_addr.sin_family      = AF_INET;
        if ("*" == host_ip_)
        {
            host_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        } 
        else
        {
            host_addr.sin_addr.s_addr = inet_addr(host_ip_.c_str());
        }        
        host_addr.sin_port        = htons(listen_port_);

        if (SOCKET_ERROR == bind(listen_sock_context_->sock_, (struct sockaddr*)&host_addr, sizeof(host_addr)))
        {
            break;
        }

        if (SOCKET_ERROR == listen(listen_sock_context_->sock_, SOMAXCONN))
        {
            break;
        }

        // Get callback pointer
        DWORD dw_bytes = 0;
        if (SOCKET_ERROR == WSAIoctl(listen_sock_context_->sock_,
                                     SIO_GET_EXTENSION_FUNCTION_POINTER,
                                     &guid_acceptex,
                                     sizeof(guid_acceptex),
                                     &AcceptEx_,
                                     sizeof(AcceptEx_),
                                     &dw_bytes,
                                     NULL,
                                     NULL))
        {
            break;
        }

        if (SOCKET_ERROR == WSAIoctl(listen_sock_context_->sock_,
                                     SIO_GET_EXTENSION_FUNCTION_POINTER,
                                     &guid_getacceptexsockaddrs,
                                     sizeof(guid_getacceptexsockaddrs),
                                     &GetAcceptExSockAddrs_,
                                     sizeof(GetAcceptExSockAddrs_),
                                     &dw_bytes,
                                     NULL,
                                     NULL))
        {
            break;
        }

        return true;
    } while (0);

    pool_sock_context_->DelActiveContext(listen_sock_context_->sock_id_);
    return false;
}

} // end of namespace
using namespace lite;

#endif // ifdef OS_WIN

#endif // ifndef _LITE_IOCP_TCP_SERVER_H_