#include <InjectionsAndRejections.h>
#include <Utils.h>

  ChangeDetails::ChangeDetails()
  {
      outSeq_= 0;
      inSeq_ = 0;
  }

  bool ChangeDetails::operator == (const ChangeDetails& interval) const
  {
      return outSeq_ == interval.outSeq_;
  }

  bool ChangeDetails::operator < (const ChangeDetails& interval) const
  {
      return outSeq_ < interval.outSeq_;
  }

  ChangeDetails::ChangeDetails(    InjectionType injectionType
                  , uint32_t seq
                  , std::string_view payload
                  , uint32_t rejSeq
                  , std::string_view rejectPayload
                  , bool dontChange
                  , uint32_t origSeq)
  {
    outSeq_ = seq;
    payload_ = payload;
    injectionType_ = injectionType;
    rejSeq_ =  rejSeq;
    rejPayload_ = rejectPayload;
    inSeq_ = origSeq;
    dontChange_ = dontChange;
  }

  std::ostream& operator << (std::ostream& ostrm, ChangeDetails inj)
  {
      ostrm << "{"
      << "InSeq : " << inj.inSeq_ << ","
      << "OutSeq : " << inj.outSeq_ << ","
      << "Payload : " << inj.payload_ << ","
      << "RejPayload : " << inj.rejPayload_ << ","
      << "RejSeq : " << inj.rejSeq_ << ","
      << "Type : " << inj.injectionType_
      << "}";

      return ostrm;
  }

  ChangeDetails::InjectionType ChangeDetails::injectionType () const
  {
      return injectionType_;
  }

  uint32_t ChangeDetails::expectedAck() const
  {
    if (injectionType_ == InjectionType::Reject)
    {
      return outSeq_;
    }

    return outSeq_ + payloadSize();
  }

  uint32_t ChangeDetails::outSeq() const
  {
    return outSeq_;
  }

  uint32_t ChangeDetails::rejSeq() const
  {
    return rejSeq_;
  }

  uint32_t ChangeDetails::inSeq() const
  {
      return inSeq_;
  }

  std::string_view  ChangeDetails::rejPayload() const
  {
    return rejPayload_;
  }

  bool ChangeDetails::dontChange() const
  {
    return dontChange_;
  }

  std::string_view ChangeDetails::payload() const
  {
    return payload_;
  }

  uint32_t ChangeDetails::payloadSize() const
  {
    return payload_.size();
  }

  bool ChangeDetails::split(uint32_t seq, std::string_view payload, std::string_view& left, std::string_view& right) const
  {
    if (inSeq_ <= seq + payload.size() && inSeq_ >=  seq)
    {
      left = payload.substr(0, inSeq_ - seq);
      right = payload.substr(inSeq_ - seq);
      return true;
    }
    return false;
  }


PendingInjections::PendingInjections()
{

}

PendingInjections::Intervals& PendingInjections::intervals()
{
  return intervals_;
}

void PendingInjections::processReceivedAck(uint32_t ackNum, uint32_t& bytesAcked, uint32_t& bytesRejectedAcked_)
{
  if (!hasPendingInjections())
  {
    return;
  }

  auto it = intervals_.begin();
  bool done = false;

  for (; it != intervals_.end() && !done;  ++ it)
  {
      auto& [i_data, elements] = *it;

      for (auto eit = elements.begin(); eit !=  elements.end(); eit = elements.erase(eit))
      {
            const auto& element = *eit;

            if (ackNum < element.expectedAck())
            {
              LOG("!!!!Leaving pending requests " << ackNum  << " < " << element.expectedAck());
              done = true;
              break;
            }

            if (element.injectionType() == ChangeDetails::Added)
            {
              LOG("!!!!Bytes added acked changed by " << element.payloadSize() );
              bytesAcked  += element.payloadSize();

            }
            else if (element.injectionType() == ChangeDetails::Reject)
            {
              if ( element.dontChange() == false )
              {
                bytesRejectedAcked_ += element.payloadSize();
                LOG("!!!Bytes rejected acked changed by " << element.payloadSize() );
              }
              else
              {
                LOG("!!!No change");
              }
            }
            else
            {
               LOG("!!! Delayed was removed ");
            }

      }

      if (done)
      {
        break;
      }
  }
  intervals_.erase(intervals_.begin(), it);
}

std::pair<uint32_t, PendingInjections::IntervalRange > PendingInjections::calculateSeqNumForDup( uint32_t inSeq
                              , std::string_view payload
                              , uint32_t bytesAlreadyAdded
                              , uint32_t bytesAlreadyRejected)
{
    uint32_t outSeq = inSeq + bytesAlreadyAdded - bytesAlreadyRejected;
    uint32_t outSeqOrig  = outSeq;
    LOG("SeqNum if evt is fine" << outSeq);

    LOG("InSeq " << inSeq);
    LOG("BytesAdded " << bytesAlreadyAdded << ", Bytes rejected " << bytesAlreadyRejected );

    auto fit = intervals_.find(inSeq + payload.size());
    auto sit = intervals_.begin();

    LOG(intervals_);
    for (auto it = intervals_.begin(); it != fit; ++it )
    {
        const auto& [i, elements] = *it;

        for (auto& element : elements)
        {
          if (element.inSeq() < inSeq)
          {
            sit++;
            continue;
          }

          if (element.injectionType() == ChangeDetails::Reject)
          {
            outSeq += element.payloadSize();
            LOG("Out seq change + " << element.payloadSize());

          }
          else if (element.injectionType() == ChangeDetails::Added)
          {
            LOG("Out seq change - " << element.payloadSize() );
            outSeq -= element.payloadSize();
          }
          else
          {
            LOG("Delayed Was calculated");
          }
        }
    }

    return std::make_pair(outSeq,  std::make_pair(sit, fit));
}

bool PendingInjections::hasPendingInjections() const
{
  return !this->intervals_.empty();
}

void PendingInjections::clear()
{
    intervals_.clear();
}

void PendingInjections::addInjection(uint32_t seq, std::string_view payload, uint32_t inSeq)
{
  ChangeDetails inj (ChangeDetails::Added, seq, payload, 0, std::string_view(), false, inSeq);
  std::set<ChangeDetails> v = {inj};
  LOG("Before " << intervals_);
  intervals_ += std::make_pair(interval<uint32_t>::open(inSeq, inSeq + payload.size()), v);
  LOG("Added injection sequence  " << seq << ", with size " << payload.size());
  LOG("After " << intervals_);
}


void PendingInjections::addRejection(uint32_t seq, std::string_view payload, uint32_t rejSeq, std::string_view rejP, bool dontChange , uint32_t origSeq)
{
  LOG("Added rejection sequence " << seq << ", " << payload.size());
  ChangeDetails inj(ChangeDetails::Reject, seq, payload, rejSeq, rejP, dontChange, origSeq);
  std::set<ChangeDetails> v = {inj};
  LOG("Before " << intervals_);
  intervals_ += std::make_pair(interval<uint32_t>::open(origSeq, origSeq + payload.size()), v);
  LOG("After " << intervals_);
}
