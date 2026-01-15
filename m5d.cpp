#include "md5.h"

String calculateMD5(const String &input)
{
    MD5Builder md5;
    md5.begin();
    md5.add(input);
    md5.calculate();
    return md5.toString(); 
}