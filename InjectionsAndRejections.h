#pragma once

#include <Utils.h>
#include <boost/icl/map.hpp>
#include <boost/icl/interval_map.hpp>
#include <boost/icl/interval_map.hpp>
#include <deque>


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


public: // Interval keys
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

using namespace boost::icl; // ok no collisions expected

struct PendingInjections
{

  using  Intervals = std::deque<ChangeDetails>;
  using  IntervalRange = std::pair<Intervals::const_iterator, Intervals::const_iterator>;

  PendingInjections();

  Intervals& intervals();

  void processReceivedAck(uint32_t ackNum, uint32_t& bytesAcked, uint32_t& bytesRejectedAcked);


  template <typename OnPassThroughPayload, typename OnRejectedPayload>
  bool processDupPayload(uint32_t recvSeq, std::string_view dupData
                      , uint32_t bytesAdded, uint32_t bytesRejected
                      , OnPassThroughPayload&& passFunctor
                      , OnRejectedPayload&& rejFunctor
                      , uint32_t maxAck)
  {

    const auto& [outSeq, range] = calculateSeqNumForDup(recvSeq, dupData, bytesAdded, bytesRejected);

  /*  if (recvSeq + dupData.length() <= maxAck)
    {
      LOG("Skipping dup bz " <<  maxAck << " >=  " << outSeq + dupData.length());
      return  true;
    }*/

    const auto& [sit, fit] = range;
    LOG("Procesing dup data : " << dupData);

    uint32_t dataProcessed = 0;
    uint32_t passDataAdded = 0;
    uint32_t passDataRej = 0;

    uint32_t newSeq = outSeq;
    uint32_t curRecvSeq = recvSeq;

    Intervals& intervals = this->intervals();

    for (auto it = sit ; it != fit ; ++ it  )
    {
        const auto& rejectedInterval = *it;

        std::string_view left;
        std::string_view right;

        LOG("Distance " << std::distance(it, fit));
        LOG("Checkig " <<  rejectedInterval);

        if(rejectedInterval .injectionType() == ChangeDetails::Added && curRecvSeq > rejectedInterval.inSeq())
        {
            passFunctor(rejectedInterval.outSeq() , rejectedInterval.payload());
            passDataAdded += rejectedInterval.payload().size();
            continue;
        }

        if(rejectedInterval.split(curRecvSeq, dupData, left, right))
        {
          LOG("Split dup data: left = " <<  left << " ,right = " << right);

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
          else if(rejectedInterval .injectionType() == ChangeDetails::Reject)
          {
              rejFunctor(rejectedInterval.rejSeq(), rejectedInterval.rejPayload());
              passDataRej += rejectedInterval.payload().size();
              dataProcessed += rejectedInterval.payload().size();
              if (right.size() >= rejectedInterval.payload().size())
              {
                right = right.substr(rejectedInterval.payload().size());
              }
              else
              {
                right = std::string_view();
              }
          }
          else
          {
            assert(0);
          }

          dupData = right;
          curRecvSeq += dataProcessed;
          LOG("Moving to seqNum  : " << curRecvSeq << "Dup data left : " << dupData );
        }
        else
        {
          break;
        }
    }

    if (dupData.length() )
    {
      LOG("Dup data left : " << dupData << "Passing through...");
      passFunctor(newSeq + dataProcessed + passDataAdded - passDataRej, dupData);
      dataProcessed += dupData.size();
    }
    return false;
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
  Intervals intervals_;
};
