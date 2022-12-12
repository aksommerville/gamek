#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common/gamek_image.h"
#include "opt/fs/gamek_fs.h"
#include "opt/argv/gamek_argv.h"

// For multibyte unit types, (src) must be in the native byte order.
int mkdata_encode_c(
  void *dst,
  const void *src,int srcc,
  int unit_type, // eg -8=int8_t, 8=uint8_t, ...
  const char *name
);

int mkdata_font_from_png(void *dst,const void *src,int srcc,const char *path);
int mkdata_image_from_png(void *dst,const void *src,int srcc,const char *path,int imgfmt);
int mkdata_song_from_midi(void *dst,const void *src,int srcc,const char *path);

/* Data formats.
 */

#define MKDATA_FORMAT_UNSPEC     0 /* Infer from other parameters. */
#define MKDATA_FORMAT_binary     1 /* Take whatever the prior step gives us. */
#define MKDATA_FORMAT_c          2
#define MKDATA_FORMAT_png        3
#define MKDATA_FORMAT_midi       4
#define MKDATA_FORMAT_wav        5
#define MKDATA_FORMAT_font       6
#define MKDATA_FORMAT_image      7
#define MKDATA_FORMAT_song       8

#define MKDATA_FOR_EACH_FORMAT \
  _(binary) \
  _(c) \
  _(png) \
  _(midi) \
  _(wav) \
  _(font) \
  _(image) \
  _(song)
  
static const char *mkdata_format_repr(int format) {
  switch (format) {
    #define _(tag) case MKDATA_FORMAT_##tag: return #tag;
    MKDATA_FOR_EACH_FORMAT
    #undef _
  }
  return "?";
}

static int mkdata_format_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(src,#tag,srcc)) return MKDATA_FORMAT_##tag;
  MKDATA_FOR_EACH_FORMAT
  #undef _
  return -1;
}

/* Other symbols.
 */
 
static int gamek_imgfmt_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(src,#tag,srcc)) return GAMEK_IMGFMT_##tag;
  GAMEK_FOR_EACH_IMGFMT
  #undef _
  
  // Aliases
  if ((srcc==4)&&!memcmp(src,"RGBA",4)) return GAMEK_IMGFMT_RGBX;
  if ((srcc==4)&&!memcmp(src,"TINY",4)) return GAMEK_IMGFMT_BGR332;
  
  return -1;
}

/* Globals.
 */

static const char *exename="mkdata";
static char *srcpath=0;
static char *dstpath=0;
static int srcfmt=MKDATA_FORMAT_UNSPEC; // Usually (binary,png,midi,wav)
static int midfmt=MKDATA_FORMAT_UNSPEC; // Usually (font,image,song)
static int dstfmt=MKDATA_FORMAT_UNSPEC; // Usually (c)
static int unit_size=8; // -64,-32,-16,-8,8,16,32,64; size of primitive C type, if outputting C.
static char *name=0; // name of C object, if outputting C.
static int imgfmt=GAMEK_IMGFMT_RGBX;

/* --help
 */
 
static void mkdata_print_help() {
  fprintf(stderr,"\nUsage: %s -oOUTPUT -iINPUT [OPTIONS...]\n",exename);
  fprintf(stderr,
    "\n"
    "OPTIONS:\n"
    "  --help            Print this message.\n"
    "  --srcfmt=FORMAT   Input file format, override detection.\n"
    "  --midfmt=FORMAT   Intermediate format, override assumption.\n"
    "  --dstfmt=FORMAT   Output file format, override assumption.\n"
    "  --unit-size=INT   C unit size (8,16,32,64, negative for signed), default 8.\n"
    "  --name=STRING     Name of C object, otherwise inferred from file name.\n"
    "  --imgfmt=NAME     Gamek image format. RGBX BGR332\n"
    "\n"
    "FORMAT:\n"
    "  binary         Raw data, implies no conversion.\n"
    "  c              C code. Output.\n"
    "  png            PNG image. Input.\n"
    "  midi           MIDI song. Input.\n"
    "  wav            WAV sound. Input.\n"
    "  font           Gamek font, input should be an image. Intermediate.\n"
    "  image          Gamek image. Intermediate.\n"
    "  song           Gamek song. Intermediate.\n"
    "\n"
    "Options are usually inferred from input and output paths.\n"
    "\n"
  );
}

/* Receive argument.
 */
 
static int cb_arg(const char *k,int kc,const char *v,int vc,int vn,void *userdata) {

  if ((kc==4)&&!memcmp(k,"help",4)) {
    mkdata_print_help();
    return 1;
  }
  
  if ((kc==1)&&(k[0]=='o')) {
    if (dstpath) {
      fprintf(stderr,"%s: Multiple output paths.\n",exename);
      return -1;
    }
    if (!(dstpath=malloc(vc+1))) return -1;
    memcpy(dstpath,v,vc);
    dstpath[vc]=0;
    return 0;
  }
  
  if ((kc==1)&&(k[0]=='i')) {
    if (srcpath) {
      fprintf(stderr,"%s: Multiple input paths.\n",exename);
      return -1;
    }
    if (!(srcpath=malloc(vc+1))) return -1;
    memcpy(srcpath,v,vc);
    srcpath[vc]=0;
    return 0;
  }
  
  if ((kc==9)&&!memcmp(k,"unit-size",9)) {
    switch (vn) {
      case -64: case -32: case -16: case -8:
      case  64: case  32: case  16: case  8:
        break;
      default: {
          fprintf(stderr,"%s: unit-size must be one of: -64,-32,-16,-8,8,16,32,64\n",exename);
          return -1;
        }
    }
    unit_size=vn;
    return 0;
  }
  
  if ((kc==4)&&!memcmp(k,"name",4)) {
    if (name) {
      fprintf(stderr,"%s: Multiple names.\n",exename);
      return -1;
    }
    if (vc) {
      if (!(name=malloc(vc+1))) return -1;
      memcpy(name,v,vc);
      name[vc]=0;
    }
    return 0;
  }
  
  if ((kc==6)&&!memcmp(k,"imgfmt",6)) {
    if ((imgfmt=gamek_imgfmt_eval(v,vc))<0) {
      fprintf(stderr,"%s: Unknown image format '%.*s'\n",exename,vc,v);
      return -1;
    }
    return 0;
  }

  fprintf(stderr,"%s:WARNING: Ignoring unexpected option '%.*s'='%.*s'\n",exename,kc,k,vc,v);
  return 0;
}

/* Guess data format from path or content.
 */
 
static int mkdata_guess_format_from_suffix(const char *path) {
  if (!path) return -1;
  char sfx[8];
  int sfxc=0,pathp=0,reading=0;
  for (;path[pathp];pathp++) {
    if (path[pathp]=='/') {
      reading=0;
      sfxc=0;
    } else if (path[pathp]=='.') {
      reading=1;
      sfxc=0;
    } else if (!reading) {
    } else if (sfxc>=sizeof(sfx)) {
      sfxc=0;
      reading=0;
    } else if ((path[pathp]>='A')&&(path[pathp]<='Z')) {
      sfx[sfxc++]=path[pathp]+0x20;
    } else {
      sfx[sfxc++]=path[pathp];
    }
  }
  switch (sfxc) {
    case 1: switch (sfx[0]) {
        case 'c': return MKDATA_FORMAT_c;
      } break;
    case 3: {
        if (!memcmp(sfx,"png",3)) return MKDATA_FORMAT_png;
        if (!memcmp(sfx,"mid",3)) return MKDATA_FORMAT_midi;
        if (!memcmp(sfx,"wav",3)) return MKDATA_FORMAT_wav;
      } break;
  }
  return -1;
}
 
static int mkdata_guess_format_from_directories(const char *path) {
  if (!path) return -1;
  int pathp=0,fmt=-1;
  while (path[pathp]) {
    if (path[pathp]=='/') { pathp++; continue; }
    const char *dir=path+pathp;
    int dirc=0;
    while (path[pathp]&&(path[pathp++]!='/')) dirc++;
    int qfmt=mkdata_format_eval(dir,dirc);
    if (qfmt>0) fmt=qfmt; // Don't return it yet; we want the rightmost if there's more than one.
  }
  return fmt;
}

static int mkdata_guess_format_from_content(const void *src,int srcc) {
  if (!src) return -1;
  if ((srcc>=8)&&!memcmp(src,"\x89PNG\r\n\x1a\n",8)) return MKDATA_FORMAT_png;
  if ((srcc>=8)&&!memcmp(src,"MThd\0\0\0\6",8)) return MKDATA_FORMAT_midi;
  if ((srcc>=4)&&!memcmp(src,"RIFF",4)) return MKDATA_FORMAT_wav;
  return -1;
}

/* Set (srcfmt,midfmt,dstfmt) from paths and input.
 * Logs all errors.
 */
 
static int mkdata_resolve_formats(const void *src,int srcc) {

  if (srcfmt==MKDATA_FORMAT_UNSPEC) {
    if ((srcfmt=mkdata_guess_format_from_content(src,srcc))>0) {
      // A file's signature is the best clue.
    } else if ((srcfmt=mkdata_guess_format_from_suffix(srcpath))>0) {
      // Trust the suffix.
    } else {
      // One of those really ought to have matched, don't try anything else.
      fprintf(stderr,"%s: Unable to guess input format.\n",srcpath);
      return -2;
    }
  }
  
  if (dstfmt==MKDATA_FORMAT_UNSPEC) {
    if ((dstfmt=mkdata_guess_format_from_suffix(dstpath))>0) {
      // Usual case, determined by output path suffix.
    } else {
      // "binary" is always valid, so go with it.
      dstfmt=MKDATA_FORMAT_binary;
    }
  }
  
  if (midfmt==MKDATA_FORMAT_UNSPEC) {
    if ((midfmt=mkdata_guess_format_from_directories(srcpath))>0) {
      // Usually data files come from something like "src/data/font/my-file.png"; we know midfmt from "font".
    } else switch (srcfmt) {
      // Anything else, we can make a reasonable guess from the input format.
      case MKDATA_FORMAT_binary: midfmt=MKDATA_FORMAT_binary; break;
      case MKDATA_FORMAT_png: midfmt=MKDATA_FORMAT_image; break;
      case MKDATA_FORMAT_midi: midfmt=MKDATA_FORMAT_song; break;
      case MKDATA_FORMAT_wav: midfmt=MKDATA_FORMAT_binary; break;//TODO gamek sound format
      // The rest are unusual as input. Pass them thru verbatim. (font,image,song,c)
      default: midfmt=MKDATA_FORMAT_binary;
    }
  }
  
  return 0;
}

/* Make up a C object name from one of the paths.
 */
 
static char *mkdata_synthesize_object_name_1(const char *src) {
  if (!src) return 0;
  const char *base=src;
  int basec=0,reading=1;
  for (;*src;src++) {
    if (*src=='/') {
      base=src+1;
      basec=0;
      reading=1;
    } else if ((*src=='.')||(*src=='-')) {
      reading=0;
    } else if (reading) {
      basec++;
    }
  }
  if (basec<1) return 0;
  char *name=malloc(basec+1);
  if (!name) return 0;
  memcpy(name,base,basec);
  name[basec]=0;
  return name;
}
 
static char *mkdata_synthesize_object_name() {
  char *v;
  if (!(v=mkdata_synthesize_object_name_1(srcpath))) {
    v=mkdata_synthesize_object_name_1(dstpath);
  }
  return v;
}

/* Convert data in memory.
 */
 
static int mkdata_convert(void *dst,int dstfmt,const void *src,int srcc,int srcfmt) {
  
  // Same format, or "binary" output, just copy it.
  if ((dstfmt==srcfmt)||(dstfmt==MKDATA_FORMAT_binary)) {
    if (!(dst=malloc(srcc?srcc:1))) return -1;
    memcpy(dst,src,srcc);
    return srcc;
  }
  
  // "c" as output is agnostic towards input format.
  if (dstfmt==MKDATA_FORMAT_c) {
    if (!name) {
      if (!(name=mkdata_synthesize_object_name())) {
        fprintf(stderr,"%s: Failed to synthesize object name.\n",exename);
        return -2;
      }
    }
    return mkdata_encode_c(dst,src,srcc,unit_size,name);
  }
  
  // The rest, the useful ones, are all intermediates from inputs.
  if ((dstfmt==MKDATA_FORMAT_font)&&(srcfmt==MKDATA_FORMAT_png)) return mkdata_font_from_png(dst,src,srcc,srcpath);
  if ((dstfmt==MKDATA_FORMAT_image)&&(srcfmt==MKDATA_FORMAT_png)) return mkdata_image_from_png(dst,src,srcc,srcpath,imgfmt);
  if ((dstfmt==MKDATA_FORMAT_song)&&(srcfmt==MKDATA_FORMAT_midi)) return mkdata_song_from_midi(dst,src,srcc,srcpath);
  
  fprintf(stderr,"%s: No known conversion to '%s' from '%s'\n",srcpath,mkdata_format_repr(dstfmt),mkdata_format_repr(srcfmt));
  return -2;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  
  exename=gamek_argv_exename(argc,argv);
  int err=gamek_argv_read(argc,argv,cb_arg,0);
  if (err) return (err<0)?1:0;
  if (!srcpath||!dstpath) { mkdata_print_help(); return 1; }
  
  void *src=0;
  int srcc=gamek_file_read(&src,srcpath);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",srcpath);
    return 1;
  }
  
  if (mkdata_resolve_formats(src,srcc)<0) return 1;
  
  void *mid=0;
  int midc=mkdata_convert(&mid,midfmt,src,srcc,srcfmt);
  if (midc<0) {
    if (midc!=-2) fprintf(stderr,"%s: Failed to reencode data.\n",srcpath);
    return 1;
  }
  
  void *dst=0;
  int dstc=mkdata_convert(&dst,dstfmt,mid,midc,midfmt);
  if (dstc<0) {
    if (dstc!=-2) fprintf(stderr,"%s: Failed to encode to output format.\n",srcpath);
    return 1;
  }
  
  if (gamek_file_write(dstpath,dst,dstc)<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes.\n",dstpath,dstc);
    return 1;
  }
  
  return 0;
}
