#include "gamek_argv.h"
#include "pf/gamek_pf.h"
#include <string.h>
#include <stdio.h>

/* exename.
 */

const char *gamek_argv_exename(int argc,char **argv) {
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) return argv[0];
  if (gamek_client.title&&gamek_client.title[0]) return gamek_client.title;
  return "gamek";
}

/* Process a split field.
 */
 
static int gamek_argv_kv(
  const char *k,int kc,
  const char *v,int vc,
  int (*cb)(const char *k,int kc,const char *v,int vc,int vn,void *userdata),
  void *userdata
) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  
  int vn=0;
  if (!vc) {
    vn=1;
  } else {
    if ((v[0]=='-')&&(vc>=2)) {
      int i=1;
      for (;i<vc;i++) {
        int digit=v[i]-'0';
        if ((digit<0)||(digit>9)) { vn=0; break; }
        vn*=10;
        vn-=digit;
      }
    } else {
      int i=0;
      for (;i<vc;i++) {
        int digit=v[i]-'0';
        if ((digit<0)||(digit>9)) { vn=0; break; }
        vn*=10;
        vn+=digit;
      }
    }
  }
  
  return cb(k,kc,v,vc,vn,userdata);
}

/* Read argv, main entry point.
 */
 
int gamek_argv_read(
  int argc,char **argv,
  int (*cb)(const char *k,int kc,const char *v,int vc,int vn,void *userdata),
  void *userdata
) {
  if (!cb) return -1;
  if (argc<2) return 0;
  int argi=1,err;
  while (argi<argc) {
    const char *arg=argv[argi++];
    
    // Null or empty is ignored.
    if (!arg||!arg[0]) continue;
    
    // No leading dash? Positional.
    if (arg[0]!='-') {
      if (err=gamek_argv_kv(0,0,arg,-1,cb,userdata)) return err;
      continue;
    }
    
    // Leading dash alone? Not yet defined, so call it positional.
    if (!arg[1]) {
      if (err=gamek_argv_kv(0,0,arg,-1,cb,userdata)) return err;
      continue;
    }
    
    // Single leading dash? Short option.
    if (arg[1]!='-') {
      char k=arg[1];
      const char *v=0;
      if (arg[2]) v=arg+2;
      else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
      if (err=gamek_argv_kv(&k,1,v,-1,cb,userdata)) return err;
      continue;
    }
    
    // Strip all leading dashes. (technically we allow more than two)
    // If empty, that's undefined, so call it positional.
    int dashc=2;
    while (arg[dashc]=='-') dashc++;
    if (!arg[dashc]) {
      if (err=gamek_argv_kv(0,0,arg,dashc,cb,userdata)) return err;
      continue;
    }
    arg+=dashc;
    
    // Long option.
    int kc=0;
    while (arg[kc]&&(arg[kc]!='=')) kc++;
    const char *v=0;
    if (arg[kc]=='=') v=arg+kc+1;
    else if ((kc>3)&&!memcmp(arg,"no-",3)) { arg+=3; kc-=3; v="0"; }
    else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
    if (err=gamek_argv_kv(arg,kc,v,-1,cb,userdata)) return err;
  }
  return 0;
}
