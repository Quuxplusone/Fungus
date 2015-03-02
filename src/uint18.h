
#ifndef H_UINT18
 #define H_UINT18

 #ifndef __cplusplus
  #error "uint18.h is a C++ header file!"
 #else

enum MaskingModes { MaskVector, MaskX, MaskY, MaskScalar };
extern enum MaskingModes currentMode;

class uint18 {
    unsigned int value;
  public:
    uint18(): value(0) {}
    uint18(const uint18 &r): value(r.value) {}
    explicit uint18(unsigned int x): value(x) {}
    uint18(int x, int y): value(((y & 0777) << 9) | (x & 0777)) {}
    operator unsigned int() const { return value; }
    operator bool() const { return (value != 0); }
#define INCLUDENAME "opeqprot.hi"
#include "foreach.i"
#undef INCLUDENAME
    uint18 & operator <<= (unsigned int n);
    uint18 & operator >>= (unsigned int n);
    unsigned int getx() const { return value & 0777; }
    unsigned int gety() const { return (value >> 9) & 0777; }
    unsigned int getm() const;
    void setx(unsigned int x) { value = (value & 0777000) | (x & 0777); }
    void sety(unsigned int y) { value = ((y & 0777) << 9) | (value & 0777); }
    void setm(const uint18 & r) { this->setm(r.getx(), r.gety()); }
    void setm(int x, int y);
    bool operator == (const uint18 & r) { return value == r.value; }
    bool operator != (const uint18 & r) { return value != r.value; }
};

#define INCLUDENAME "opprot.hi"
#include "foreach.i"
#undef INCLUDENAME

uint18 operator ~ (uint18 x);
uint18 operator << (uint18 x, unsigned int n);
uint18 operator >> (uint18 x, unsigned int n);

 #endif
#endif
