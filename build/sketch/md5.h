#line 1 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\master\\master\\md5.h"
// md5_utils.h
#ifndef MD5_UTILS_H
#define MD5_UTILS_H

#include <Arduino.h>
#include <MD5Builder.h>

/**
 * Tính MD5 hash từ một chuỗi String
 * Trả về chuỗi hex 32 ký tự lowercase (giống hashlib.md5().hexdigest() trong Python)
 */
String calculateMD5(const String &input);

#endif // MD5_UTILS_H