#pragma once

#include <array>

template<typename Validator>
class ValidatorBase
{
public:
  enum class Status : char
  {
    Valid = 'V'
  , Invalid  = 'I'
  , PartialPayload = 'P'
  , PayloadAdded  = 'A'
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
      isCompleteMessage = validator_.isCompleteMessage_(payload);
    }
    else
    {
      isCompleteMessage = validator_.isCompleteMessage_(storedPayload_, payload);
    }

    if (!isCompleteMessage)
    {
      std::memmove(buffer_.data() + storedPayload_.size() , payload.data(), payload.size());
      storedPayload_ = std::string_view(buffer_.data(), storedPayload_.size() + payload.size());
      lenProcessed = 0;

      LOG("PartialMessage " << storedPayload_ );

      return Status::PartialPayload;
    }

    if (!storedPayload_.size())
    {
        return validator_.validate_(payload, lenProcessed, outPayload);
    }

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

protected:
  std::string_view storedPayload_;
  Validator& validator_;
  std::array<char, 2048> buffer_;
};
