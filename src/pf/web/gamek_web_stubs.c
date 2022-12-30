/* gamek_web_stubs.c
 * I don't want to introduce a bunch of unnecessary cruft.
 * So instead, the few libc things that the app does, just emulate them here.
 */

#include "gamek_web_internal.h" 
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <string.h>

FILE *const stderr=0;

/* Fake printf.
 * Not going to bother with a compliant implementation, but let's get close:
 *  - % [ALIGN] [PAD] [SIZE] [.PRECISION] FORMAT
 *  - ALIGN is "-" or nothing.
 *  - PAD is "0", " ", or nothing.
 *  - SIZE is a decimal integer not starting with zero. Output length.
 *  - PRECISION is a decimal integer or "*". Fraction digit count or string length.
 *  - FORMAT is a letter:
 *  - - d int decimal
 *  - - x int hexadecimal
 *  - - f double float
 *  - - s char* string
 *  - - p void* address
 *  - Or "%%" for a literal single "%".
 *  - If a format unit doesn't parse perfectly, dump it and all remaining units verbatim.
 */
 
struct gamek_web_printf_unit {
  int align; // -1,0,1
  char pad; // ' ', '0'
  int size;
  int precision; // -1 if unspecified; -2 for '*'
  char format;
};

static int gamek_web_printf_unit_eval(struct gamek_web_printf_unit *unit,const char *src) {
  unit->align=1;
  unit->pad=' ';
  unit->size=0;
  unit->precision=-1;
  unit->format=0;
  
  // Introducer, and get out fast for literal '%'.
  if (!src[0]) return 0;
  if (src[0]!='%') return 0;
  if (src[1]=='%') {
    unit->format='%';
    return 2;
  }
  int srcp=1;
  
  // Align.
  if (src[srcp]=='-') {
    srcp++;
    unit->align=-1;
  }
  
  // Pad.
  if ((src[srcp]=='0')||(src[srcp]==' ')) {
    unit->pad=src[srcp++];
  }
  
  // Size.
  while ((src[srcp]>='0')&&(src[srcp]<='9')) {
    int digit=src[srcp++]-'0';
    if (unit->size>INT_MAX/10) return 0;
    unit->size*=10;
    if (unit->size>INT_MAX-digit) return 0;
    unit->size+=digit;
  }
  
  // Precision.
  if (src[srcp]=='.') {
    srcp++;
    unit->precision=0;
    if (src[srcp]=='*') {
      srcp++;
      unit->precision=-2;
    } else {
      while ((src[srcp]>='0')&&(src[srcp]<='9')) {
        int digit=src[srcp++]-'0';
        if (unit->precision>INT_MAX/10) return 0;
        unit->precision*=10;
        if (unit->precision>INT_MAX-digit) return 0;
        unit->precision+=digit;
      }
    }
  }
  
  // Format. Don't enumerate them here, just require that it be a letter.
  if (
    ((src[srcp]>='a')&&(src[srcp]<='z'))||
    ((src[srcp]>='A')&&(src[srcp]<='Z'))
  ) {
    unit->format=src[srcp++];
    return srcp;
  }
  return 0;
}

static int gamek_web_printf_unit_execute(char *dst,int dsta,struct gamek_web_printf_unit *unit,va_list *vargs) {
  // The '%' format is special.
  if (unit->format=='%') {
    if (dsta>=1) dst[0]='%';
    return 1;
  }
  // All others, we work in a temporary buffer.
  char storage[64];
  const char *buf=storage;
  int bufc=0,vi;
  switch (unit->format) {
  
    case 'd': {
        if (unit->precision==-2) return -1;
        vi=va_arg(*vargs,int);
       _int_vi_:;
        int i;
        if (vi<0) {
          bufc=2;
          int limit=-10;
          while (vi<=limit) { bufc++; if (limit<INT_MIN/10) break; limit*=10; }
          for (i=bufc;i-->1;vi/=10) storage[i]='0'-vi%10;
          storage[0]='-';
        } else {
          bufc=1;
          int limit=10;
          while (vi>=limit) { bufc++; if (limit>INT_MAX/10) break; limit*=10; }
          for (i=bufc;i-->0;vi/=10) storage[i]='0'+vi%10;
        }
      } break;
      
    case 'x': {
        if (unit->precision==-2) return -1;
        unsigned int v=va_arg(*vargs,int);
        bufc=1;
        unsigned int mask=~0xf;
        while (v&mask) { bufc++; mask<<=4; }
        int i=bufc; for (;i-->0;v>>=4) storage[i]="0123456789abcdef"[v&15];
      } break;
      
    case 'p': {
        if (unit->precision==-2) return -1;
        uintptr_t v=va_arg(*vargs,uintptr_t);
        bufc=1;
        uintptr_t mask=~(uintptr_t)0xf;
        while (v&mask) { bufc++; mask<<=4; }
        int i=bufc; for (;i-->0;v>>=4) storage[i]="0123456789abcdef"[v&15];
      } break;
      
    case 'f': {
        // We don't really use floats, so not going to do this well...
        if (unit->precision==-2) return -1;
        double f=va_arg(*vargs,double);
        if (f<=INT_MIN) vi=INT_MIN;
        else if (f>=INT_MAX) vi=INT_MAX;
        else vi=(int)f;
        goto _int_vi_;
      }
      
    case 'c': {
        if (unit->precision==-2) return -1;
        int ch=va_arg(*vargs,int);
        ch&=0x7f;
        storage[0]=ch;
        bufc=1;
      } break;
      
    case 's': {
        if (unit->precision==-2) {
          bufc=va_arg(*vargs,int);
          if (bufc<0) bufc=0;
        } else bufc=-1;
        buf=va_arg(*vargs,const char*);
        if (!buf) bufc=0; else if (bufc<0) { bufc=0; if (buf) while (buf[bufc]) bufc++; }
      } break;
  
    default: return -1;
  }
  // Then apply alignment from buffer into dst.
  int dstc=bufc;
  if (unit->size>dstc) dstc=unit->size;
  if (dstc<dsta) {
    memset(dst,unit->pad,dstc);
    if (unit->align<0) {
      memcpy(dst,buf,bufc);
    } else if (unit->align>0) {
      memcpy(dst+dstc-bufc,buf,bufc);
    } else {
      memcpy(dst+(dstc>>1)-(bufc>>1),buf,bufc);
    }
  }
  return dstc;
}

int vsnprintf(char *dst,size_t dsta,const char *fmt,va_list vargs) {
  if (!dst||(dsta<0)) dsta=0;
  int dstc=0,ok=1;
  while (*fmt) {
    if (ok&&(*fmt=='%')) {
      struct gamek_web_printf_unit unit;
      int srcerr=gamek_web_printf_unit_eval(&unit,fmt);
      if (srcerr>0) {
        int dsterr=gamek_web_printf_unit_execute(dst+dstc,(int)dsta-dstc,&unit,&vargs);
        if (dsterr<0) {
          ok=0;
        } else {
          dstc+=dsterr;
          fmt+=srcerr;
        }
      } else {
        ok=0;
      }
    } else {
      if (dstc<dsta) dst[dstc]=*fmt;
      dstc++;
      fmt++;
    }
  }
  if (dsta>0) {
    if (dstc>=dsta) dst[dsta-1]=0;
    else dst[dstc]=0;
  }
  return dstc;
}

int fprintf(FILE *f,const char *fmt,...) {
  va_list vargs;
  va_start(vargs,fmt);
  char tmp[256];
  int tmpc=vsnprintf(tmp,sizeof(tmp),fmt,vargs);
  if ((tmpc>0)&&(tmpc<sizeof(tmp))) {
    gamek_web_external_console_log(tmp);
  }
  return tmpc;
}

int snprintf(char *dst,size_t dstc,const char *fmt,...) {
  va_list vargs;
  va_start(vargs,fmt);
  return vsnprintf(dst,dstc,fmt,vargs);
}

/* Other stdlib hooks, either implemented or stubbed.
 */

size_t fwrite(const void *src,size_t c,size_t len,FILE *f) {
  return 0;
}

int fputc(int ch,FILE *f) {
  return 0;
}

void *memcpy(void *dst,const void *src,unsigned long c) {
  uint8_t *DST=dst;
  const uint8_t *SRC=src;
  for (;c-->0;DST++,SRC++) *DST=*SRC;
  return dst;
}

void *memset(void *dst,int src,unsigned long c) {
  uint8_t *DST=dst;
  for (;c-->0;DST++) *DST=src;
  return dst;
}

void *memmove(void *dst,const void *src,unsigned long c) {
  uint8_t *DST=dst;
  const uint8_t *SRC=src;
  if (DST<SRC) {
    for (;c-->0;DST++,SRC++) *DST=*SRC;
  } else {
    DST+=c;
    SRC+=c;
    for (;c-->0;) *(--DST)=*(--SRC);
  }
  return 0;
}

void free(void *v) {
}

void *calloc(size_t c,size_t len) { // TODO figure out why the web build thinks it needs calloc(). (it's not supposed to)
  return 0;
}
