// auth_md5.h
#ifndef AUTH_MD5_H
#define AUTH_MD5_H

#include <MD5Builder.h>
#include <Arduino.h>

class AuthMD5
{
public:
     String calculate(
        int node_id,
        uint16_t duration_fan,
        uint16_t duration_device,
        uint16_t duration_off,
        bool out1, bool out2, bool out3, bool out4,
        uint32_t unix_time,
        const String &secretKey)
    {
        MD5Builder md5;
        md5.begin();

        // Thứ tự PHẢI TRÙNG KHỚP chính xác với Python
        md5.add(String(node_id));
        md5.add(String(duration_fan));
        md5.add(String(duration_device));
        md5.add(String(duration_off));
        md5.add(out1 ? "true" : "false");
        md5.add(out2 ? "true" : "false");
        md5.add(out3 ? "true" : "false");
        md5.add(out4 ? "true" : "false");
        md5.add(String(unix_time));
        md5.add(secretKey);

        md5.calculate();
        return md5.toString(); // 32 ký tự hex lowercase
    }

     bool verify(
        const String &receivedHash,
        int node_id,
        uint16_t duration_fan,
        uint16_t duration_device,
        uint16_t duration_off,
        bool out1, bool out2, bool out3, bool out4,
        uint32_t unix_time,
        const String &secretKey)
    {
        String expected = calculate(node_id, duration_fan, duration_device, duration_off,
                                    out1, out2, out3, out4, unix_time, secretKey);
        return expected.equalsIgnoreCase(receivedHash);
    }
};
 
#endif