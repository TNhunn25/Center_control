#ifndef SERIAL_CONTROL_H
#define SERIAL_CONTROL_H

#include <cstdint>
#include <functional>
#include <sstream>
#include <string>

namespace serial_control {

inline std::string jsonCompact(const std::string &value)
{
    return value;
}

inline std::string buildIoData(bool out1, bool out2, bool out3, bool out4)
{
    std::ostringstream out;
    out << "{\"out1\":" << (out1 ? 1 : 0)
        << ",\"out2\":" << (out2 ? 1 : 0)
        << ",\"out3\":" << (out3 ? 1 : 0)
        << ",\"out4\":" << (out4 ? 1 : 0)
        << "}";
    return out.str();
}

inline std::string buildEmptyData()
{
    return "{}";
}

inline std::string buildCommandJson(
    int idDes,
    int opcode,
    const std::string &dataJson,
    std::uint32_t unixTime,
    const std::string &secretKey,
    const std::function<std::string(const std::string &)> &md5Fn)
{
    std::string combined = std::to_string(idDes) + std::to_string(opcode) + dataJson +
                           std::to_string(unixTime) + secretKey;
    std::string auth = md5Fn(combined);

    std::ostringstream out;
    out << "{\"id_des\":" << idDes << ",\"opcode\":" << opcode
        << ",\"data\":" << jsonCompact(dataJson)
        << ",\"time\":" << unixTime
        << ",\"auth\":\"" << auth << "\"}";
    return out.str();
}

inline std::string buildIoCommand(
    int idDes,
    bool out1,
    bool out2,
    bool out3,
    bool out4,
    std::uint32_t unixTime,
    const std::string &secretKey,
    const std::function<std::string(const std::string &)> &md5Fn)
{
    return buildCommandJson(idDes, 2, buildIoData(out1, out2, out3, out4), unixTime, secretKey, md5Fn);
}

inline std::string buildGetInfoCommand(
    int idDes,
    std::uint32_t unixTime,
    const std::string &secretKey,
    const std::function<std::string(const std::string &)> &md5Fn)
{
    return buildCommandJson(idDes, 3, buildEmptyData(), unixTime, secretKey, md5Fn);
}

} // namespace serial_control

#endif