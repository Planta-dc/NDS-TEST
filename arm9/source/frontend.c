// arm9/source/frontend.c
// DSi exploit launch chain handler - unchanged from SSEQPlayer by CaitSith2/Rocket Robz
// Supports: loadfile.dat (HiyaCFW), plgargs.dat (TWiLight Menu++), moonshl2/extlink.dat
#include <nds.h>
#include <stdio.h>
#include <unistd.h>

size_t ucs2tombs(unsigned char* dst, const unsigned short* src, size_t len) {
    size_t i=0,j=0;
    for (;src[i];i++){
        if(src[i] <= 0x007f){
            if(!dst)j++;else{
                if(len-j<2)break;
                dst[j++] = ((src[i] & 0x007f)      );
            }
        }else if(src[i] <= 0x07ff){
            if(!dst)j+=2;else{
                if(len-j<3)break;
                dst[j++] = ((src[i] & 0x07c0) >>  6) | 0xc0;
                dst[j++] = ((src[i] & 0x003f)      ) | 0x80;
            }
        }else if((src[i] & 0xdc00) == 0xd800 && (src[i+1] & 0xdc00) == 0xdc00){
            if(!dst)j+=4;else{
                unsigned short z = (src[i]&0x3ff)+0x40;
                if(len-j<5)break;
                dst[j++] = ((z      & 0x0300) >>  8) | 0xf0;
                dst[j++] = ((z      & 0x00fc) >>  2) | 0x80;
                dst[j++] = ((z      & 0x0003) <<  4)
                          | ((src[i+1] & 0x03c0) >>  6) | 0x80;
                dst[j++] = ((src[i+1] & 0x003f)      ) | 0x80;
            }i++;
        }else{
            if(!dst)j+=3;else{
                if(len-j<4)break;
                dst[j++] = ((src[i] & 0xf000) >> 12) | 0xe0;
                dst[j++] = ((src[i] & 0x0fc0) >>  6) | 0x80;
                dst[j++] = ((src[i] & 0x003f)      ) | 0x80;
            }
        }
    }
    if(dst)dst[j]=0;
    return j;
}

static u8 buf[768];

typedef u16 UnicodeChar;
#define ExtLinkBody_MaxLength (256)
#define ExtLinkBody_ID (0x30545845) // EXT0
typedef struct {
    u32 ID,dummy1,dummy2,dummy3;
    char DataFullPathFilenameAlias[ExtLinkBody_MaxLength];
    char DataPathAlias[ExtLinkBody_MaxLength];
    char DataFilenameAlias[ExtLinkBody_MaxLength];
    char NDSFullPathFilenameAlias[ExtLinkBody_MaxLength];
    char NDSPathAlias[ExtLinkBody_MaxLength];
    char NDSFilenameAlias[ExtLinkBody_MaxLength];
    UnicodeChar DataFullPathFilenameUnicode[ExtLinkBody_MaxLength];
    UnicodeChar DataPathUnicode[ExtLinkBody_MaxLength];
    UnicodeChar DataFilenameUnicode[ExtLinkBody_MaxLength];
    UnicodeChar NDSFullPathFilenameUnicode[ExtLinkBody_MaxLength];
    UnicodeChar NDSPathUnicode[ExtLinkBody_MaxLength];
    UnicodeChar NDSFilenameUnicode[ExtLinkBody_MaxLength];
} TExtLinkBody;

static char* myfgets(char *buf, int n, FILE *fp) {
    char *ret = fgets(buf, n, fp);
    if (!ret) return NULL;
    if (strlen(buf) && buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = 0;
    if (strlen(buf) && buf[strlen(buf)-1] == '\r') buf[strlen(buf)-1] = 0;
    return ret;
}

static void SplitItemFromFullPathAlias(const char *pFullPathAlias, char *pPathAlias, char *pFilenameAlias) {
    u32 SplitPos = 0;
    u32 idx = 0;
    while (1) {
        char uc = pFullPathAlias[idx];
        if (uc == 0) break;
        if (uc == '/') SplitPos = idx + 1;
        idx++;
    }
    if (pPathAlias) {
        if (SplitPos <= 1) { pPathAlias[0]='/'; pPathAlias[1]=0; }
        else {
            for (u32 i = 0; i < SplitPos - 1; i++)
                pPathAlias[i] = pFullPathAlias[i];
            pPathAlias[SplitPos-1] = 0;
        }
    }
    if (pFilenameAlias) strcpy(pFilenameAlias, &pFullPathAlias[SplitPos]);
}

static bool _readFrontend(char *target) {
    FILE *f = fopen("/loadfile.dat", "rb");
    if (f) {
        int i = 0;
        myfgets((char*)buf, 768, f);
        fclose(f);
        unlink("/loadfile.dat");
        if (!memcmp((char*)buf+i, "//",  2)) i += 1;
        if (!memcmp((char*)buf+i, "/./", 3)) i += 2;
        strcpy(target, (char*)buf+i);
        // Accept .mp3 files passed via loadfile.dat too
        return true;
    }
    f = fopen("/plgargs.dat", "rb");
    if (f) {
        int i = 0;
        myfgets((char*)buf, 768, f);
        myfgets((char*)buf, 768, f); // second line is the file path
        fclose(f);
        unlink("/plgargs.dat");
        strcpy(target, (char*)buf+i);
        return true;
    }
    f = fopen("/moonshl2/extlink.dat", "r+b");
    if (f) {
        TExtLinkBody extlink;
        memset(&extlink, 0, sizeof(TExtLinkBody));
        fread(&extlink, 1, sizeof(TExtLinkBody), f);
        if (extlink.ID != ExtLinkBody_ID) { fclose(f); return false; }
        ucs2tombs((unsigned char*)target, extlink.DataFullPathFilenameUnicode, 768);
        fseek(f, 0, SEEK_SET);
        fwrite("____", 1, 4, f);
        fclose(f);
        return true;
    }
    return false;
}

extern int argc;
extern char **argv;

bool readFrontend(char *target) {
    char dir[768];
    if (argc > 1) {
        strcpy(target, argv[1]);
    } else {
        if (!_readFrontend(target)) return false;
    }
    SplitItemFromFullPathAlias(target, dir, NULL);
    chdir(dir);
    return true;
}
