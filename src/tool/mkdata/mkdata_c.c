#include "opt/serial/serial.h"
#include <stdint.h>

// Something we can use as a preprocessor "if" condition, to know whether to include <avr/pgmspace.h>
#define MKDATA_C_HAVE_PROGMEM_TEST "GAMEK_USE_tiny"

// We'll exceed it by one unit before each line break.
#define MKDATA_C_LINE_LIMIT 100

/* Encode payload, just the numbers.
 */
 
static int mkdata_encode_c_payload_8(struct sr_encoder *dst,const uint8_t *v,int c) {
  int linelen=0,err;
  for (;c-->0;v++) {
    if ((err=sr_encode_fmt(dst,"%d,",*v))<0) return -1;
    linelen+=err;
    if (linelen>=MKDATA_C_LINE_LIMIT) {
      if (sr_encode_raw(dst,"\n",1)<0) return -1;
      linelen=0;
    }
  }
  if (linelen&&(sr_encode_raw(dst,"\n",-1)<0)) return -1;
  return 0;
}
 
static int mkdata_encode_c_payload_16(struct sr_encoder *dst,const uint16_t *v,int c) {
  int linelen=0,err;
  for (;c-->0;v++) {
    if ((err=sr_encode_fmt(dst,"%d,",*v))<0) return -1;
    linelen+=err;
    if (linelen>=MKDATA_C_LINE_LIMIT) {
      if (sr_encode_raw(dst,"\n",1)<0) return -1;
      linelen=0;
    }
  }
  if (linelen&&(sr_encode_raw(dst,"\n",-1)<0)) return -1;
  return 0;
}
 
static int mkdata_encode_c_payload_32(struct sr_encoder *dst,const uint32_t *v,int c) {
  int linelen=0,err;
  for (;c-->0;v++) {
    if ((err=sr_encode_fmt(dst,"%d,",*v))<0) return -1;
    linelen+=err;
    if (linelen>=MKDATA_C_LINE_LIMIT) {
      if (sr_encode_raw(dst,"\n",1)<0) return -1;
      linelen=0;
    }
  }
  if (linelen&&(sr_encode_raw(dst,"\n",-1)<0)) return -1;
  return 0;
}
 
static int mkdata_encode_c_payload_64(struct sr_encoder *dst,const uint64_t *v,int c) {
  int linelen=0,err;
  for (;c-->0;v++) {
    if ((err=sr_encode_fmt(dst,"%lld,",(long long)(*v)))<0) return -1;
    linelen+=err;
    if (linelen>=MKDATA_C_LINE_LIMIT) {
      if (sr_encode_raw(dst,"\n",1)<0) return -1;
      linelen=0;
    }
  }
  if (linelen&&(sr_encode_raw(dst,"\n",-1)<0)) return -1;
  return 0;
}

/* Encode to C text in context.
 */
 
static int mkdata_encode_c_internal(
  struct sr_encoder *dst,
  const void *src,int srcc,
  const char *tname,
  int tsize, // bytes per unit
  const char *name
) {
  int unitc=srcc/tsize;
  if (sr_encode_raw(dst,"#include <stdint.h>\n",-1)<0) return -1;
  if (sr_encode_raw(dst,"#if "MKDATA_C_HAVE_PROGMEM_TEST"\n#include <avr/pgmspace.h>\n#else\n#define PROGMEM\n#endif\n",-1)<0) return -1;
  if (sr_encode_fmt(dst,"const %s %s[%d] PROGMEM={\n",tname,name,unitc)<0) return -1;
  switch (tsize) {
    case 1: if (mkdata_encode_c_payload_8(dst,src,unitc)<0) return -1; break;
    case 2: if (mkdata_encode_c_payload_16(dst,src,unitc)<0) return -1; break;
    case 4: if (mkdata_encode_c_payload_32(dst,src,unitc)<0) return -1; break;
    case 8: if (mkdata_encode_c_payload_64(dst,src,unitc)<0) return -1; break;
    default: return -1;
  }
  if (sr_encode_raw(dst,"};\n",-1)<0) return -1;
  return 0;
}

/* Encode to C text, main entry point.
 */
 
int mkdata_encode_c(
  void *dst,
  const void *src,int srcc,
  int unit_type, // eg -8=int8_t, 8=uint8_t, ...
  const char *name
) {
  if (!dst||(srcc<0)||(srcc&&!src)||!name) return -1;
  
  const char *tname;
  int tsize=unit_type;
  if (tsize<0) tsize=-tsize;
  tsize>>=3;
  if (srcc%tsize) return -1;
  switch (unit_type) {
    case -64: tname="int64_t"; break;
    case -32: tname="int32_t"; break;
    case -16: tname="int16_t"; break;
    case -8: tname="int8_t"; break;
    case 8: tname="uint8_t"; break;
    case 16: tname="uint16_t"; break;
    case 32: tname="uint32_t"; break;
    case 64: tname="uint64_t"; break;
    default: return -1;
  }
  
  struct sr_encoder encoder={0};
  if (mkdata_encode_c_internal(&encoder,src,srcc,tname,tsize,name)<0) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  *(void**)dst=encoder.v;
  return encoder.c;
}
