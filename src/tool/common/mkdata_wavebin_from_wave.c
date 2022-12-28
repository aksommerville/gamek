/* mkdata_wavebin_from_wave.c
 * A more apt name might have been "mkdata_wave_compile", but I'm keeping to the naming pattern for converters.
 * This file illogically lives in tool/common instead of tool/mkdata, because fiddle needs it too.
 */
 
#include "opt/serial/serial.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MKDATA_WAVE_LENGTH 512
 
/* Compiler context.
 */
 
struct mkdata_wave_compiler {
  const char *path;
  int lineno;
  float wave[MKDATA_WAVE_LENGTH];
  int init;
};

static void mkdata_wave_compiler_cleanup(struct mkdata_wave_compiler *ctx) {
}

/* Find peak level.
 */
 
static float mkdata_wave_find_peak(const float *v,int c) {
  float peak=0.0f;
  for (;c-->0;v++) {
    if (*v>peak) peak=*v;
    else if (*v<-peak) peak=-*v;
  }
  return peak;
}

static float mkdata_wave_measure_rms(const float *v,int c) {
  if (c<1) return 0.0f;
  float sqsum=0.0f;
  int i=c; for (;i-->0;v++) sqsum+=(*v)*(*v);
  return sqrtf(sqsum/c);
}

/* Generate a linear ramp or flat line.
 */
 
static void mkdata_wave_ramp(float *v,int c,float a,float z) {
  if (c<1) return;
  if (a==z) {
    for (;c-->0;v++) *v=a;
  } else {
    int limit=c;
    for (;c-->0;v++) {
      *v=(a*c+z*(limit-c))/limit;
    }
  }
}

/* "shape"
 */
 
static void mkdata_wave_shape_sine(struct mkdata_wave_compiler *ctx) {
  float *v=ctx->wave;
  int i=0;
  for (;i<MKDATA_WAVE_LENGTH;i++,v++) {
    *v=sinf((i*M_PI*2.0f)/MKDATA_WAVE_LENGTH);
  }
}

static void mkdata_wave_shape_sawup(struct mkdata_wave_compiler *ctx) {
  mkdata_wave_ramp(ctx->wave,MKDATA_WAVE_LENGTH,-1.0f,1.0f);
}

static void mkdata_wave_shape_sawdown(struct mkdata_wave_compiler *ctx) {
  mkdata_wave_ramp(ctx->wave,MKDATA_WAVE_LENGTH,1.0f,-1.0f);
}

static void mkdata_wave_shape_square(struct mkdata_wave_compiler *ctx) {
  int headc=MKDATA_WAVE_LENGTH>>1;
  int tailc=MKDATA_WAVE_LENGTH-headc;
  mkdata_wave_ramp(ctx->wave,headc,1.0f,1.0f);
  mkdata_wave_ramp(ctx->wave+headc,tailc,-1.0f,-1.0f);
}

static void mkdata_wave_shape_triangle(struct mkdata_wave_compiler *ctx) {
  // All other waves are at 0 phase, but triangles start at 3/4 phase for geometric convenience.
  int headc=MKDATA_WAVE_LENGTH>>1;
  int tailc=MKDATA_WAVE_LENGTH-headc;
  mkdata_wave_ramp(ctx->wave,headc,-1.0f,1.0f);
  mkdata_wave_ramp(ctx->wave+headc,tailc,1.0f,-1.0f);
}

static void mkdata_wave_shape_noise(struct mkdata_wave_compiler *ctx) {
  float *v=ctx->wave;
  int i=MKDATA_WAVE_LENGTH;
  for (;i-->0;v++) *v=((rand()&0xffff)-0x8000)/32768.0f;
}

static void mkdata_wave_shape_silence(struct mkdata_wave_compiler *ctx) {
  memset(ctx->wave,0,sizeof(ctx->wave));
}
 
static int mkdata_wave_command_shape(struct mkdata_wave_compiler *ctx,const char *src,int srcc) {

  // Aliases.
  if ((srcc==3)&&!memcmp(src,"saw",3)) { src="sawdown"; srcc=7; }

  #define SHAPE(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(src,#tag,srcc)) { mkdata_wave_shape_##tag(ctx); return 0; }
  
  SHAPE(sine)
  SHAPE(sawup)
  SHAPE(sawdown)
  SHAPE(square)
  SHAPE(triangle)
  SHAPE(noise)
  SHAPE(silence)
  
  #undef SHAPE
  fprintf(stderr,"%s:%d: Unknown shape '%.*s'\n",ctx->path,ctx->lineno,srcc,src);
  return -2;
}

/* "norm"
 */
 
static int mkdata_wave_command_norm(struct mkdata_wave_compiler *ctx,const char *src,int srcc) {
  
  float target=1.0f;
  if (srcc) {
    double d;
    if (sr_double_eval(&d,src,srcc)<0) return -1;
    target=d;
  }
  
  float peak=mkdata_wave_find_peak(ctx->wave,MKDATA_WAVE_LENGTH);
  if (peak<=0.0f) return 0;
  
  float adjust=target/peak;
  float *v=ctx->wave;
  int i=MKDATA_WAVE_LENGTH;
  for (;i-->0;v++) (*v)*=adjust;
  return 0;
}

/* "normrms"
 */
 
static int mkdata_wave_command_normrms(struct mkdata_wave_compiler *ctx,const char *src,int srcc) {
  
  float target=sqrtf(2.0f)/2.0f;
  if (srcc) {
    double d;
    if (sr_double_eval(&d,src,srcc)<0) return -1;
    target=d;
  }
  
  float current=mkdata_wave_measure_rms(ctx->wave,MKDATA_WAVE_LENGTH);
  if (current<=0.0f) return 0;
  
  float adjust=target/current;
  float *v=ctx->wave;
  int i=MKDATA_WAVE_LENGTH;
  for (;i-->0;v++) (*v)*=adjust;
  
  return 0;
}

/* "clip"
 */
 
static int mkdata_wave_command_clip(struct mkdata_wave_compiler *ctx,const char *src,int srcc) {
  float level=1.0f;
  if (srcc) {
    double d;
    if (sr_double_eval(&d,src,srcc)<0) return -1;
    level=d;
  }
  float nlevel=-level;
  float *v=ctx->wave;
  int i=MKDATA_WAVE_LENGTH;
  for (;i-->0;v++) {
    if (*v>level) *v=level;
    else if (*v<nlevel) *v=nlevel;
  }
  return 0;
}

/* "gate"
 */
 
static int mkdata_wave_command_gate(struct mkdata_wave_compiler *ctx,const char *src,int srcc) {
  float level=1.0f;
  if (srcc) {
    double d;
    if (sr_double_eval(&d,src,srcc)<0) return -1;
    level=d;
  }
  float nlevel=-level;
  float *v=ctx->wave;
  int i=MKDATA_WAVE_LENGTH;
  for (;i-->0;v++) {
    if (*v>level) continue;
    if (*v<nlevel) continue;
    *v=0.0f;
  }
  return 0;
}

/* "gain"
 */
 
static int mkdata_wave_command_gain(struct mkdata_wave_compiler *ctx,const char *src,int srcc) {
  double d;
  if (sr_double_eval(&d,src,srcc)<0) return -1;
  float gain=d;
  float *v=ctx->wave;
  int i=MKDATA_WAVE_LENGTH;
  for (;i-->0;v++) (*v)*=gain;
  return 0;
}

/* "average"
 */
 
static int mkdata_wave_command_average(struct mkdata_wave_compiler *ctx,const char *src,int srcc) {
  int len;
  if (sr_int_eval(&len,src,srcc)<2) return -1;
  if (len<2) return 0;
  if (len>MKDATA_WAVE_LENGTH) len=MKDATA_WAVE_LENGTH;
  
  float *buf=malloc(sizeof(float)*len);
  if (!buf) return -1;
  
  // Prepopulate the buffer with the tail end of the signal.
  memcpy(buf,ctx->wave+MKDATA_WAVE_LENGTH-len,sizeof(float)*len);
  
  // Calculate initial average.
  float sum=0.0f;
  int i=len; while (i-->0) sum+=buf[i];
  float scale=1.0f/len;
  
  // Run across the signal like a live moving average.
  int bufp=0;
  float *v=ctx->wave;
  for (i=MKDATA_WAVE_LENGTH;i-->0;v++) {
    sum-=buf[bufp];
    sum+=*v;
    buf[bufp]=*v;
    *v=sum*scale;
    bufp++;
    if (bufp>=len) bufp=0;
  }
  
  free(buf);
  return 0;
}

/* "phase"
 */
 
static int mkdata_wave_command_phase(struct mkdata_wave_compiler *ctx,const char *src,int srcc) {
  double fphase;
  if (sr_double_eval(&fphase,src,srcc)<0) return -1;
  int p=fphase*MKDATA_WAVE_LENGTH;
  while (p<0) p+=MKDATA_WAVE_LENGTH;
  p%=MKDATA_WAVE_LENGTH;
  if (!p) return 0;
  
  // Stash head in a new buffer.
  float *buf=malloc(sizeof(float)*p);
  if (!buf) return -1;
  memcpy(buf,ctx->wave,sizeof(float)*p);
  
  // Shuffle main and copy stashed head to tail.
  memmove(ctx->wave,ctx->wave+p,sizeof(float)*(MKDATA_WAVE_LENGTH-p));
  memcpy(ctx->wave+MKDATA_WAVE_LENGTH-p,buf,sizeof(float)*p);
  
  free(buf);
  return 0;
}

/* "harmonics"
 */
 
static void mkdata_wave_add_harmonic(float *dst,int c,const float *src,int step,float coef) {
  if (step>=c) return;
  if ((coef>=-0.0f)&&(coef<=0.0f)) return;
  int srcp=0,i=c;
  for (;i-->0;dst++) {
    (*dst)+=src[srcp]*coef;
    srcp+=step;
    if (srcp>=c) srcp-=c;
  }
}
 
static int mkdata_wave_command_harmonics(struct mkdata_wave_compiler *ctx,const char *src,int srcc) {
  float *buf=calloc(sizeof(float),MKDATA_WAVE_LENGTH);
  if (!buf) return -1;
  
  int step=1;
  for (;;step++) {
    while (srcc&&((unsigned char)(*src)<=0x20)) { src++; srcc--; }
    if (!srcc) break;
    const char *token=src;
    int tokenc=0;
    while (srcc&&((unsigned char)(*src)>0x20)) { src++; srcc--; tokenc++; }
    double coef;
    if (sr_double_eval(&coef,token,tokenc)<0) {
      fprintf(stderr,"%s:%d: Expected float, found '%.*s'\n",ctx->path,ctx->lineno,tokenc,token);
      free(buf);
      return -1;
    }
    mkdata_wave_add_harmonic(buf,MKDATA_WAVE_LENGTH,ctx->wave,step,coef);
  }
  
  memcpy(ctx->wave,buf,sizeof(float)*MKDATA_WAVE_LENGTH);
  free(buf);
  return 0;
}

/* Command dispatch.
 */
 
static int mkdata_wave_command(
  struct mkdata_wave_compiler *ctx,
  const char *kw,int kwc,
  const char *arg,int argc
) {
  int err;

       if ((kwc==5)&&!memcmp(kw,"shape",5)) err=mkdata_wave_command_shape(ctx,arg,argc);
  else if ((kwc==4)&&!memcmp(kw,"norm",4)) err=mkdata_wave_command_norm(ctx,arg,argc);
  else if ((kwc==7)&&!memcmp(kw,"normrms",7)) err=mkdata_wave_command_normrms(ctx,arg,argc);
  else if ((kwc==4)&&!memcmp(kw,"clip",4)) err=mkdata_wave_command_clip(ctx,arg,argc);
  else if ((kwc==4)&&!memcmp(kw,"gate",4)) err=mkdata_wave_command_gate(ctx,arg,argc);
  else if ((kwc==4)&&!memcmp(kw,"gain",4)) err=mkdata_wave_command_gain(ctx,arg,argc);
  else if ((kwc==7)&&!memcmp(kw,"average",7)) err=mkdata_wave_command_average(ctx,arg,argc);
  else if ((kwc==5)&&!memcmp(kw,"phase",5)) err=mkdata_wave_command_phase(ctx,arg,argc);
  else if ((kwc==9)&&!memcmp(kw,"harmonics",9)) err=mkdata_wave_command_harmonics(ctx,arg,argc);

  else {
    err=-2;
    fprintf(stderr,"%s:%d: Unknown command '%.*s'\n",ctx->path,ctx->lineno,kwc,kw);
  }
  if (err<0) return err;
  ctx->init=1;
  return 0;
}

/* Wave initialization.
 */
 
static int mkdata_wave_keyword_is_overwrite(const char *kw,int kwc) {
  if ((kwc==5)&&!memcmp(kw,"shape",5)) return 1;
  return 0;
}

static void mkdata_assert_init(struct mkdata_wave_compiler *ctx) {
  if (ctx->init) return;
  mkdata_wave_command(ctx,"shape",5,"sine",4);
}

/* Compile one line.
 */
 
static int mkdata_wave_compile_line(struct mkdata_wave_compiler *ctx,const char *src,int srcc) {

  // Trim. Ignore empty and comments.
  while (srcc&&((unsigned char)src[0]<=0x20)) { src++; srcc--; }
  if (srcc<1) return 0;
  if (src[0]=='#') return 0;
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  
  // Pop keyword.
  const char *kw=src;
  int kwc=0;
  while (srcc&&((unsigned char)src[0]>0x20)) { kwc++; src++; srcc--; }
  while (srcc&&((unsigned char)src[0]<=0x20)) { src++; srcc--; }
  
  // Commands that overwrite the buffer, do them directly.
  // Otherwise we must first assert initialization (create a perfect sine wave if uninitialized).
  if (mkdata_wave_keyword_is_overwrite(kw,kwc)) {
    return mkdata_wave_command(ctx,kw,kwc,src,srcc);
  } else {
    mkdata_assert_init(ctx);
    return mkdata_wave_command(ctx,kw,kwc,src,srcc);
  }
}
 
/* Compile wave, main entry point.
 */
 
int mkdata_wavebin_from_wave(void *dstpp,const void *src,int srcc,const char *path) {
  struct mkdata_wave_compiler ctx={.path=path};
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,err;
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    ctx.lineno++;
    if ((err=mkdata_wave_compile_line(&ctx,line,linec))<0) {
      if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error during wave compilation.\n",path,ctx.lineno);
      mkdata_wave_compiler_cleanup(&ctx);
      return -2;
    }
  }
  mkdata_assert_init(&ctx); // empty input becomes a sine wave
  
  // Clamp and quantize.
  int16_t *dst=malloc(MKDATA_WAVE_LENGTH*sizeof(int16_t));
  if (!dst) {
    mkdata_wave_compiler_cleanup(&ctx);
    return -1;
  }
  *(void**)dstpp=dst;
  const float *f=ctx.wave;
  int i=MKDATA_WAVE_LENGTH;
  for (;i-->0;f++,dst++) {
    if (*f<=-1.0f) *dst=-32768;
    else if (*f>=1.0f) *dst=32767;
    else *dst=(*f)*32767.0f;
  }
  mkdata_wave_compiler_cleanup(&ctx);
  return MKDATA_WAVE_LENGTH*sizeof(int16_t);
}
