#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void hex2asc(unsigned char *hexStr, unsigned int hexLen, char *ascStr, unsigned int ascLen){
        int i;
        unsigned char uc;
        for (i=0;i<hexLen;i++){
                if (i > ascLen*2)
                        break;
                uc = hexStr[i];
                if (uc >= 0x30  && uc <= 0x39)
                        uc -= 0x30;
                else if (uc >= 0x41 && uc <= 0x46)
                        uc -= 0x37;
                else if (uc >= 0x61 && uc <= 0x66)
                        uc -= 0x57;
                else
                        break;
                if (i%2 == 1)
                        ascStr[(i-1)/2] = ascStr[(i-1)/2] | uc;
                else
                        ascStr[i/2] = uc << 4;
        }
}

void asc2hex(char *ascStr, unsigned int ascLen, unsigned char *hexStr, unsigned int hexLen){
        int i;
        unsigned char uc;
        for (i=0;i<ascLen;i++){
                if (i*2 >= hexLen) break;
                uc = (ascStr[i] & 0xF0)  >> 4;
                if (uc < 0x0A) 
                        uc += 0x30;
                else 
                        uc+=0x37;
                hexStr[i*2] = uc;
                uc = ascStr[i] & 0x0F;
                 if (uc < 0x0A) 
                        uc += 0x30;
                else 
                        uc += 0x37;
                if (i*2+1 >= hexLen) break;
                hexStr[i*2+1] = uc;
        }
}

#ifdef _MAIN_
int main(){
        unsigned char hexStr[16+1] = { 0x00 };
        strcpy((char *)hexStr, "65");
        char ascStr[8+1] = { 0x00 };
        hex2asc(hexStr, 4, ascStr, 2);
        printf("%s\n", ascStr);
        memset(hexStr, 0x00, sizeof(hexStr));
        asc2hex(ascStr, 2, hexStr, 5);
        printf("%s\n", hexStr);
        return 0;
}
#endif