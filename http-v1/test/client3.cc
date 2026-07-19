//给服务器发送一个数据，告诉服务器要发送1024个数据，但实际上发送的数据不足1024，查看服务器处理结果
/*  1.如果数据只发送了一次，服务器得不到完整请求，不会进行业务处理，
    2.如果数据发送了多次，服务器会把第二次的报头当报文接受，第二次的请求可能被出错*/
#include "../source/server.hpp"
int main()
{
    Socket cli_sock;
    cli_sock.CreateClient(8085, "127.0.0.1");
    std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n这是一条测试请求";
    while(1) {
        assert(cli_sock.Send(req.c_str(), req.size()) != -1);
        char buf[1024] = {0};
        assert(cli_sock.Recv(buf, 1023));
        DBG_LOG("[%s]", buf);
        sleep(3);
    }
    cli_sock.Close();
    return 0;
}