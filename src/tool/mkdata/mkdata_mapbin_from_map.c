#include "opt/serial/serial.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define MKDATA_MAP_SECTION_UNSET 0
#define MKDATA_MAP_SECTION_LEGEND 1
#define MKDATA_MAP_SECTION_MAP 2
#define MKDATA_MAP_SECTION_COMMANDS 3

/* Compiler context.
 */
 
struct mkdata_map_ctx {
  struct sr_encoder dst;
  const char *path;
  int lineno;
  int section;
  int commands_length;
  struct mkdata_legend {
    uint16_t id; // big-endian
    uint8_t v;
    int lineno;
  } *legendv;
  int legendc,legenda;
};

static void mkdata_map_ctx_cleanup(struct mkdata_map_ctx *ctx) {
  sr_encoder_cleanup(&ctx->dst);
  if (ctx->legendv) free(ctx->legendv);
}

/* Legend.
 */
 
static int mkdata_legend_search(const struct mkdata_map_ctx *ctx,uint16_t id) {
  int lo=0,hi=ctx->legendc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct mkdata_legend *legend=ctx->legendv+ck;
         if (id<legend->id) hi=ck;
    else if (id>legend->id) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int mkdata_legend_insert(struct mkdata_map_ctx *ctx,int p,uint16_t id,uint8_t v) {
  if ((p<0)||(p>ctx->legendc)) return -1;
  if (p&&(id<=ctx->legendv[p-1].id)) return -1;
  if ((p<ctx->legendc)&&(id>=ctx->legendv[p].id)) return -1;
  if (ctx->legendc>=ctx->legenda) {
    int na=ctx->legenda+32;
    if (na>INT_MAX/sizeof(struct mkdata_legend)) return -1;
    void *nv=realloc(ctx->legendv,sizeof(struct mkdata_legend)*na);
    if (!nv) return -1;
    ctx->legendv=nv;
    ctx->legenda=na;
  }
  struct mkdata_legend *legend=ctx->legendv+p;
  memmove(legend+1,legend,sizeof(struct mkdata_legend)*(ctx->legendc-p));
  ctx->legendc++;
  legend->id=id;
  legend->v=v;
  legend->lineno=ctx->lineno;
  return 0;
}

/* LEGEND section.
 */
 
static int mkdata_map_begin_LEGEND(struct mkdata_map_ctx *ctx) {
  ctx->section=MKDATA_MAP_SECTION_LEGEND;
  return 0;
}

static int mkdata_map_line_LEGEND(struct mkdata_map_ctx *ctx,const char *src,int srcc) {
  if ((srcc<4)||(src[2]!=0x20)) {
    fprintf(stderr,"%s:%d: Lines in LEGEND section must be at least four bytes: IDENTIFIER SPACE VALUE\n",ctx->path,ctx->lineno);
    return -2;
  }
  int v;
  if ((sr_int_eval(&v,src+3,srcc-3)<2)||(v<0)||(v>0xff)) {
    fprintf(stderr,"%s:%d: Expected value in 0..255, found '%.*s'\n",ctx->path,ctx->lineno,srcc-3,src+3);
    return -2;
  }
  uint16_t id=(((unsigned char)src[0])<<8)|(unsigned char)src[1];
  int p=mkdata_legend_search(ctx,id);
  if (p<0) {
    if (mkdata_legend_insert(ctx,-p-1,id,v)<0) return -1;
  } else if (ctx->legendv[p].v==v) {
    // redundant redefinition, whatever.
  } else {
    fprintf(stderr,
      "%s:%d: Redefinition of identifier '%.2s' as %d. Previously defined at line %d with value %d.\n",
      ctx->path,ctx->lineno,src,v,ctx->legendv[p].lineno,ctx->legendv[p].v
    );
    return -2;
  }
  return 0;
}

/* MAP section.
 */
 
static int mkdata_map_begin_MAP(struct mkdata_map_ctx *ctx) {
  if (ctx->dst.v[0]||ctx->dst.v[1]) {
    fprintf(stderr,"%s:%d: Multiple MAP sections. You may only have one.\n",ctx->path,ctx->lineno);
    return -2;
  }
  if (ctx->commands_length) {
    fprintf(stderr,"%s:%d: MAP not permitted after COMMANDS\n",ctx->path,ctx->lineno);
    return -2;
  }
  ctx->section=MKDATA_MAP_SECTION_MAP;
  return 0;
}

static int mkdata_map_line_decode(uint8_t *dst,struct mkdata_map_ctx *ctx,const char *src,int srcc) {
  for (;srcc;dst++,src+=2,srcc-=2) {
    uint16_t id=(((unsigned char)src[0])<<8)|(unsigned char)src[1];
    int p=mkdata_legend_search(ctx,id);
    if (p>=0) {
      *dst=ctx->legendv[p].v;
    } else {
      int hi=sr_digit_eval(src[0]);
      int lo=sr_digit_eval(src[1]);
      if ((hi<0)||(hi>15)||(lo<0)||(lo>15)) {
        fprintf(stderr,"%s:%d: Undefined identifier '%.2s'\n",ctx->path,ctx->lineno,src);
        return -2;
      }
      *dst=(hi<<4)|lo;
    }
  }
  return 0;
}

static int mkdata_map_line_MAP(struct mkdata_map_ctx *ctx,const char *src,int srcc) {

  // If the header width is set, input must match it. Otherwise use this input to set it.
  uint8_t w=ctx->dst.v[0];
  if (w) {
    int expect=w<<1;
    if (srcc!=expect) {
      fprintf(stderr,"%s:%d: Width of map rows must be %d, found %d%s.\n",ctx->path,ctx->lineno,w,srcc>>1,(srcc&1)?".5":"");
      return -2;
    }
  } else if (srcc&1) {
    fprintf(stderr,"%s:%d: Map rows can't have odd length. Found %d.\n",ctx->path,ctx->lineno,srcc);
    return -2;
  } else if (srcc>510) {
    fprintf(stderr,"%s:%d: Too wide. %d, limit 510 (255 cells).\n",ctx->path,ctx->lineno,srcc);
    return -2;
  } else {
    w=ctx->dst.v[0]=srcc>>1;
  }
  
  // Increment header height, or fail if it overflows.
  uint8_t h=ctx->dst.v[1];
  if (h==0xff) {
    fprintf(stderr,"%s:%d: Height limit 255.\n",ctx->path,ctx->lineno);
    return -2;
  }
  ctx->dst.v[1]=h+1;
  
  if (sr_encoder_require(&ctx->dst,w)<0) return -1;
  uint8_t *dst=ctx->dst.v+ctx->dst.c;
  ctx->dst.c+=w;
  return mkdata_map_line_decode(dst,ctx,src,srcc);
}

/* Command opcodes.
 */
 
static int mkdata_map_command_opcode_eval(const struct mkdata_map_ctx *ctx,const char *src,int srcc) {
  
  //TODO named opcodes
  
  int v;
  if (sr_int_eval(&v,src,srcc)>=2) {
    if ((v>=0)&&(v<0x100)) return v;
  }
  return -1;
}

// (*fmt) will be the size in bytes, ready for sr_encode_intbe
static int mkdata_map_command_value_eval(
  int *v,int *fmt,char *order,
  const struct mkdata_map_ctx *ctx,
  const char *src,int srcc,
  uint8_t opcode,int paylen
) {

  //TODO special formats per (opcode,paylen)

  *order='b';
  *fmt=1;
  if ((srcc>=3)&&!memcmp(src,"0x",2)) switch (srcc-2) {
    case 2: *fmt=1; break;
    case 4: *fmt=2; break;
    case 6: *fmt=3; break;
    case 8: *fmt=4; break;
  }
  int i=0; for (;i<srcc;i++) {
    if (src[i]=='_') {
      int j=i+1; for (;j<srcc;j++) switch (src[j]) {
        case 'b': *order='b'; break;
        case 'l': *order='l'; break;
        case '1': *fmt=1; break;
        case '2': *fmt=2; break;
        case '3': *fmt=3; break;
        case '4': *fmt=4; break;
        default: {
            fprintf(stderr,"%s:%d: Unexpected integer format char '%c'\n",ctx->path,ctx->lineno,src[j]);
            return -2;
          }
      }
      srcc=i;
      break;
    }
  }
  if (sr_int_eval(v,src,srcc)<2) {
    fprintf(stderr,"%s:%d: Failed to evaluate '%.*s' as integer.\n",ctx->path,ctx->lineno,srcc,src);
    return -2;
  }
  switch (*fmt) {
    case 1: if ((*v<-128)||(*v>0xff)) { fprintf(stderr,"%s:%d: Value %d out of range for 8 bits\n",ctx->path,ctx->lineno,*v); return -2; } break;
    case 2: if ((*v<-32768)||(*v>0xffff)) { fprintf(stderr,"%s:%d: Value %d out of range for 16 bits\n",ctx->path,ctx->lineno,*v); return -2; } break;
    case 3: if ((*v<-8388608)||(*v>0xffffff)) { fprintf(stderr,"%s:%d: Value %d out of range for 24 bits\n",ctx->path,ctx->lineno,*v); return -2; } break;
  }
}

/* COMMANDS section.
 */
 
static int mkdata_map_begin_COMMANDS(struct mkdata_map_ctx *ctx) {
  if (!ctx->dst.v[0]||!ctx->dst.v[1]) {
    fprintf(stderr,"%s:%d: Expected MAP section before COMMANDS\n",ctx->path,ctx->lineno);
    return -2;
  }
  ctx->section=MKDATA_MAP_SECTION_COMMANDS;
  return 0;
}

static int mkdata_map_line_COMMANDS(struct mkdata_map_ctx *ctx,const char *src,int srcc) {
  int i=0; for (;i<srcc;i++) if (src[i]=='#') srcc=i; // strip comment
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return 0; // just whitespace, that's fine
  
  // Opcode.
  const char *kw=src+srcp;
  int kwc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; kwc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  int opcode=mkdata_map_command_opcode_eval(ctx,kw,kwc);
  if (opcode<0) {
    if (opcode!=-2) fprintf(stderr,"%s:%d: Unknown command opcode '%.*s'\n",ctx->path,ctx->lineno,kwc,kw);
    return -2;
  }
  if (!opcode) {
    fprintf(stderr,"%s:%d: Explicit commands terminator not allowed.\n",ctx->path,ctx->lineno);
    return -2;
  }
  if (sr_encode_u8(&ctx->dst,opcode)<0) return -1;
  ctx->commands_length++;
  
  // Payload.
  int paylen=0;
  while (srcp<srcc) {
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenc++; }
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    int v,fmt,err;
    char order;
    if ((err=mkdata_map_command_value_eval(&v,&fmt,&order,ctx,token,tokenc,opcode,paylen))<0) {
      if (err!=-2) fprintf(stderr,"%s:%d: Failed to evaluate '%.*s' as command parameter.\n",ctx->path,ctx->lineno,tokenc,token);
      return -2;
    }
    if (order=='b') {
      if (sr_encode_intbe(&ctx->dst,v,fmt)<0) return -1;
    } else {
      if (sr_encode_intle(&ctx->dst,v,fmt)<0) return -1;
    }
    if (fmt<0) paylen-=fmt; else paylen+=fmt;
  }
  ctx->commands_length+=paylen;
  
  // Validate payload length.
  #define EXPECT(c) if (paylen!=c) { \
    fprintf(stderr,"%s:%d: Opcode 0x%02x expects %d bytes payload, found %d\n",ctx->path,ctx->lineno,opcode,c,paylen); \
    return -2; \
  }
       if (opcode<0x20) { EXPECT(0) }
  else if (opcode<0x40) { EXPECT(1) }
  else if (opcode<0x60) { EXPECT(2) }
  else if (opcode<0x80) { EXPECT(4) }
  else if (opcode<0xa0) { EXPECT(6) }
  else if (opcode<0xc0) { EXPECT(8) }
  else if (opcode<0xe0) {
    // Variable-length opcodes, the user must provide the length (even though in theory we could take care of it).
    if (paylen<1) {
      fprintf(stderr,"%s:%d: Variable-length command 0x%02x requires a one-byte length.\n",ctx->path,ctx->lineno,opcode);
      return -2;
    }
    uint8_t actual=((uint8_t*)ctx->dst.v)[ctx->dst.c-paylen];
    if (actual!=paylen-1) {
      fprintf(stderr,"%s:%d: Incorrect payload length. You said %d, but remaining payload is %d bytes long.\n",ctx->path,ctx->lineno,actual,paylen-1);
      return -2;
    }
  } else {
    // Opcodes 0xe0..0xff have no generic length. Let anything pass.
  }
  #undef EXPECT
  
  return 0;
}

/* Receive one line of text.
 */
 
static int mkdata_map_line(struct mkdata_map_ctx *ctx,const char *src,int srcc) {

  // Trim the trailing newline, but nothing else.
  if (srcc&&(src[srcc-1]==0x0a)) srcc--;
  if (srcc&&(src[srcc-1]==0x0d)) srcc--;
  
  // A completely empty line is always legal, and never meaningful.
  if (!srcc) return 0;
  
  // Check for section headers.
  if ((srcc==6)&&!memcmp(src,"LEGEND",6)) return mkdata_map_begin_LEGEND(ctx);
  if ((srcc==3)&&!memcmp(src,"MAP",3)) return mkdata_map_begin_MAP(ctx);
  if ((srcc==8)&&!memcmp(src,"COMMANDS",8)) return mkdata_map_begin_COMMANDS(ctx);
  
  switch (ctx->section) {
    case MKDATA_MAP_SECTION_UNSET: {
        fprintf(stderr,"%s:%d: Expected section header ('LEGEND', 'MAP', or 'COMMANDS')\n",ctx->path,ctx->lineno);
        return -2;
      }
    case MKDATA_MAP_SECTION_LEGEND: return mkdata_map_line_LEGEND(ctx,src,srcc);
    case MKDATA_MAP_SECTION_MAP: return mkdata_map_line_MAP(ctx,src,srcc);
    case MKDATA_MAP_SECTION_COMMANDS: return mkdata_map_line_COMMANDS(ctx,src,srcc);
    default: return -1;
  }
}

/* Finish compilation.
 */
 
static int mkdata_map_finish(struct mkdata_map_ctx *ctx) {

  uint8_t colc=ctx->dst.v[0];
  uint8_t rowc=ctx->dst.v[1];
  if (!colc||!rowc) {
    // I guess size zero is technically legal. Should we allow it?
    fprintf(stderr,"%s:ERROR: No content.\n",ctx->path);
    return -2;
  }
  
  // Append the commands terminator.
  if (sr_encode_raw(&ctx->dst,"\0",1)<0) return -1;

  return 0;
}

/* Main entry point.
 */

int mkdata_mapbin_from_map(void *dst,const void *src,int srcc,const char *path) {

  struct mkdata_map_ctx ctx={
    .path=path,
    .lineno=0,
  };
  // Emit the dimensions first, and we'll read and update that as we go.
  if (sr_encode_raw(&ctx.dst,"\0\0",2)<0) {
    mkdata_map_ctx_cleanup(&ctx);
    return -1;
  }
  
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line=0;
  int linec=0;
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    ctx.lineno++;
    int err=mkdata_map_line(&ctx,line,linec);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error compiling map.\n",path,ctx.lineno);
      mkdata_map_ctx_cleanup(&ctx);
      return -2;
    }
  }
  int err=mkdata_map_finish(&ctx);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error finishing map compilation.\n",path);
    mkdata_map_ctx_cleanup(&ctx);
    return -2;
  }
  int dstc=ctx.dst.c;
  *(void**)dst=ctx.dst.v;
  ctx.dst.v=0;
  mkdata_map_ctx_cleanup(&ctx);
  return dstc;
}
