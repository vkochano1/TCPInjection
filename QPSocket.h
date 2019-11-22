
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <string>
#include <tins/ethernetII.h>
#include <tins/tins.h>
#include <memory>

#include <iostream>
#include <sstream>
#include <Utils.h>

#include <TCPPacket.h>
#include <QPSocketCfg.h>



struct ibv_context;
struct ibv_pd;
struct ibv_mr;
struct ibv_cq;
struct ibv_qp;
struct ibv_flow;

class Device final
{
 public:
   Device(const std::string& deviceName = std::string());

   operator ibv_context* ();
   const std::string& deviceName() const;
   ibv_context* context();
private:
   ibv_context* context_;
   std::string deviceName_;
};

class QPSocket
{
 public:
   QPSocket(Device& device, const QPSocketCfg& cfg);

public:
   void init();
   void open();

public:
   void postRecv(size_t workID);
   void postRecv();
   bool pollRecv(size_t& workID, std::string_view& data);

public:
   void send(size_t len, const char* buf, size_t workID = 0);
   void pollSend();

protected:
  void initBuffers();
  void initCompletionQueues();
  void initQeuePair();
  void initSniffer();

protected:
  Device& device_;
  ibv_pd*  memDomain_;
  ibv_mr*  memRegion_;
  ibv_cq*  recvCompletionQueue_;
  ibv_cq*  sendCompletionQueue_;
  ibv_qp*  queuePair_;
  ibv_flow* flow_;

public:
  char* recvBuf_;
  char* sendBuf_;
public:
  QPSocketCfg cfg_;
  size_t recvIdx_;
};
