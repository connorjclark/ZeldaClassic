//--------------------------------------------------------
//  Zelda Classic
//  by Jeremy Craner, 1999-2000
//
//  zsys.cc
//
//  System functions, etc.
//
//--------------------------------------------------------

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "base/zc_alleg.h"
#include <allegro/internal/aintern.h>
#include <string>
#include <sstream>
#include "base/util.h"

using namespace util;
using std::string;
using std::istringstream;
using std::getline;

#include "zsyssimple.h"
#include "base/zdefs.h"
#include "base/zsys.h"
#include "jwin.h"
#include "zconsole/ConsoleLogger.h"

#ifdef _MSC_VER
#define stricmp _stricmp
#endif

extern volatile int32_t myvsync;
extern bool update_hw_pal;
void update_hw_screen(bool force);

CConsoleLoggerEx zscript_coloured_console;
extern bool is_zquest();

char *time_str_long(dword time)
{
    static char s[16];
    
    dword decs = (time%60)*100/60;
    dword secs = (time/60)%60;
    dword mins = (time/3600)%60;
    dword hours = time/216000;
    
    sprintf(s,"%u:%02u:%02u.%02u",hours,mins,secs,decs);
    return s;
}

char *time_str_med(dword time)
{
    static char s[16];
    
    dword secs = (time/60)%60;
    dword mins = (time/3600)%60;
    dword hours = time/216000;
    
    sprintf(s,"%u:%02u:%02u",hours,mins,secs);
    return s;
}

char *time_str_short(dword time)
{
    static char s[16];
    
    dword mins = (time/3600)%60;
    dword hours = time/216000;
    
    sprintf(s,"%u:%02u",hours,mins);
    return s;
}

char *time_str_short2(dword time)
{
    static char s[16];
    
    dword mins = (time/3600)%60;
    dword hours = time/216000;
    
    sprintf(s,"%02u%s%02u",hours,(time%60)<30?":":";",mins);
    return s;
}

void extract_name(char const* path,char *name,int32_t type)
{
    int32_t l=(int32_t)strlen(path);
    int32_t i=l;
    
    while(i>0 && path[i-1]!='/' && path[i-1]!='\\')
        --i;
        
    int32_t n=0;
    
    if(type==FILENAME8__)
    {
        while(i<l && n<8 && path[i]!='.')
            name[n++]=path[i++];
    }
    else if(type==FILENAME8_3)
    {
        while(i<l && n<12)
            name[n++]=path[i++];
    }
    else
    {
        while(i<l)
            name[n++]=path[i++];
    }
    
    name[n]=0;
}

void temp_name(char temporaryname[])
{
    // TODO: remove temp_name, use std::tmpnam() directly
    std::tmpnam(temporaryname);
}

int32_t bound(int32_t &x,int32_t low,int32_t high)
{
    if(x<low) x=low;
    
    if(x>high) x=high;
    
    return x;
}

char *snapshotformat_str[ssfmtMAX][2]=
{
    { (char *)"BMP", (char *)"bmp"},
    { (char *)"GIF", (char *)"gif"},
    { (char *)"JPG", (char *)"jpg"},
    { (char *)"PNG", (char *)"png"},
    { (char *)"PCX", (char *)"pcx"},
    { (char *)"TGA", (char *)"tga"},
};

const char *snapshotformatlist(int32_t index, int32_t *list_size)
{
    if(index>=0)
    {
        bound(index,0,ssfmtMAX);
        return snapshotformat_str[index][0];
    }
    
    *list_size=ssfmtMAX;
    return NULL;
}


//Allegro's make_relative_filename doesn't handle uppercase/lowercase too well for drive letters
char *zc_make_relative_filename(char *dest, const char *path, const char *filename, int32_t size)
{
#ifdef ALLEGRO_LINUX
    return make_relative_filename(dest, path, filename, size);
#elif defined(ALLEGRO_MACOSX)
    return make_relative_filename(dest, path, filename, size);
#else
    char *tpath = new char[size+1];
    make_relative_filename(dest, path, filename, size);
    
    if(dest[0]==0) //can't make relative path
    {
        sprintf(tpath, "%s", path);
        int32_t temp = ugetc(tpath);
    
        if(ugetat(tpath, 1) == DEVICE_SEPARATOR)
        {
            if((temp >= 'A') && (temp <= 'Z'))
            {
                usetat(tpath,0,utolower(temp));
            }
            else if((temp >= 'a') && (temp <= 'a'))
            {
                usetat(tpath,0,utoupper(temp));
            }
    
            make_relative_filename(dest, tpath, filename, size);
        }
    }
    
    delete[] tpath;
    return dest;
#endif
}



void chop_path(char *path)
{
    int32_t p = (int32_t)strlen(path);
    int32_t f = (int32_t)strlen(get_filename(path));
    
    if(f<p)
        path[p-f]=0;
}

int32_t used_switch(int32_t argc,char *argv[],const char *s)
{
    // assumes a switch won't be in argv[0]
    for(int32_t i=1; i<argc; i++)
        if(stricmp(argv[i],s)==0)
            return i;
            
    return 0;
}
//There is some hardcore constant truncation here...
#ifdef _MSC_VER
#pragma warning(disable: 4309)
#pragma warning(disable: 4310)
#endif

char zeldapwd[8]  = "N0S71M3";
char zquestpwd[8] = "S3(r37!";
char datapwd[8]   = "longtan";

#ifdef _MSC_VER
#pragma warning(default: 4309)
#endif

void set_bit(byte *bitstr,int32_t bit,byte val)
{
    bitstr += bit>>3;
    byte mask = 1 << (bit&7);
    
    if(val)
        *bitstr |= mask;
    else
        *bitstr &= ~mask;
}

int32_t get_bit(byte const *bitstr,int32_t bit)
{
    bitstr += bit>>3;
    return ((*bitstr) >> (bit&7))&1;
}

bool toggle_bit(byte *bitstr,int32_t bit)
{
	bitstr += bit>>3;
    byte mask = 1 << (bit&7);
	*bitstr ^= mask;
	return *bitstr&mask;
}


void set_bitl(int32_t bitstr,int32_t bit,byte val)
{
	if ( val ) //intel u32
	{
		bitstr |= (1<<bit);
	}
	else bitstr &= ~(1<<bit);
}

int32_t get_bitl(int32_t bitstr,int32_t bit)
{
    return bitstr&(1<<bit); //intel u32
    //bitstr += bit>>3;
    //return ((bitstr) >> (bit&7))&1;
}


[[noreturn]] void Z_error_fatal(const char *format,...)
{
    char buf[256];
    
    va_list ap;
    va_start(ap, format);
    vsprintf(buf, format, ap);
    va_end(ap);
    
#if defined(ALLEGRO_MAXOSX)
    printf("%s",buf);
#endif
#ifndef __EMSCRIPTEN__
    if (!zscript_coloured_console.valid() && !is_ci())
    {
        al_show_native_message_box(all_get_display(), "Zelda Classic: I AM ERROR", "", buf, NULL, ALLEGRO_MESSAGEBOX_ERROR);
    }
#endif
    zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_RED | CConsoleLoggerEx::COLOR_INTENSITY | 
		CConsoleLoggerEx::COLOR_BACKGROUND_BLACK), "%s", buf);
	al_trace("%s",buf);
    abort();
}

void Z_error(const char *format,...)
{
    char buf[256];
    
    va_list ap;
    va_start(ap, format);
    vsprintf(buf, format, ap);
    va_end(ap);
    
#if defined(ALLEGRO_MAXOSX)
    printf("%s",buf);
#endif
    zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_RED | CConsoleLoggerEx::COLOR_INTENSITY | 
		CConsoleLoggerEx::COLOR_BACKGROUND_BLACK), "%s", buf);
	al_trace("%s",buf);
}

void Z_message(const char *format,...)
{
    char buf[2048];
    
    va_list ap;
    va_start(ap, format);
    vsprintf(buf, format, ap);
    va_end(ap);
    
#if defined(ALLEGRO_MAXOSX)
    printf("%s",buf);
#endif
    al_trace("%s",buf);
    zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_RED | CConsoleLoggerEx::COLOR_BLUE | CConsoleLoggerEx::COLOR_INTENSITY | 
		CConsoleLoggerEx::COLOR_BACKGROUND_BLACK), "%s", buf);
}

void Z_title(const char *format,...)
{
    char buf[256];
    va_list ap;
    va_start(ap, format);
    vsprintf(buf, format, ap);
    va_end(ap);
    
    al_trace("%s\n",buf);
	
    if(zscript_coloured_console.valid())
		zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_RED | CConsoleLoggerEx::COLOR_BLUE | CConsoleLoggerEx::COLOR_INTENSITY | 
			CConsoleLoggerEx::COLOR_BACKGROUND_BLACK), "%s\n", buf);
}

static std::function<bool()> should_zprint_cb = []() { return true; };
void set_should_zprint_cb(std::function<bool()> cb)
{
    should_zprint_cb = cb;
}

void zprint(const char * const format,...)
{
	if(should_zprint_cb() || DEVLEVEL > 0)
	{
		char buf[2048];
		
		va_list ap;
		va_start(ap, format);
		vsprintf(buf, format, ap);
		va_end(ap);
		al_trace("%s",buf);
		
		zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_RED | CConsoleLoggerEx::COLOR_BLUE | CConsoleLoggerEx::COLOR_INTENSITY | 
			CConsoleLoggerEx::COLOR_BACKGROUND_BLACK),"%s",buf);
	}
}

void zprint2(const char * const format,...)
{
	char buf[8192];
	
	va_list ap;
	va_start(ap, format);
	vsprintf(buf, format, ap);
	va_end(ap);
	safe_al_trace(buf);
	
	zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_RED | CConsoleLoggerEx::COLOR_BLUE | CConsoleLoggerEx::COLOR_INTENSITY | 
		CConsoleLoggerEx::COLOR_BACKGROUND_BLACK),"%s",buf);
}

int32_t anim_3_4(int32_t clk, int32_t speed)
{
    clk /= speed;
    
    switch(clk&3)
    {
    case 0:
    case 2:
        clk = 0;
        break;
        
    case 1:
        clk = 1;
        break;
        
    case 3:
        clk = 2;
        break;
    }
    
    return clk;
}

/**********  Encryption Stuff  *****************/

//#define MASK 0x4C358938
static int32_t enc_seed = 0;
//#define MASK 0x91B2A2D1
//static int32_t enc_seed = 7351962;
static int32_t enc_mask[ENC_METHOD_MAX]= {(int32_t)0x4C358938,(int32_t)0x91B2A2D1,(int32_t)0x4A7C1B87,(int32_t)0xF93941E6,(int32_t)0xFD095E94};
static int32_t pvalue[ENC_METHOD_MAX]= {0x62E9,0x7D14,0x1A82,0x02BB,0xE09C};
static int32_t qvalue[ENC_METHOD_MAX]= {0x3619,0xA26B,0xF03C,0x7B12,0x4E8F};

static int32_t rand_007(int32_t method)
{
    int16_t BX = enc_seed >> 8;
    int16_t CX = (enc_seed & 0xFF) << 8;
    signed char AL = enc_seed >> 24;
    signed char C = AL >> 7;
    signed char D = BX >> 15;
    AL <<= 1;
    BX = (BX << 1) | C;
    CX = (CX << 1) | D;
    CX += enc_seed & 0xFFFF;
    BX += (enc_seed >> 16) + C;
    //  CX += 0x62E9;
    //  BX += 0x3619 + D;
    CX += pvalue[method];
    BX += qvalue[method] + D;
    enc_seed = (BX << 16) + CX;
    return (CX << 16) + BX;
}

void encode_007(byte *buf, dword size, dword key2, word *check1, word *check2, int32_t method)
{
    dword i;
    byte *p;
    
    *check1 = 0;
    *check2 = 0;
    
    p = buf;
    
    for(i=0; i<size; i++)
    {
        *check1 += *p;
        *check2 = (*check2 << 4) + (*check2 >> 12) + *p;
        ++p;
    }
    
    p = buf;
    enc_seed = key2;
    
    for(i=0; i<size; i+=2)
    {
        byte q = rand_007(method);
        *p ^= q;
        ++p;
        
        if(i+1 < size)
        {
            *p += q;
            ++p;
        }
    }
}

bool decode_007(byte *buf, dword size, dword key2, word check1, word check2, int32_t method)
{
    dword i;
    word c1 = 0, c2 = 0;
    byte *p;
    
    p = buf;
    enc_seed = key2;
    
    for(i=0; i<size; i+=2)
    {
        uint8_t q = rand_007(method);
        *p ^= q;
        ++p;
        
        if(i+1 < size)
        {
            *p -= q;
            ++p;
        }
    }
    
    p = buf;
    
    for(i=0; i<size; i++)
    {
        c1 += *p;
        c2 = (c2 << 4) + (c2 >> 12) + *p;
        ++p;
    }
    
    return (c1 == check1) && (c2 == check2);
}

//
// RETURNS:
//   0 - OK
//   1 - srcfile not opened
//   2 - destfile not opened
//
int32_t encode_file_007(const char *srcfile, const char *destfile, int32_t key2, const char *header, int32_t method)
{
    FILE *src, *dest;
    int32_t tog = 0, c, r=0;
    int16_t c1 = 0, c2 = 0;
    
    enc_seed = key2;
    src = fopen(srcfile, "rb");
    
    if(!src)
        return 1;
        
    dest = fopen(destfile, "wb");
    
    if(!dest)
    {
        fclose(src);
        return 2;
    }
    
    
    // write the header
    if(header)
    {
        for(c=0; header[c]; c++)
            fputc(header[c], dest);
    }
    
    // write the key, XORed with MASK
    key2 ^= enc_mask[method];
    fputc(key2>>24, dest);
    fputc((key2>>16)&255, dest);
    fputc((key2>>8)&255, dest);
    fputc(key2&255, dest);
    
    // encode the data
    while((c=fgetc(src)) != EOF)
    {
        c1 += c;
        c2 = (c2 << 4) + (c2 >> 12) + c;
        
        if(tog)
            c += r;
        else
        {
            r = rand_007(method);
            c ^= r;
        }
        
        tog ^= 1;
        
        fputc(c, dest);
    }
    
    // write the checksums
    r = rand_007(method);
    c1 ^= r;
    c2 += r;
    fputc(c1>>8, dest);
    fputc(c1&255, dest);
    fputc(c2>>8, dest);
    fputc(c2&255, dest);
    
    fclose(src);
    fclose(dest);
    return 0;
}

//
// RETURNS:
//   0 - OK
//   1 - srcfile not opened
//   2 - destfile not opened
//   3 - scrfile too small
//   4 - srcfile EOF
//   5 - checksum mismatch
//   6 - header mismatch
//
int32_t decode_file_007(const char *srcfile, const char *destfile, const char *header, int32_t method, bool packed, const char *password)
{
    FILE *normal_src=NULL, *dest=NULL;
    PACKFILE *packed_src=NULL;
    int32_t tog = 0, c, r=0, err;
    int32_t size, i;
    int16_t c1 = 0, c2 = 0, check1, check2;
    
    // open files
    size = file_size_ex_password(srcfile, password);
    
    if(size < 1)
    {
        return 1;
    }
    
    size -= 8;                                                // get actual data size, minus key and checksums
    
    if(size < 1)
    {
        return 3;
    }
    
    if(!packed)
    {
        normal_src = fopen(srcfile, "rb");
        
        if(!normal_src)
        {
            return 1;
        }
    }
    else
    {
        packed_src = pack_fopen_password(srcfile, F_READ_PACKED,password);
        
        if(errno==EDOM)
        {
            packed_src = pack_fopen_password(srcfile, F_READ,password);
        }
        
        if(!packed_src)
        {
            return 1;
        }
    }
    
    dest = fopen(destfile, "wb");
    
    if(!dest)
    {
        if(packed)
        {
            pack_fclose(packed_src);
        }
        else
        {
            fclose(normal_src);
        }
        
        return 2;
    }
    
    // read the header
    err = 4;
    
    if(header)
    {
        for(i=0; header[i]; i++)
        {
            if(packed)
            {
                if((c=pack_getc(packed_src)) == EOF)
                {
                    goto error;
                }
            }
            else
            {
                if((c=fgetc(normal_src)) == EOF)
                {
                    goto error;
                }
            }
            
            if((c&255) != header[i])
            {
                err = 6;
                goto error;
            }
            
            --size;
        }
    }
    
    // read the key
    if(packed)
    {
        if((c=pack_getc(packed_src)) == EOF)
        {
            goto error;
        }
    }
    else
    {
        if((c=fgetc(normal_src)) == EOF)
        {
            goto error;
        }
    }
    
    enc_seed = c << 24;
    
    if(packed)
    {
        if((c=pack_getc(packed_src)) == EOF)
        {
            goto error;
        }
    }
    else
    {
        if((c=fgetc(normal_src)) == EOF)
        {
            goto error;
        }
    }
    
    enc_seed += (c & 255) << 16;
    
    if(packed)
    {
        if((c=pack_getc(packed_src)) == EOF)
        {
            goto error;
        }
    }
    else
    {
        if((c=fgetc(normal_src)) == EOF)
        {
            goto error;
        }
    }
    
    enc_seed += (c & 255) << 8;
    
    if(packed)
    {
        if((c=pack_getc(packed_src)) == EOF)
        {
            goto error;
        }
    }
    else
    {
        if((c=fgetc(normal_src)) == EOF)
        {
            goto error;
        }
    }
    
    enc_seed += c & 255;
    enc_seed ^= enc_mask[method];
    
    // decode the data
    for(i=0; i<size; i++)
    {
        if(packed)
        {
            if((c=pack_getc(packed_src)) == EOF)
            {
                goto error;
            }
        }
        else
        {
            if((c=fgetc(normal_src)) == EOF)
            {
                goto error;
            }
        }
        
        if(tog)
        {
            c -= r;
        }
        else
        {
            r = rand_007(method);
            c ^= r;
        }
        
        tog ^= 1;
        
        c &= 255;
        c1 += c;
        c2 = (c2 << 4) + (c2 >> 12) + c;
        
        fputc(c, dest);
    }
    
    // read checksums
    if(packed)
    {
        if((c=pack_getc(packed_src)) == EOF)
        {
            goto error;
        }
    }
    else
    {
        if((c=fgetc(normal_src)) == EOF)
        {
            goto error;
        }
    }
    
    check1 = c << 8;
    
    if(packed)
    {
        if((c=pack_getc(packed_src)) == EOF)
        {
            goto error;
        }
    }
    else
    {
        if((c=fgetc(normal_src)) == EOF)
        {
            goto error;
        }
    }
    
    check1 += c & 255;
    
    if(packed)
    {
        if((c=pack_getc(packed_src)) == EOF)
        {
            goto error;
        }
    }
    else
    {
        if((c=fgetc(normal_src)) == EOF)
        {
            goto error;
        }
    }
    
    check2 = c << 8;
    
    if(packed)
    {
        if((c=pack_getc(packed_src)) == EOF)
        {
            goto error;
        }
    }
    else
    {
        if((c=fgetc(normal_src)) == EOF)
        {
            goto error;
        }
    }
    
    check2 += c & 255;
    
    // verify checksums
    r = rand_007(method);
    check1 ^= r;
    check2 -= r;
    check1 &= 0xFFFF;
    check2 &= 0xFFFF;
    
    if(check1 != c1 || check2 != c2)
    {
        err = 5;
        goto error;
    }
    
    if(packed)
    {
        pack_fclose(packed_src);
    }
    else
    {
        fclose(normal_src);
    }
    
    fclose(dest);
    return 0;
    
error:

    if(packed)
    {
        pack_fclose(packed_src);
    }
    else
    {
        fclose(normal_src);
    }
    
    fclose(dest);
    delete_file(destfile);
    return err;
}

void copy_file(const char *src, const char *dest)
{
    int32_t c;
    FILE *fin, *fout;
    fin = fopen(src, "rb");
    fout = fopen(dest, "wb");
    
    while((c=fgetc(fin)) != EOF)
        fputc(c, fout);
        
    fclose(fin);
    fclose(fout);
}

static int32_t box_x = 0;
static int32_t box_y = 0;
static bool box_active=false;
static int32_t box_store_x = 0;
static FONT *box_title_font=font;
static FONT *box_message_font=font;
static int32_t box_style=0;
static int32_t box_titlebar_height=0;
static int32_t box_message_height=0;
static uint8_t box_text_scale=1;
static int32_t box_w=304;
static int32_t box_h=176;
static int32_t box_l=8;
static int32_t box_r=312;
static int32_t box_t=32;
static int32_t box_b=208;
static bool box_log=true;
static char box_log_msg[480];
static int32_t box_msg_pos=0;
static int32_t box_store_pos=0;
static BITMAP* box_bg=NULL;
/*
  static int32_t jwin_pal[jcMAX] =
  {
  vc(11),vc(15),vc(4),vc(7),vc(6),vc(0),
  192,223,vc(14),vc(15),vc(0),vc(1),vc(14)
  };
  */
int32_t onSnapshot2()
{
    char buf[20];
    int32_t num=0;
    
    do
    {
        sprintf(buf, "zelda%03d.bmp", ++num);
    }
    while(num<999 && exists(buf));
    
    PALETTE temppal;
    get_palette(temppal);
    BITMAP *tempbmp=create_bitmap_ex(8,screen->w, screen->h);
    blit(screen,tempbmp,0,0,0,0,screen->w,screen->h);
    save_bitmap(buf,screen,temppal);
    destroy_bitmap(tempbmp);
    return D_O_K;
}

void set_default_box_size()
{
    int32_t screen_w = screen->w;
    int32_t screen_h = screen->h;
    
    box_w=MIN(512, screen_w-16);
    box_h=MIN(256, (screen_h-64)&0xFFF0);
    
    box_l=(screen_w-box_w)/2;
    box_t=(screen_h-box_h)/2;
    box_r=box_l+box_w;
    box_b=box_t+box_h;
}
/* resizes the box */
void set_box_size(int32_t w, int32_t h)
{
	int32_t screen_w = screen->w;
	int32_t screen_h = screen->h;
	
	if(w <= 0) w = 512;
	if(h <= 0) h = 256;
	box_w=MIN(w, screen_w-16);
	box_h=MIN(h, (screen_h-64)&0xFFF0);
	
	box_l=(screen_w-box_w)/2;
	box_t=(screen_h-box_h)/2;
	box_r=box_l+box_w;
	box_b=box_t+box_h;
}

/* starts outputting a progress message */
void box_start(int32_t style, const char *title, FONT *title_font, FONT *message_font, bool log, int32_t w, int32_t h, uint8_t scale)
{
    box_text_scale=scale;
    box_style=style;
    box_title_font=(title_font!=NULL)?title_font:font;
    box_message_font=(message_font!=NULL)?message_font:font;
    box_message_height=text_height(box_message_font)*scale;
    box_titlebar_height=title?text_height(box_title_font)+2:0;
    set_box_size(w,h);
    /*
    box_w=BOX_W;
    box_h=BOX_H;
    box_l=BOX_L;
    box_r=BOX_R;
    box_t=BOX_T;
    box_b=BOX_B;
    */
    box_log=log;
    memset(box_log_msg, 0, 480);
    box_msg_pos=0;
    box_store_pos=0;
    
    if(box_bg)
    {
        destroy_bitmap(box_bg);
    }
    box_bg = create_bitmap_ex(8, box_w, box_h);
    blit(screen, box_bg, box_l, box_t, 0, 0, box_w, box_h);
    jwin_draw_win(screen, box_l, box_t, box_r-box_l, box_b-box_t, FR_WIN);
    
    if(title!=NULL)
    {
        zc_swap(font,box_title_font);
        jwin_draw_titlebar(screen, box_l+3, box_t+3, box_r-box_l-6, 18, title, false);
        zc_swap(font,box_title_font);
        box_titlebar_height=18;
    }
    
    
    box_store_x = box_x = box_y = 0;
    box_active = true;
    box_t+=box_titlebar_height;
    box_h-=box_titlebar_height;
    box_log=log;
    memset(box_log_msg, 0, 480);
    box_msg_pos=0;
    box_store_pos=0;
}

/* outputs text to the progress message */
void box_out(const char *msg)
{
    string remainder = "";
    string temp(msg);
    
    if(box_active)
    {
        //do primitive text wrapping
        uint32_t i;
        for(i=0; i<temp.size(); i++)
        {
            int32_t length = text_length(box_message_font,temp.substr(0,i).c_str())*box_text_scale;
            
            if(length > box_r-box_l-16)
            {
                i = zc_max(i-1,0);
                break;
            }
        }
        
        set_clip_rect(screen, box_l+8, box_t+1, box_r-8, box_b-1);
        if(box_text_scale == 1)
            textout_ex(screen, box_message_font, temp.substr(0,i).c_str(), box_l+8+box_x, box_t+(box_y+1)*box_message_height, gui_fg_color, gui_bg_color);
        else
        {
            int32_t length = text_length(box_message_font,temp.substr(0,i).c_str());
            BITMAP* tempbit = create_bitmap_ex(8, length, box_message_height);
            clear_bitmap(tempbit);
            textout_ex(tempbit, box_message_font, temp.substr(0,i).c_str(), 0, 0, gui_fg_color, gui_bg_color);
            stretch_blit(tempbit, screen, 0, 0, length, box_message_height/box_text_scale, box_l+8+box_x, box_t+(box_y+1)*box_message_height, length*box_text_scale, box_message_height);
            destroy_bitmap(tempbit);
        }
        set_clip_rect(screen, 0, 0, screen->w-1, screen->h-1);
        remainder = temp.substr(i,temp.size()-i);
    }
    
    if(box_log)
    {
        sprintf(box_log_msg+box_msg_pos, "%s", msg);
    }
    
    box_x += text_length(box_message_font, msg);
    box_msg_pos+=(int32_t)strlen(msg);
    
    if(remainder != "")
    {
        bool oldlog = box_log;
        box_log = false;
        box_eol();
        box_out(remainder.c_str());
        box_log = oldlog;
    }
    
    if (box_active)
        update_hw_screen(true);
}

/* calls box_out, and box_eol for newlines */
void box_out_nl(const char *msg)
{
	string line;
	istringstream reader(msg);
	while (getline(reader, line))
	{
		box_out(line.c_str());
		box_eol();
	}
}
		
/* remembers the current x position */
void box_save_x()
{
    if(box_active)
    {
        box_store_x=box_x;
    }
    
    box_store_pos=box_msg_pos;
}

/* remembers the current x position */
void box_load_x()
{
    if(box_active)
    {
        box_x=box_store_x;
    }
    
    box_msg_pos=box_store_pos;
}

/* outputs text to the progress message */
void box_eol()
{
    if(box_active)
    {
        box_x = 0;
        box_y++;
        
        if((box_y+2)*box_message_height >= box_h)
        {
            blit(screen, screen, box_l+8, box_t+(box_message_height*2), box_l+8, box_t+(box_message_height), box_w-16, box_y*box_message_height);
            rectfill(screen, box_l+8, box_t+box_y*box_message_height, box_l+box_w-8, box_t+(box_y+1)*box_message_height, gui_bg_color);
            box_y--;
        }
    }
    
    box_msg_pos = 0;
    
    if(box_log)
    {
        al_trace("%s", box_log_msg);
        al_trace("\n");
        memset(box_log_msg, 0, 480);
    }
    
    if (box_active)
        update_hw_screen(true);
}

/* ends output of a progress message */
void box_end(bool pause)
{
    if(box_active)
    {
        if(pause)
        {
	    //zc_set_volume(255,-1);
	    // kill_sfx();
	    // sfx(20,128, false,true);
	
            box_eol();
            box_out("-- press a key --");
            
            do
            {
                rest(1);
            }
            while(gui_mouse_b());
            
            do
            {
                rest(1);
            }
            while((!keypressed()) && (!gui_mouse_b()));
            
            do
            {
                rest(1);
            }
            while(gui_mouse_b());
            
            clear_keybuf();
        }
        
        box_active = false;
        if(box_bg)
        {
            blit(box_bg, screen, 0, 0, box_l, box_t-box_titlebar_height, box_w, box_h+box_titlebar_height);
            destroy_bitmap(box_bg);
            box_bg = NULL;
        }
    }
}

/* pauses box output */
void box_pause()
{
    if(box_active)
    {
        box_save_x();
        box_out("-- press a key --");
        
        do
        {
            //        poll_mouse();
        }
        while(gui_mouse_b());
        
        do
        {
            //        poll_mouse();
        }
        while((!keypressed()) && (!gui_mouse_b()));
        
        do
        {
            //        poll_mouse();
        }
        while(gui_mouse_b());
        
        clear_keybuf();
        box_load_x();
    }
}

// Checking for double clicks is complicated. The user could release the
// mouse button at almost any point, and I might miss it if I am doing some
// other processing at the same time (eg. sending the single-click message).
// To get around this I install a timer routine to do the checking for me,
// so it will notice double clicks whenever they happen.

volatile int32_t dclick_status, dclick_time;

// dclick_check:
//  Double click checking user timer routine.

void dclick_check(void)
{
    if(dclick_status==DCLICK_NOT)
    {
        if(gui_mouse_b())
        {
            dclick_status = DCLICK_START;           // let's go!
            dclick_time = 0;
            return;
        }
    }
    else if(dclick_status==DCLICK_START)                 // first click...
    {
        if(!gui_mouse_b())
        {
            dclick_status = DCLICK_RELEASE;           // aah! released first
            dclick_time = 0;
            return;
        }
    }
    else if(dclick_status==DCLICK_RELEASE)          // wait for second click
    {
        if(gui_mouse_b())
        {
            dclick_status = DCLICK_AGAIN;             // yes! the second click
            dclick_time = 0;
            return;
        }
    }
    else
    {
        return;
    }
    
    // timeout?
    if(dclick_time++ > 10)
    {
        dclick_status = DCLICK_NOT;
    }
}

END_OF_FUNCTION(dclick_check)

void lock_dclick_function()
{
    LOCK_FUNCTION(dclick_check);
}


void textout_shadow_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t shadow, int32_t bg)
{
    if(bg!=-1)
    {
        rectfill(bmp, x, y, x+text_length(f, s), y+text_height(f), bg);
    }
    
    textout_ex(bmp, f, s, x+1, y, shadow, -1);
    textout_ex(bmp, f, s, x+1, y+1, shadow, -1);
    textout_ex(bmp, f, s, x, y+1, shadow, -1);
}

void textout_shadow_center_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t shadow, int32_t bg)
{
    textout_shadow_ex(bmp, f, s, x-(text_length(f, s)/2), y, shadow, bg);
}

void textout_shadow_right_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t shadow, int32_t bg)
{
    textout_shadow_ex(bmp, f, s, x-text_length(f, s), y, shadow, bg);
}

void textout_shadow_u_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t shadow, int32_t bg)
{
    if(bg!=-1)
    {
        rectfill(bmp, x-1, y, x+text_length(f, s), y+text_height(f), bg);
    }
    
    textout_ex(bmp, f, s, x+1, y, shadow, -1);
    textout_ex(bmp, f, s, x+1, y+1, shadow, -1);
    textout_ex(bmp, f, s, x, y+1, shadow, -1);
    textout_ex(bmp, f, s, x-1, y+1, shadow, -1);
    textout_ex(bmp, f, s, x-1, y, shadow, -1);
}

void textout_shadow_center_u_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t shadow, int32_t bg)
{
    textout_shadow_u_ex(bmp, f, s, x-(text_length(f, s)/2), y, shadow, bg);
}

void textout_shadow_right_u_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t shadow, int32_t bg)
{
    textout_shadow_u_ex(bmp, f, s, x-text_length(f, s), y, shadow, bg);
}

void textout_shadow_o_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t shadow, int32_t bg)
{
    if(bg!=-1)
    {
        rectfill(bmp, x-1, y-1, x+text_length(f, s), y+text_height(f), bg);
    }
    
    textout_ex(bmp, f, s, x+1, y-1, shadow, -1);
    textout_ex(bmp, f, s, x+1, y, shadow, -1);
    textout_ex(bmp, f, s, x+1, y+1, shadow, -1);
    textout_ex(bmp, f, s, x, y+1, shadow, -1);
    textout_ex(bmp, f, s, x-1, y+1, shadow, -1);
    textout_ex(bmp, f, s, x-1, y, shadow, -1);
    textout_ex(bmp, f, s, x-1, y-1, shadow, -1);
    textout_ex(bmp, f, s, x, y-1, shadow, -1);
}

void textout_shadow_center_o_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t shadow, int32_t bg)
{
    textout_shadow_o_ex(bmp, f, s, x-(text_length(f, s)/2), y, shadow, bg);
}

void textout_shadow_right_o_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t shadow, int32_t bg)
{
    textout_shadow_o_ex(bmp, f, s, x-text_length(f, s), y, shadow, bg);
}

void textout_shadow_plus_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t shadow, int32_t bg)
{
    if(bg!=-1)
    {
        rectfill(bmp, x-1, y-1, x+text_length(f, s), y+text_height(f), bg);
    }
    
    textout_ex(bmp, f, s, x+1, y, shadow, -1);
    textout_ex(bmp, f, s, x, y+1, shadow, -1);
    textout_ex(bmp, f, s, x-1, y, shadow, -1);
    textout_ex(bmp, f, s, x, y-1, shadow, -1);
}

void textout_shadow_center_plus_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t shadow, int32_t bg)
{
    textout_shadow_plus_ex(bmp, f, s, x-(text_length(f, s)/2), y, shadow, bg);
}

void textout_shadow_right_plus_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t shadow, int32_t bg)
{
    textout_shadow_plus_ex(bmp, f, s, x-text_length(f, s), y, shadow, bg);
}

void textout_shadow_x_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t shadow, int32_t bg)
{
    if(bg!=-1)
    {
        rectfill(bmp, x-1, y-1, x+text_length(f, s), y+text_height(f), bg);
    }
    
    textout_ex(bmp, f, s, x+1, y-1, shadow, -1);
    textout_ex(bmp, f, s, x+1, y+1, shadow, -1);
    textout_ex(bmp, f, s, x-1, y+1, shadow, -1);
    textout_ex(bmp, f, s, x-1, y-1, shadow, -1);
}

void textout_shadow_center_x_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t shadow, int32_t bg)
{
    textout_shadow_x_ex(bmp, f, s, x-(text_length(f, s)/2), y, shadow, bg);
}

void textout_shadow_right_x_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t shadow, int32_t bg)
{
    textout_shadow_x_ex(bmp, f, s, x-text_length(f, s), y, shadow, bg);
}

void textout_shadowed_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg)
{
    textout_shadow_ex(bmp, f, s, x, y, shadow, bg);
    textout_ex(bmp, f, s, x, y, color, -1);
}

void textout_shadowed_center_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg)
{
    textout_shadowed_ex(bmp, f, s, x-(text_length(f, s)/2), y, color, shadow, bg);
}

void textout_shadowed_right_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg)
{
    textout_shadowed_ex(bmp, f, s, x-text_length(f, s), y, color, shadow, bg);
}

void textout_shadowed_u_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg)
{
    textout_shadow_u_ex(bmp, f, s, x, y, shadow, bg);
    textout_ex(bmp, f, s, x, y, color, -1);
}

void textout_shadowed_center_u_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg)
{
    textout_shadowed_u_ex(bmp, f, s, x-(text_length(f, s)/2), y, color, shadow, bg);
}

void textout_shadowed_right_u_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg)
{
    textout_shadowed_u_ex(bmp, f, s, x-text_length(f, s), y, color, shadow, bg);
}

void textout_shadowed_o_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg)
{
    textout_shadow_o_ex(bmp, f, s, x, y, shadow, bg);
    textout_ex(bmp, f, s, x, y, color, -1);
}

void textout_shadowed_center_o_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg)
{
    textout_shadowed_o_ex(bmp, f, s, x-(text_length(f, s)/2), y, color, shadow, bg);
}

void textout_shadowed_right_o_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg)
{
    textout_shadowed_o_ex(bmp, f, s, x-text_length(f, s), y, color, shadow, bg);
}

void textout_shadowed_plus_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg)
{
    textout_shadow_plus_ex(bmp, f, s, x, y, shadow, bg);
    textout_ex(bmp, f, s, x, y, color, -1);
}

void textout_shadowed_center_plus_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg)
{
    textout_shadowed_plus_ex(bmp, f, s, x-(text_length(f, s)/2), y, color, shadow, bg);
}

void textout_shadowed_right_plus_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg)
{
    textout_shadowed_plus_ex(bmp, f, s, x-text_length(f, s), y, color, shadow, bg);
}

void textout_shadowed_x_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg)
{
    textout_shadow_x_ex(bmp, f, s, x, y, shadow, bg);
    textout_ex(bmp, f, s, x, y, color, -1);
}

void textout_shadowed_center_x_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg)
{
    textout_shadowed_x_ex(bmp, f, s, x-(text_length(f, s)/2), y, color, shadow, bg);
}

void textout_shadowed_right_x_ex(BITMAP *bmp, const FONT *f, const char *s, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg)
{
    textout_shadowed_x_ex(bmp, f, s, x-text_length(f, s), y, color, shadow, bg);
}

void textprintf_shadow_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadow_ex(bmp, f, buf, x, y, shadow, bg);
}

void textprintf_shadow_center_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadow_ex(bmp, f, buf, x-(text_length(f, buf)/2), y, shadow, bg);
}

void textprintf_shadow_right_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadow_ex(bmp, f, buf, x-text_length(f, buf), y, shadow, bg);
}


void textprintf_shadow_u_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadow_u_ex(bmp, f, buf, x, y, shadow, bg);
}

void textprintf_shadow_center_u_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadow_u_ex(bmp, f, buf, x-(text_length(f, buf)/2), y, shadow, bg);
}

void textprintf_shadow_right_u_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadow_u_ex(bmp, f, buf, x-text_length(f, buf), y, shadow, bg);
}


void textprintf_shadow_o_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadow_o_ex(bmp, f, buf, x, y, shadow, bg);
}

void textprintf_shadow_center_o_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadow_o_ex(bmp, f, buf, x-(text_length(f, buf)/2), y, shadow, bg);
}

void textprintf_shadow_right_o_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadow_o_ex(bmp, f, buf, x-text_length(f, buf), y, shadow, bg);
}







void textprintf_shadow_plus_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadow_plus_ex(bmp, f, buf, x, y, shadow, bg);
}

void textprintf_shadow_center_plus_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadow_plus_ex(bmp, f, buf, x-(text_length(f, buf)/2), y, shadow, bg);
}

void textprintf_shadow_right_plus_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadow_plus_ex(bmp, f, buf, x-text_length(f, buf), y, shadow, bg);
}

void textprintf_shadow_x_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadow_x_ex(bmp, f, buf, x, y, shadow, bg);
}

void textprintf_shadow_center_x_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadow_x_ex(bmp, f, buf, x-(text_length(f, buf)/2), y, shadow, bg);
}

void textprintf_shadow_right_x_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadow_x_ex(bmp, f, buf, x-text_length(f, buf), y, shadow, bg);
}

void textprintf_shadowed_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadowed_ex(bmp, f, buf, x, y, color, shadow, bg);
}

void textprintf_shadowed_center_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadowed_ex(bmp, f, buf, x-(text_length(f, buf)/2), y, color, shadow, bg);
}

void textprintf_shadowed_right_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadowed_ex(bmp, f, buf, x-text_length(f, buf), y, color, shadow, bg);
}

void textprintf_shadowed_u_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadowed_u_ex(bmp, f, buf, x, y, color, shadow, bg);
}

void textprintf_shadowed_center_u_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadowed_u_ex(bmp, f, buf, x-(text_length(f, buf)/2), y, color, shadow, bg);
}

void textprintf_shadowed_right_u_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadowed_u_ex(bmp, f, buf, x-text_length(f, buf), y, color, shadow, bg);
}

void textprintf_shadowed_o_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadowed_o_ex(bmp, f, buf, x, y, color, shadow, bg);
}

void textprintf_shadowed_center_o_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadowed_o_ex(bmp, f, buf, x-(text_length(f, buf)/2), y, color, shadow, bg);
}

void textprintf_shadowed_right_o_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadowed_o_ex(bmp, f, buf, x-text_length(f, buf), y, color, shadow, bg);
}

void textprintf_shadowed_plus_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadowed_plus_ex(bmp, f, buf, x, y, color, shadow, bg);
}

void textprintf_shadowed_center_plus_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadowed_plus_ex(bmp, f, buf, x-(text_length(f, buf)/2), y, color, shadow, bg);
}

void textprintf_shadowed_right_plus_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadowed_plus_ex(bmp, f, buf, x-text_length(f, buf), y, color, shadow, bg);
}


void textprintf_shadowed_x_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadowed_x_ex(bmp, f, buf, x, y, color, shadow, bg);
}

void textprintf_shadowed_center_x_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadowed_x_ex(bmp, f, buf, x-(text_length(f, buf)/2), y, color, shadow, bg);
}

void textprintf_shadowed_right_x_ex(BITMAP *bmp, const FONT *f, int32_t x, int32_t y, int32_t color, int32_t shadow, int32_t bg, const char *format, ...)
{
    char buf[512];
    va_list ap;
    ASSERT(bmp);
    ASSERT(f);
    ASSERT(format);
    
    va_start(ap, format);
    uvszprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    textout_shadowed_x_ex(bmp, f, buf, x-text_length(f, buf), y, color, shadow, bg);
}

// A lot of crashes in ZQuest can be traced to rect(). Hopefully, this will help.
void safe_rect(BITMAP *bmp, int x1, int y1, int x2, int y2, int color)
{
	rect(bmp, vbound(x1, 0, bmp->w-1), vbound(y1, 0, bmp->h-1), vbound(x2, 0, bmp->w-1), vbound(y2, 0, bmp->h-1), color);
}
void safe_rect(BITMAP *bmp, int x1, int y1, int x2, int y2, int color, int thick)
{
	if(thick < 1) return;
	if(x1 > x2) zc_swap(x1,x2);
	if(y1 > y2) zc_swap(y1,y2);
	for(int q = 0; q < thick; ++q)
		safe_rect(bmp,x1+q,y1+q,x2-q,y2-q,color);
}

//computes the positive gcd of two integers (using Euclid's algorithm)

int32_t gcd(int32_t a, int32_t b)
{
    a = abs(a);
    b = abs(b);
    
    if(b == 0)
        return a;
        
    int32_t res = a%b;
    
    while(res != 0)
    {
        a = b;
        b = res;
        res = a%b;
    }
    
    return b;
}

//computes the positive lcm of two integers
int32_t lcm(int32_t a, int32_t b)
{
    return a*b/gcd(a,b);
}

size_t count_digits(int32_t n)
{
    return std::to_string(n).size();
}

void sane_destroy_bitmap(BITMAP **bmp)
{
	if(*bmp)
	{
		destroy_bitmap(*bmp);
		*bmp = NULL;
	}
}