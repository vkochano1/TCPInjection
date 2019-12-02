#pragma once
#include <Utils.h>

struct InjectionDetails
{
    enum InjectionType
    {
      Added,
      Reject
    };

    InjectionDetails()
    {
        mySequence_= 0;
    }

    InjectionDetails( InjectionType injectionType
                    , uint32_t seq
                   , std::string_view payload
                   , uint32_t rejSeq = 0
                   , std::string_view rejectPayload = std::string_view()
                    , bool dontChange = false
                  , uint32_t origSeq = 0)
    {
      mySequence_ = seq;
      payload_ = payload;
      injectionType_ = injectionType;
      dontChange_ = dontChange;
      rejSeq_ =  rejSeq;
      rejPayload_ = rejectPayload;
      origSeq_ = origSeq;
    }

    friend std::ostream& operator << (std::ostream& ostrm, InjectionDetails inj)
    {
        ostrm << "MySequence : " << inj.mySequence_ << ","
        << "Payload : " << inj.payload_ << ","
        << "rejPayload : " << inj.rejPayload_ << ","
        << "origSeq : " << inj.origSeq_ << ","
        << "rejSeq : " << inj.rejSeq_ << ","
        << "Type : " << inj.injectionType_;

        return ostrm;
    }

    InjectionType injectionType () const
    {
        return injectionType_;
    }

    uint32_t expectedAck() const
    {
      if (injectionType_ == InjectionType::Reject)
      {
        return mySequence_;
      }
      return mySequence_ + payloadSize();
    }

    uint32_t mySequence() const
    {
      return mySequence_;
    }

    uint32_t rejSeq() const
    {
      return rejSeq_;
    }

    uint32_t myOrigSequence() const
    {
        return origSeq_;
    }

    std::string_view  rejPayload() const
    {
      return rejPayload_;
    }

    bool dontChange() const
    {
      return dontChange_;
    }

    std::string_view payload() const
    {
      return payload_;
    }

    uint32_t payloadSize() const
    {
      return payload_.size();
    }
private:
    InjectionType injectionType_;
    uint32_t mySequence_;
    uint32_t rejSeq_;
    uint32_t origSeq_;
    bool dontChange_;
    std::string_view payload_;
    std::string_view rejPayload_;
};


struct PendingInjections : protected std::deque<InjectionDetails>
{

  void processReceivedAck(uint32_t ackNum, uint32_t& bytesAcked, uint32_t& bytesRejectedAcked_)
  {
    while (!this->empty())
    {
      const auto& element = this->front();
      if (ackNum < element.expectedAck())
      {
        LOG("Leaving pending requests " << ackNum  << " < " << element.expectedAck() << ", " << this->size());
        break;
      }

      if (element.injectionType() == InjectionDetails::Added)
      {
        LOG("Bytes added acked changed by " << element.payloadSize() );
        bytesAcked  += element.payloadSize();
      }
      else
      {
        if ( element.dontChange() == false )
        {
          bytesRejectedAcked_ += element.payloadSize();
          LOG("Bytes rejected acked changed by " << element.payloadSize() );
        }
        else
        {
          LOG("No change");
        }
      }

      this->pop_front();
    }

    LOG("Leaving pending requests");
  }

  uint32_t calculateSeqNumForDup(uint32_t inSeq, uint32_t bytesAlreadyAdded, uint32_t bytesAlreadyRejected, std::vector<InjectionDetails>& rejected)
  {
      uint32_t outSeqOrig = inSeq + bytesAlreadyAdded - bytesAlreadyRejected;
      uint32_t outSeq = inSeq + bytesAlreadyAdded - bytesAlreadyRejected;

      for (auto it = this->rbegin(); it != rend(); ++it )
      {
          const auto& element = *it;

          if (element.mySequence() > outSeqOrig)
          {
            if (element.injectionType() == InjectionDetails::Added)
            {
              LOG("Out seq change -" <<element.payloadSize() );
              outSeq -= element.payloadSize();
            }
            else
            {
              LOG("REJ Element " << element);
              outSeq += element.payloadSize();
              LOG("Out seq change +" <<element.payloadSize() );
              rejected.insert(rejected.begin(), element);
            }
          }
          else
          {
            LOG(element.mySequence() << " <=  "<< outSeqOrig);
            break;
          }
      }
      LOG("Leaving dup out seq calc " << outSeq);
      return outSeq;
  }

  bool hasPendingInjections() const
  {
    return !this->empty();
  }

  void clear()
  {
      while(!this->empty())
      {
         this->pop_front();
      }
  }

  void addInjection(uint32_t seq, std::string_view payload)
  {
    LOG("Added seq " << seq << ", " << payload.size());

    this->push_back(InjectionDetails(InjectionDetails::Added, seq, payload));
  }

  void addRejection(uint32_t seq, std::string_view payload, uint32_t rejSeq, std::string_view rejP, bool dontChange , uint32_t origSeq)
  {
    LOG("Rej seq " << seq << ", " << payload.size());

    this->push_back(InjectionDetails(InjectionDetails::Reject, seq, payload, rejSeq, rejP, dontChange, origSeq));
  }
};
