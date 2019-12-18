#pragma once

#include <Utils.h>
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
                    , uint32_t rejSeq
                    , std::string_view rejectPayload
                    , bool dontActivate
                    , uint32_t origSeq);
public:
    InjectionType injectionType () const;

    uint32_t expectedAck() const;
    uint32_t outSeq() const;
    uint32_t inSeq() const;
    uint32_t rejSeq() const;
    uint32_t payloadSize() const;

    bool dontActivate() const;
    std::string_view  rejPayload() const;
    std::string_view  payload() const;
public:
    // split interval [origSeq, origSeq + payload.size]
    bool split( uint32_t origSeq, std::string_view payload
              , /*out*/std::string_view& left
              , /*out*/std::string_view& right) const;

    friend std::ostream& operator << (std::ostream& ostrm, const ChangeDetails& inj);

private:
    InjectionType injectionType_;
    bool dontActivate_;

private:
    uint32_t outSeq_;
    uint32_t inSeq_;
    uint32_t rejSeq_;
    std::string_view payload_;
    std::string_view rejPayload_;
};

class PendingInjections final
{
public:
  using  Intervals     = std::deque<ChangeDetails>;
  using  IntervalRange = std::pair<Intervals::const_iterator, Intervals::const_iterator>;

public:
  PendingInjections();
  Intervals& intervals();

public:
  void processReceivedAck(uint32_t ackNum, uint32_t& bytesAcked, uint32_t& bytesRejectedAcked);

  bool hasPendingInjections() const;

  void clear();

  void addInjection(uint32_t activationSeq, std::string_view payload, uint32_t origSeq);

  void addRejection( uint32_t activationSeq
                   , std::string_view payload
                   , uint32_t rejectSeq
                   , std::string_view rejectPayload
                   , bool alreadyActivated
                   , uint32_t origSeq);

  template <typename OnPassThroughPayload, typename OnRejectedPayload>
  void processDupPayload(uint32_t recvSeq, std::string_view dupData
                       , uint32_t bytesAdded, uint32_t bytesRejected
                       , OnPassThroughPayload&& passFunctor
                       , OnRejectedPayload&& rejFunctor);


private:
  std::pair<uint32_t, IntervalRange> calculateSeqNumForDup(uint32_t inSeq
                                , std::string_view payload
                                , uint32_t bytesAlreadyAdded
                                , uint32_t bytesAlreadyRejected);
private:
  Intervals intervals_;
};

#include <InjectionsAndRejections.hpp>
