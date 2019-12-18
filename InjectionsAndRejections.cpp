#include <InjectionsAndRejections.h>
#include <Utils.h>

  ChangeDetails::ChangeDetails()
  {
      outSeq_= 0;
      inSeq_ = 0;
      dontActivate_ = true;
  }

  ChangeDetails::ChangeDetails(InjectionType injectionType
                  , uint32_t activationOutSeq
                  , std::string_view payload
                  , uint32_t rejSeq
                  , std::string_view rejectPayload
                  , bool dontActivate
                  , uint32_t origSeq)
  {
    outSeq_ = activationOutSeq;
    payload_ = payload;
    injectionType_ = injectionType;
    rejSeq_ =  rejSeq;
    rejPayload_ = rejectPayload;
    inSeq_ = origSeq;
    dontActivate_ = dontActivate;
  }

  std::ostream& operator << (std::ostream& ostrm, const ChangeDetails& inj)
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

  bool ChangeDetails::dontActivate() const
  {
    return dontActivate_;
  }

  std::string_view ChangeDetails::payload() const
  {
    return payload_;
  }

  uint32_t ChangeDetails::payloadSize() const
  {
    return payload_.size();
  }

  bool ChangeDetails::split( uint32_t origSeq
                           , std::string_view payload
                           , std::string_view& left
                           , std::string_view& right) const
  {
    if (inSeq_ <= origSeq + payload.size() && inSeq_ >=  origSeq)
    {
      left = payload.substr(0, inSeq_ - origSeq);
      right = payload.substr(inSeq_ - origSeq);
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
  if ( !hasPendingInjections() )
  {
    return;
  }

  auto it = intervals_.begin();

  for (; !intervals_.empty(); )
  {
        const auto& element = intervals_.front();

        if (ackNum < element.expectedAck())
        {
            break;
        }

        if (element.injectionType() == ChangeDetails::Added)
        {
          LOG("Activated injection: bytes added acked changed by " << element.payloadSize() );
          bytesAcked  += element.payloadSize();
        }
        else
        //if (element.injectionType() == ChangeDetails::Reject)
        {
          if ( element.dontActivate() == false )
          {
            bytesRejectedAcked_ += element.payloadSize();
            LOG("Activated rejection: bytes rejected acked changed by " << element.payloadSize() );
          }
          else
          {
            LOG("Rejection was not activated");
          }
        }
        intervals_.pop_front();
  }
}

std::pair<uint32_t, PendingInjections::IntervalRange > PendingInjections::calculateSeqNumForDup( uint32_t inSeq
                              , std::string_view payload
                              , uint32_t bytesAlreadyAdded
                              , uint32_t bytesAlreadyRejected)
{
    uint32_t outSeq = inSeq + bytesAlreadyAdded - bytesAlreadyRejected;
    uint32_t outSeqOrig  = outSeq;

    LOG("InSeq " << inSeq);
    LOG("BytesAdded " << bytesAlreadyAdded << ", Bytes rejected " << bytesAlreadyRejected );

    auto endInSeq = inSeq + payload.size();

    auto it = intervals_.begin();
    auto sit = intervals_.begin();

    for (; it != intervals_.end(); ++it)
    {
      auto& element = *it;
      if (element.inSeq() < inSeq)
      {
        sit++;
        continue;
      }

      if (endInSeq <= element.inSeq() + element.payloadSize())
      {
        break;
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
    }

    return std::make_pair(outSeq,  std::make_pair(sit, it));
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
  intervals_.push_back(inj);
}


void PendingInjections::addRejection(uint32_t seq
                                    , std::string_view payload
                                    , uint32_t rejSeq
                                    , std::string_view rejP
                                    , bool dontChange
                                    , uint32_t origSeq)
{
  ChangeDetails rej(ChangeDetails::Reject, seq, payload, rejSeq, rejP, dontChange, origSeq);
  intervals_.push_back(rej);
}
