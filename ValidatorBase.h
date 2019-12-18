#pragma once

#include <array>

template<typename Validator>
class ValidatorBase
{
public:
  enum class Status : char
  {
    Valid           = 'V'
  , Invalid         = 'I'
  , PartialPayload  = 'P'
  , PayloadAdded    = 'A'
  };

  ValidatorBase (Validator& validator)
  : validator_(validator)
  {
  }

  Status validate(std::string_view& payload, uint32_t& lenProcessed, std::string_view& outPayload)
  {
    bool isCompleteMessage = false;

    if (!storedPayload_.size())
    {
      // No partial payloads
      isCompleteMessage = validator_.isCompleteMessage_(payload);

      if (isCompleteMessage)
      {
        return validator_.validate_(payload, lenProcessed, outPayload);
      }
    }
    else
    {
      // Need to work with partial payloads =(
      isCompleteMessage = validator_.isCompleteMessage_(storedPayload_, payload);

      if (isCompleteMessage)
      {
        Status res = validator_.validate_(storedPayload_, payload, lenProcessed, outPayload);

        if (res == Status::Valid)
        {
          outPayload = storedPayload_;
          res = Status::PayloadAdded;
        }
        storedPayload_ = std::string_view();
        //!!! storedPayload is already marked as rejected}
        return res;
      }
    }

    //if (!isCompleteMessage)
    {
      std::memmove(buffer_.data() + storedPayload_.size() , payload.data(), payload.size());
      storedPayload_ = std::string_view(buffer_.data(), storedPayload_.size() + payload.size());
      lenProcessed = 0;
      LOG("Partial payload is " << storedPayload_ );
      return Status::PartialPayload;
    }

  }

protected:
  std::string_view storedPayload_;
  Validator& validator_;
  std::array<char, 2048> buffer_;
};
