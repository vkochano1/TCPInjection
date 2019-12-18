#pragma once


template <typename OnPassThroughPayload, typename OnRejectedPayload>
void PendingInjections::processDupPayload(uint32_t recvSeq, std::string_view dupData
                    , uint32_t bytesAdded, uint32_t bytesRejected
                    , OnPassThroughPayload&& passFunctor
                    , OnRejectedPayload&& rejFunctor)
{

  LOG("Procesing dup data : " << dupData);

  const auto& [outSeq, range] = calculateSeqNumForDup(recvSeq, dupData, bytesAdded, bytesRejected);
  const auto& [sit, fit] = range;

  uint32_t dataProcessed = 0;
  uint32_t passDataAdded = 0;
  uint32_t passDataRej = 0;
  uint32_t newSeq = outSeq;
  uint32_t curRecvSeq = recvSeq;

  for (auto it = sit ; it != fit; ++ it  )
  {
      const auto& rejectedOrInjectedInterval = *it;

      std::string_view left;
      std::string_view right;

      LOG("Checking " <<  rejectedOrInjectedInterval);

      if(rejectedOrInjectedInterval.injectionType() == ChangeDetails::Added
         && curRecvSeq > rejectedOrInjectedInterval.inSeq())
      {
          passFunctor(rejectedOrInjectedInterval.outSeq() , rejectedOrInjectedInterval.payload());
          passDataAdded += rejectedOrInjectedInterval.payload().size();
          continue;
      }

      if(rejectedOrInjectedInterval.split(curRecvSeq, dupData, left, right))
      {
        LOG("Split dup data: left = " <<  left << ",right = " << right);

        if (left.size())
        {
          passFunctor(newSeq + dataProcessed + passDataAdded - passDataRej, left);
          dataProcessed += left.size();
        }

        if(rejectedOrInjectedInterval.injectionType() == ChangeDetails::Added)
        {
            passFunctor(rejectedOrInjectedInterval.outSeq() , rejectedOrInjectedInterval.payload());
            passDataAdded += rejectedOrInjectedInterval.payload().size();
        }
        else
        //if(rejectedOrInjectedInterval.injectionType() == ChangeDetails::Reject)
        {
            rejFunctor(rejectedOrInjectedInterval.rejSeq(), rejectedOrInjectedInterval.rejPayload());
            passDataRej += rejectedOrInjectedInterval.payload().size();
            dataProcessed += rejectedOrInjectedInterval.payload().size();
            if (right.size() >= rejectedOrInjectedInterval.payload().size())
            {
              right = right.substr(rejectedOrInjectedInterval.payload().size());
            }
            else
            {
              right = std::string_view();
            }
        }

        dupData = right;
        curRecvSeq += dataProcessed;
        LOG("Moving to seqNum  : " << curRecvSeq << " Dup data left : " << dupData );
      }
      else
      {
        break;
      }
  }

  if (dupData.length() )
  {
    LOG("Dup data left : " << dupData << " Passing through...");
    passFunctor(newSeq + dataProcessed + passDataAdded - passDataRej, dupData);
    dataProcessed += dupData.size();
  }

}
