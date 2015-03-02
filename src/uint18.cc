
#include "uint18.h"

#define INCLUDENAME "opdefn.cci"
#include "foreach.i"
#undef INCLUDENAME


void uint18::setm(int x, int y)
{
    if (currentMode != MaskX)
      this->sety(y);
    if (currentMode != MaskY)
      this->setx(x);
}

unsigned int uint18::getm() const
{
    unsigned int ret = 0;
    if (currentMode != MaskX)
      ret |= this->gety() << 9;
    if (currentMode != MaskY)
      ret |= this->getx();
    return ret;
}

uint18 operator ~ (uint18 x)
{
    x ^= uint18(0777,0777);
    return x;
}

uint18 & uint18::operator <<= (unsigned int n)
{
    unsigned int x = (this->value & 0777) << n;
    unsigned int y = (this->value & 0777000) << n;
    x &= 0777;
    y &= 0777000;
    switch (currentMode) {
        case MaskVector:
            this->value = y | x;
            break;
        case MaskX:
            this->value = (this->value & 0777000) | x;
            break;
        case MaskY:
            this->value = y | (this->value & 0777);
            break;
        case MaskScalar:
            this->value = (this->value << n) & 0777777;
            break;
    }
    return *this;
}

uint18 & uint18::operator >>= (unsigned int n)
{
    unsigned int x = (this->value & 0777) >> n;
    unsigned int y = (this->value & 0777000) >> n;
    x &= 0777;
    y &= 0777000;
    switch (currentMode) {
        case MaskVector:
            this->value = y | x;
            break;
        case MaskX:
            this->value = (this->value & 0777000) | x;
            break;
        case MaskY:
            this->value = y | (this->value & 0777);
            break;
        case MaskScalar:
            this->value = (this->value >> n) & 0777777;
            break;
    }
    return *this;
}

uint18 operator << (uint18 x, unsigned int n)
{
    x <<= n;
    return x;
}

uint18 operator >> (uint18 x, unsigned int n)
{
    x >>= n;
    return x;
}
