#ifndef XECRYPT_H
#define XECRYPT_H

#include "winnames.h"
#include <iostream>
#include <string.h>

#include "XboxInternals_global.h"

class XBOXINTERNALSSHARED_EXPORT XeCrypt
{
public:
    static void BnQw_SwapDwQwLeBe(BYTE *data, DWORD length);
};

#endif // XECRYPT_H
