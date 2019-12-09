#pragma once

#include <Utils.h>
#include <boost/icl/map.hpp>
#include <boost/icl/interval_map.hpp>
#include <boost/icl/interval_map.hpp>

using namespace boost::icl;

class ChangeDetails
{
public:
    enum InjectionType
    {
      Added,
      Reject
    };

    ChangeDetails();
    ChangeDetails(    InjectionType injectionType
                    , uint32_t seq
                    , std::string_view payload
                    , uint32_t rejSeq = 0
                    , std::string_view rejectPayload = std::string_view()
                    , bool dontChange = false
                    , uint32_t origSeq = 0);

    friend std::ostream& operator << (std::ostream& ostrm, ChangeDetails inj);

    InjectionType injectionType () const;

    uint32_t expectedAck() const;

    uint32_t outSeq() const;
    uint32_t inSeq() const;
    uint32_t rejSeq() const;

    bool dontChange() const;

    std::string_view  rejPayload() const;
    std::string_view payload() const;

    uint32_t payloadSize() const;

    bool split(uint32_t seq, std::string_view payload, std::string_view& left, std::string_view& right) const;

    bool operator == (const ChangeDetails& interval) const;
    bool operator < (const ChangeDetails& interval) const;

private:
    InjectionType injectionType_;
    uint32_t outSeq_;
    uint32_t inSeq_;

    std::string_view payload_;

    uint32_t rejSeq_;
    bool dontChange_;
    std::string_view rejPayload_;
};


struct PendingInjections
{
  using  Intervals = interval_map<uint32_t, std::set<ChangeDetails> >;
  using  IntervalRange = std::pair<Intervals::const_iterator, Intervals::const_iterator>;

  PendingInjections();

  Intervals& intervals();

  void processReceivedAck(uint32_t ackNum, uint32_t& bytesAcked, uint32_t& bytesRejectedAcked);


  template <typename OnPassThroughPayload, typename OnRejectedPayload>
  void processDupPayload(uint32_t recvSeq, std::string_view dupData
                      , uint32_t bytesAdded, uint32_t bytesRejected
                      , OnPassThroughPayload&& passFunctor
                      , OnRejectedPayload&& rejFunctor )
  {

    /*if (outSeq + dupData.length() <= reversePathContext().maxAckRecv_)
    {
      LOG("Skipping dup bz " <<  reversePathContext().maxAckRecv_ << " >=  " << outSeq + dupData.length());
      return ;
    }*/

    const auto& [outSeq, p] = calculateSeqNumForDup(recvSeq, dupData, bytesAdded, bytesRejected);
    const auto& [sit, fit] = p;
    LOG("Dup data " << dupData);


    uint32_t dataProcessed = 0;
    uint32_t passDataAdded = 0;
    uint32_t passDataRej = 0;

    uint32_t newSeq = outSeq;
    uint32_t tmpRecv = recvSeq;

    Intervals& intervals = this->intervals();

    for (auto it = sit ; it != fit ; ++ it  )
    {

      for (const auto& rejectedInterval : it->second)
      {
        std::string_view left;
        std::string_view right;

        if(rejectedInterval.split(tmpRecv, dupData, left, right))
        {
          LOG("Split " <<  left << ", " << right);
          if (left.size())
          {
            passFunctor(newSeq + dataProcessed + passDataAdded - passDataRej, left);
            dataProcessed += left.size();
          }

          if(rejectedInterval .injectionType() == ChangeDetails::Added)
          {
              passFunctor(rejectedInterval.outSeq() , rejectedInterval.payload());
              passDataAdded += rejectedInterval.payload().size();
          }
          else
          {
              rejFunctor(rejectedInterval.rejSeq(), rejectedInterval.rejPayload());
              passDataRej += rejectedInterval.payload().size();
              dataProcessed += rejectedInterval.payload().size();
              right = right.substr(rejectedInterval.payload().size());
          }

          dupData = right;
          tmpRecv += dataProcessed;
          LOG(tmpRecv << " " << dataProcessed);

        }
        else
        {
          break;
        }
      }
    }

    LOG ("Last passthrough data:" << dataProcessed  << " , len " <<  dupData.length());
    if (dupData.length() )
    {
      LOG(intervals);
      passFunctor(newSeq + dataProcessed + passDataAdded - passDataRej, dupData);

      dataProcessed += dupData.size();
    }

  }

  bool hasPendingInjections() const;
  void clear();

  void addInjection(uint32_t seq, std::string_view payload, uint32_t inSeq);
  void addRejection(uint32_t seq, std::string_view payload, uint32_t rejSeq, std::string_view rejP, bool dontChange , uint32_t origSeq);

private:
  std::pair<uint32_t, IntervalRange> calculateSeqNumForDup( uint32_t inSeq
                                , std::string_view payload
                                , uint32_t bytesAlreadyAdded
                                , uint32_t bytesAlreadyRejected);
private:
  Intervals  intervals_;

};
