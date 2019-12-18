#pragma once

#include <ValidatorBase.h>

class AlwaysValid : public ValidatorBase<AlwaysValid>
{
public:
  AlwaysValid() : ValidatorBase<AlwaysValid>(*this)
  {
  }

  Status validate_(std::string_view& payload
                  , uint32_t& processed
                  , std::string_view& outPayload)
  {
    processed = payload.size();
    return Status::Valid;
  }

  Status validate_(std::string_view& storedPayload
                  , std::string_view& payload
                  , uint32_t& processed
                  , std::string_view& outPayload)
  {
    processed = payload.size();
    return Status::Valid;
  }

  constexpr bool isCompleteMessage_(std::string_view& storedPayload
                                  , std::string_view& payload) const
  {
     return true;
  }

  constexpr bool isCompleteMessage_(std::string_view& payload) const
  {
     return true;
  }
};

struct SomeReject : public ValidatorBase<SomeReject>
{
  SomeReject() : ValidatorBase<SomeReject>(*this)
  {
    step_             = 1;
    rejectAt_         = 31;
    passBytesAtReject_ = 5;
    addPayloadAt_     = 71;
    partialPayloadAt_ = 39;
  }

  constexpr bool isCompleteMessage_(std::string_view& storedPayload
                                  , std::string_view& payload) const
  {
     return true;
  }

  constexpr bool isCompleteMessage_(std::string_view& payload) const
  {
     if (step_ % partialPayloadAt_ == 0)
     {
       ++step_;
       return false;
     }
     return true;
  }

  Status validate_(std::string_view& payload
                  , uint32_t& processed
                  , std::string_view& outPayload)
  {
    ++step_;

    if (step_ % rejectAt_ == 0)
    {
      processed = payload.size() - passBytesAtReject_;
      outPayload = "REJECTED";
     return Status::Invalid;
    }
    else if (step_ % addPayloadAt_ == 0)
    {
      processed = 0;
      outPayload = "ADDED";
      return Status::PayloadAdded;
    }

    processed = payload.size();
    return Status::Valid;
  }

  Status  validate_(  std::string_view& storedPayload
                    , std::string_view& payload
                    , uint32_t& processed
                    , std::string_view& outPayload)
  {
    ++step_;

    if (step_ % rejectAt_ == 0)
    {
      processed = payload.size() - passBytesAtReject_;
      outPayload = "R_E_J_E_C_T";
      return Status::Invalid;
    }
    else if (step_ % addPayloadAt_ == 0)
    {
      processed = 0;
      outPayload = "A_D_D_E_D";
      return Status::PayloadAdded;
    }

    return Status::Valid;
  }

private:
  mutable size_t step_;
  size_t rejectAt_;
  size_t passBytesAtReject_;
  size_t addPayloadAt_;
  size_t partialPayloadAt_;
};
