#ifndef IMSQ_EXCEPTION_H
#define IMSQ_EXCEPTION_H

#include <exception>
#include <string>

class ImageParseException final : public std::exception {
  public:
    explicit ImageParseException(const std::string&& message)
        : m_msg(message) {}

    [[nodiscard]] const char*
    what() const noexcept override {
        return m_msg.c_str();
    }

  private:
    std::string m_msg;
};

#endif  // IMSQ_EXCEPTION_H