#ifndef GIF_ENC_EXCEPTION_H
#define GIF_ENC_EXCEPTION_H

#include <exception>
#include <string>

namespace GIFEnc {
class GIFEncodeException final : public std::exception {
    const std::string m_msg;

   public:
    explicit GIFEncodeException(const std::string&& msg) : m_msg(msg) {}

    [[nodiscard]] const char*
    what() const noexcept override {
        return m_msg.c_str();
    }
};

};  // namespace GIFEnc

#endif  // GIF_ENC_EXCEPTION_H