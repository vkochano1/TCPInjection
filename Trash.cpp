#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <arpa/inet.h>
#include <thread>
#include <future>
#include <chrono>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

const int32_t printEachN = 1;
struct Sock
{
  Sock(short port)
  {
   	    ss_ = socket(AF_INET, SOCK_STREAM, 0);
	      sockaddr_in  addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);
	      int b = bind(ss_, (sockaddr*) &addr, sizeof(addr));
	      listen(ss_, 1);
  }

  void acc()
  {
    int yes = 1;
    struct linger sl;
    sl.l_onoff = 1;		/* non-zero value enables linger option in kernel */
    sl.l_linger = 0;	/* timeout interval in seconds */
    setsockopt(ss_, SOL_SOCKET, SO_LINGER, &sl, sizeof(sl));
    cs_ = accept(ss_, 0, 0);

    if (setsockopt(cs_, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) != 0)
    {
      std::exit(0);
    }

    int sendbuff = 198304000;

    if (setsockopt(cs_, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff)  ) != 0)
    {
      std::exit(0);
    }
  }
  void send(char* buf, size_t len)
  {
        int wr = write(cs_, (void*)buf, len);
        if (wr <=  0)
          std::cerr << "ERROR writing" <<std::endl;
  }

  int recv()
  {
        static size_t c = 0;
        char buf[100];
        int len = read(cs_, buf, sizeof(buf));
        if(len > 0 )
        {
          if (c % printEachN == 0)
          {
              std::cerr << "A Server received" << std::string(buf, len) << std::endl;
          }
        }
        c++;
        return len > 0;
 }

  int ss_;
  int cs_;
};

struct CSock
{
  CSock( const char* ip, short port )
  {
        std::cerr << "Creating client sock  "<< port << std::endl;
        cs_ = socket(AF_INET, SOCK_STREAM, 0);
	      struct sockaddr_in addr;

	      inet_pton(AF_INET, ip, &(addr.sin_addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        int ret = connect(cs_, (sockaddr*) &addr, sizeof(addr));
        int yes = 1;
        if (setsockopt(cs_, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) != 0)
        {
          std::exit(0);
        }
        int sendbuff = 198304000;
        setsockopt(cs_, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff));

        std::cout << " Connected res  " << ret   <<  std::endl;
  }

  void send(char* buf, size_t len)
  {
       int wr = write(cs_, (void*)buf, len);
       if (wr<= 0)
          std::cerr << "ERROR writing" <<std::endl;
  }

  int recv()
  {
        char buf[100];
        static size_t c = 0;
        int len = read(cs_, buf, sizeof(buf));
        if(len > 0)
        {
          c++;
          if (c % printEachN == 0)
            std::cerr << "A  Client Received" <<  std::string(buf, len) << std::endl;
        }
        return len > 0 ;
  }

  int cs_;
};


int main()
{

 Sock ssock(5556);
 std::unique_ptr<std::thread> thr
 (
  new std::thread
  (
      [&ssock] ()
      {
         std::cerr << "Server started" << std::endl;
         ssock.acc();
         std::cerr << "Server Accepted" << std::endl;
         size_t idx = 0;
         while(1)
 		     {
                std::string buf = "PING_SERVER";
                buf += std::to_string(idx++);
   			        ssock.recv();
                std::cerr << " Server sent " << buf << '\n';
   			        ssock.send((char*)buf.c_str(), buf.length());

         }

		  }
  )
 );

 CSock g ("192.168.2.2", 5555);
 std::this_thread::sleep_for(std::chrono::milliseconds(2));
 size_t idx = 0;
 for(int i = 0; i < 10000000; )
 {
    std::string buf = "PING_CLIENT1234567890123456789012345678901234567890";
    buf += std::to_string(idx++);
    g.send((char*)buf.c_str(), buf.length());
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    g.recv();

 }
 thr->join();
 return 0;
}
