#include <infiniband/verbs.h>
#include <infiniband/verbs_exp.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <string>
#include <memory>
#include <arpa/inet.h>

#include <QPSocket.h>


   Device::Device(const std::string& deviceName)
   {
     ibv_device* device = 0;
     if(deviceName.empty())
     {
        ibv_device** devList = ibv_get_device_list(0);

        if (!devList)
        {
          LOG_THROW(std::runtime_error, "Failed to get ib device list");
        }

        device = devList[0];
     }
     context_ = ibv_open_device(device);
     deviceName_ = deviceName;
   }

   Device::operator ibv_context* ()
   {
     return context_;
   }

   const std::string& Device::deviceName() const
   {
     return deviceName_;
   }

   ibv_context* Device::context()
   {
     return context_;
   }

   QPSocket::QPSocket(Device& device, const QPSocketCfg& cfg)
    : device_(device)
    , cfg_(cfg)
   {
     recvIdx_ = 0;
     sendIdx_ = 0;
   }

   void QPSocket::open()
   {
        ibv_qp_attr qp_attr;
        int qp_flags;
        int ret = 0;
        {
                std::memset(&qp_attr, 0, sizeof(qp_attr));
                qp_flags = IBV_QP_STATE | IBV_QP_PORT;
                qp_attr.qp_state = IBV_QPS_INIT;
                qp_attr.port_num = cfg_.portNum();

                ret = ibv_modify_qp(queuePair_, &qp_attr, qp_flags);
                if (ret != 0)
                {
                    LOG_THROW(std::runtime_error, "Failed to move QP to init");
                }
                else
                {
                    LOG("QP " << cfg_.flowName() << " moved to init");
                }
        }

        {
                std::memset(&qp_attr, 0, sizeof(qp_attr));
                qp_flags = IBV_QP_STATE;
                qp_attr.qp_state = IBV_QPS_RTR;
                ret = ibv_modify_qp(queuePair_, &qp_attr, qp_flags);

                if (ret != 0)
                {
                    LOG_THROW(std::runtime_error, "Failed to move QP to ready to receive");
                }
                else
                {
                    LOG("QP " << cfg_.flowName() << " moved to  RTR");
                }
        }
        {
                qp_flags = IBV_QP_STATE;
                qp_attr.qp_state = IBV_QPS_RTS;
                ret = ibv_modify_qp(queuePair_, &qp_attr, qp_flags);

                if (ret != 0)
                {
                    LOG_THROW(std::runtime_error, "Failed to move QP to ready to send");
                }
                else
                {
                    LOG("QP " << cfg_.flowName() << " moved to  RTS");
                }
        }

        for (auto i = 0; i < cfg_.numOfRecvBuffers() / 2; ++i)
        {
            postRecv();
        }
   }

   void QPSocket::initBuffers()
   {
     protectionDomain_ = ibv_alloc_pd(device_);

     auto recvPortion = cfg_.recvEntrySize() * cfg_.numOfRecvBuffers();
     auto sendPortion = cfg_.sendEntrySize() * cfg_.numOfSendBuffers();
     auto fullSize = recvPortion + sendPortion;

     recvBuf_  = reinterpret_cast<char*> (std::malloc(fullSize));
     sendBuf_  = recvBuf_ + recvPortion;

     memRegion_ = ibv_reg_mr(protectionDomain_, recvBuf_, fullSize, IBV_ACCESS_LOCAL_WRITE);
   }

   void QPSocket::initCompletionQueues()
   {
     recvCompletionQueue_ = ibv_create_cq(device_, cfg_.numOfRecvBuffers() , NULL, NULL, 0);
     sendCompletionQueue_ = ibv_create_cq(device_, cfg_.numOfSendBuffers(), NULL, NULL, 0);
   }

   void QPSocket::init()
   {
        initBuffers();
        initCompletionQueues();
        initQeuePair();
        initSniffer();
   }

   void QPSocket::postRecv()
   {
      postRecv( (recvIdx_++) % cfg_.numOfRecvBuffers());
   }

   std::string_view QPSocket::reserveSendBuf()
   {
        size_t bufIdx = (sendIdx_++) % cfg_.numOfSendBuffers();
        auto* bufBegin = reinterpret_cast<char*>(sendBuf_) + cfg_.sendEntrySize() * bufIdx;
        return std::string_view(bufBegin, cfg_.sendEntrySize());
   }

   void QPSocket::postRecv(size_t workID)
   {
        ibv_sge sg_entry;
        ibv_recv_wr wr, *bad_wr;

        sg_entry.length = cfg_.recvEntrySize();
        sg_entry.lkey = memRegion_->lkey;
        wr.num_sge = 1;
        wr.sg_list = &sg_entry;
        wr.next = 0;
        sg_entry.addr = reinterpret_cast<size_t>(recvBuf_) + cfg_.recvEntrySize() * workID;
        wr.wr_id = workID;
        int res = ibv_post_recv(queuePair_, &wr, &bad_wr);

        if (res != 0)
        {
          LOG_THROW(std::runtime_error, "Failed to post receive");
        }
   }

   void QPSocket::initSniffer()
   {
        struct ibv_flow_spec_eth        spec_eth;

        struct raw_eth_flow_attr
        {
                ibv_flow_attr            attr;
                ibv_flow_spec_eth        eth;
                ibv_flow_spec_ipv4       ipv4;
                ibv_flow_spec_tcp_udp    tcp;

        } __attribute__((packed)) fa;

        std::memset(&fa, 0 , sizeof(fa));
        fa.attr.comp_mask = 0;
        fa.attr.type = IBV_FLOW_ATTR_NORMAL;
        fa.attr.size = sizeof(fa);
        fa.attr.priority = 0;
        fa.attr.num_of_specs = 3;
        fa.attr.port = cfg_.portNum();
        fa.attr.flags = 0;

        //ETH
        fa.eth.type   = IBV_FLOW_SPEC_ETH;
        fa.eth.size   = sizeof(struct ibv_flow_spec_eth);

        std::memset(fa.eth.val.dst_mac, 0, sizeof(fa.eth.val.dst_mac));
        std::memset(fa.eth.val.src_mac, 0, sizeof(fa.eth.val.src_mac));
        fa.eth.val.ether_type = 0;
        fa.eth.val.vlan_tag = 0;

        std::memset(fa.eth.mask.dst_mac, 0, sizeof(fa.eth.mask.dst_mac));
        std::memset(fa.eth.mask.src_mac, 0, sizeof(fa.eth.mask.src_mac));

        fa.eth.mask.ether_type = 0;
        fa.eth.mask.vlan_tag = 0;

        if (!cfg_.sourceMAC().empty())
        {
            std::memcpy(fa.eth.val.src_mac, cfg_.sourceMACRaw().data(), cfg_.sourceMACRaw().size());
            std::memset(fa.eth.mask.src_mac, 0xFF, sizeof(fa.eth.mask.src_mac));
        }

        if (!cfg_.destMAC().empty())
        {
            std::memcpy(fa.eth.val.dst_mac, cfg_.destMACRaw().data(), cfg_.destMACRaw().size());
            std::memset(fa.eth.mask.dst_mac, 0xFF, sizeof(fa.eth.mask.dst_mac));
        }

        // IP
        fa.ipv4.type = IBV_FLOW_SPEC_IPV4;
        fa.ipv4.size = sizeof(ibv_flow_spec_ipv4);
        fa.ipv4.val.dst_ip = 0;
        fa.ipv4.val.src_ip = 0;
        fa.ipv4.mask.dst_ip = 0;
        fa.ipv4.mask.src_ip = 0;

        if (!cfg_.sourceIP().empty())
        {
            fa.ipv4.val.src_ip = cfg_.sourceIPRaw();
            memset(&fa.ipv4.mask.src_ip, 0xFF,sizeof(fa.ipv4.mask.src_ip));
        }

        if (!cfg_.destIP().empty())
        {
            fa.ipv4.val.dst_ip = cfg_.destIPRaw();
            memset(&fa.ipv4.mask.dst_ip, 0xFF, sizeof(fa.ipv4.mask.dst_ip));
        }

        // TCP
        fa.tcp.type = IBV_FLOW_SPEC_TCP;
        fa.tcp.size =  sizeof(ibv_flow_spec_tcp_udp);


        fa.tcp.val.dst_port = htons(cfg_.destPort());
        fa.tcp.val.src_port = htons(cfg_.sourcePort());

        fa.tcp.mask.dst_port = cfg_.destPortMask();
        fa.tcp.mask.src_port = cfg_.sourcePortMask();

        flow_ = ibv_create_flow(queuePair_, &fa.attr);

        if (!flow_)
        {
          LOG_THROW(std::runtime_error, "Failed to create flow");
        }
   }

   void QPSocket::initQeuePair()
   {
        ibv_qp_init_attr qp_attr;
        std::memset(&qp_attr, 0, sizeof(qp_attr));

        qp_attr.qp_context = 0;
        qp_attr.send_cq = sendCompletionQueue_;
        qp_attr.recv_cq = recvCompletionQueue_;

        qp_attr.qp_type = IBV_QPT_RAW_PACKET;
        qp_attr.cap.max_recv_wr = cfg_.numOfRecvBuffers();
        qp_attr.cap.max_recv_sge = 1;
        qp_attr.cap.max_send_wr = cfg_.numOfSendBuffers();
        qp_attr.cap.max_send_sge = 1;
        qp_attr.cap.max_inline_data = cfg_.maxInlineSendSize();

        queuePair_ = ibv_create_qp(protectionDomain_, &qp_attr);

        if (!queuePair_)
        {
          LOG_THROW(std::runtime_error, "Failed to create QP");
        }

   }

   bool QPSocket::pollRecv(size_t& workID, std::string_view& data)
   {
        ibv_wc completion;
        int msgs_completed = ibv_poll_cq(recvCompletionQueue_, 1, &completion);

        if( msgs_completed > 0)
        {
              workID = completion.wr_id;
              data = std::string_view(recvBuf_ + cfg_.recvEntrySize() * workID, completion.byte_len);
              return  true;
        }
        return false;
   }

  void QPSocket::pollSend()
  {
     ibv_wc completion;
     int msgs_completed = ibv_poll_cq(sendCompletionQueue_, 1, &completion);
  }


  void QPSocket::sendNoCopy(const std::string_view& data, size_t workID)
  {
    ibv_sge sg_entry_s;
    ibv_exp_send_wr swr, *bad_swr = nullptr;
    sg_entry_s.addr = reinterpret_cast<size_t>(data.data());
    sg_entry_s.length = data.size();
    sg_entry_s.lkey = memRegion_->lkey;

    memset(&swr, 0, sizeof(swr));

    swr.num_sge = 1;
    swr.sg_list = &sg_entry_s;
    swr.next = 0;
    swr.exp_opcode = static_cast<ibv_exp_wr_opcode> (IBV_WR_SEND);
    swr.wr_id = workID;
    swr.exp_send_flags |=  IBV_EXP_SEND_IP_CSUM;
    swr.exp_send_flags |=  IBV_SEND_SIGNALED;
    //swr.exp_send_flags |=  IBV_EXP_SEND_INLINE;

    int ret = ibv_exp_post_send(queuePair_, &swr, &bad_swr);

    if (ret != 0)
    {
      LOG_THROW(std::runtime_error, "Failed to post send");
    }
  }

  void QPSocket::send(const std::string_view& data)
  {
        size_t bufIdx  = (sendIdx_++) % cfg_.numOfSendBuffers();
        char* outBuf = sendBuf_ + bufIdx * cfg_.sendEntrySize();
        std::memcpy(outBuf, data.data(), data.size());
        std::string_view outData(outBuf, data.size());
        sendNoCopy(outData, bufIdx);
  }
