/*
 * Copyright 2013, Jeffery Qiu. All rights reserved.
 *
 * Licensed under the GNU LESSER GENERAL PUBLIC LICENSE(the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.gnu.org/licenses/lgpl.html
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//// Author: Jeffery Qiu (dungeonsnd at gmail dot com)
////

#include "netserver/cl_server.hpp"

class IOCompleteHandler : public cl::EventHandler
{
public:
    IOCompleteHandler():_headLen(4)
    {
    }
    ~IOCompleteHandler()
    {
    }
    cf_void OnAcceptComplete(cf::T_SESSION session)
    {
        CF_PRINT_FUNC;
#if CF_SWITCH_PRINT
        fprintf (stderr, "OnAcceptComplete,fd=%d,addr=%s \n",
                 session->Fd(),session->Addr().c_str());
#endif
        AsyncRead(session->Fd(), _headLen);
        _recvHeader[session->Fd()] =true;
    }
    cf_void OnReadComplete(cf::T_SESSION session,
                           std::shared_ptr < cl::ReadBuffer > readBuffer)
    {
        CF_PRINT_FUNC;
#if CF_SWITCH_PRINT
        //        fprintf (stderr, "OnReadComplete,fd=%d,addr=%s,total()=%d,buf=%s \n",
        //                 session->Fd(),session->Addr().c_str(),readBuffer->GetTotal(),
        //                 (cf_char *)(readBuffer->GetBuffer()));
#endif
        //        printf("pid=%d \n",int(getpid()));
        cf_uint32 totalLen =readBuffer->GetTotal();
        if(_recvHeader[session->Fd()])
        {
            if(_headLen!=totalLen)
            {
#if CF_SWITCH_PRINT
                //                fprintf (stderr, "OnReadComplete,fd=%d,_headLen{%u}!=totalLen{%u} \n",
                //                         session->Fd(),_headLen,totalLen);
#endif
            }
            else
            {
                cf_uint32 * p =(cf_uint32 *)(readBuffer->GetBuffer());
                cf_uint32 size =ntohl(*p);
                _recvHeader[session->Fd()] =false;
                if(size>0)
                    AsyncRead(session->Fd(), size);
                else
                    _THROW(cf::ValueError, "size==0 !");
            }
        }
        else
        {
            _recvHeader[session->Fd()] =true;
            AsyncWrite(session->Fd(), readBuffer->GetBuffer(), totalLen);
            AsyncRead(session->Fd(), _headLen);
        }
    }
    cf_void OnWriteComplete(cf::T_SESSION session)
    {
        CF_PRINT_FUNC;
    }

    virtual cf_void OnCloseComplete(cf::T_SESSION session)
    {
        CF_PRINT_FUNC;
    }
    virtual cf_void OnTimeoutComplete()
    {
        CF_PRINT_FUNC;
    }
    virtual cf_void OnErrorComplete(cf::T_SESSION session)
    {
        CF_PRINT_FUNC;
    }

private:
    std::map < cf_fd , bool > _recvHeader;
    const cf_uint32 _headLen;
};

int g_numprocess =0;
cf_void Run()
{
    typedef cl::TcpServer < IOCompleteHandler > ServerType;
    cf_fd severfd =ServerType::CreateListenSocket(8601);

    std::vector < pid_t > pids;
    pid_t pid;
    for(int i=0; i<g_numprocess; i++)
    {
        pid =fork();
        if(pid<0)
        {
            printf("fork error,error=%s \n",strerror(errno));
            break;
        }
        else if(pid==0)
        {
            IOCompleteHandler ioHandler;
            std::shared_ptr < ServerType > server(new ServerType(severfd,ioHandler));
            printf("child, pid=%d \n",int(getpid()));
            server->Start();
            return ;
        }
        else
        {
            printf("parent, pid=%d \n",int(getpid()));
            pids.push_back(pid);
            continue;
        }
    }

    int status =0;
    for(int i=0; i<int(pids.size()); i++)
    {
        waitpid(pids[i],&status,WUNTRACED | WCONTINUED);
    }
}

cf_int main(cf_int argc,cf_char * argv[])
{
    if(argc<2)
    {
        printf("Usage:%s <process count> \n",argv[0]);
        return 0;
    }
    g_numprocess =atoi(argv[1]);
    Run();
    return 0;
}
