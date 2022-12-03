#include "akdrm_internal.h"
#include <dirent.h>

/* Iterate resources.
 */
 
static int akdrm_iterate_resources(
  const char *path,
  int fd,
  drmModeResPtr res,
  int (*cb)(struct akdrm_config *config,void *userdata),
  void *userdata
) {
  struct akdrm_config config={
    .path=(char*)path,
  };
  int conni=0,err;
  for (;conni<res->count_connectors;conni++) {
    drmModeConnectorPtr connq=drmModeGetConnector(fd,res->connectors[conni]);
    if (!connq) continue;
    
    config.connector_id=connq->connector_id;
    
    drmModeModeInfoPtr modeq=connq->modes;
    int modei=0;
    for (;modei<connq->count_modes;modei++,modeq++) {
      config.rate=modeq->vrefresh;
      config.w=modeq->hdisplay;
      config.h=modeq->vdisplay;
      if (modeq->type&DRM_MODE_TYPE_PREFERRED) config.current=1;
      else config.current=0;
      config.mode_index=modei;
      config.mode_name=modeq->name;
      config.flags=modeq->flags;
      config.type=modeq->type;
      
      if (err=cb(&config,userdata)) break;
    }
    
    drmModeFreeConnector(connq);
    if (err) return err;
  }
  return 0;
}

/* List configurations for one device file.
 */
 
static int akdrm_list_configs_in_file(
  const char *path,
  int (*cb)(struct akdrm_config *config,void *userdata),
  void *userdata
) {
  int fd=open(path,O_RDONLY);
  if (fd<0) return 0;
  
  drmModeResPtr res=drmModeGetResources(fd);
  if (!res) {
    fprintf(stderr,"%s:drmModeGetResources: %m\n",path);
    return -1;
  }
  
  int err=akdrm_iterate_resources(path,fd,res,cb,userdata);
  drmModeFreeResources(res);
  close(fd);
  return err;
}

/* List devices, directory iterator.
 */
 
int akdrm_list_devices(
  const char *path, // null for default "/dev/dri"
  int (*cb)(struct akdrm_config *config,void *userdata),
  void *userdata
) {
  if (!cb) return -1;
  
  if (!path||!path[0]) path="/dev/dri";
  DIR *dir=opendir(path);
  if (!dir) return -1;
  char subpath[1024];
  int pathc=0; while (path[pathc]) pathc++;
  if (pathc>=sizeof(subpath)) {
    closedir(dir);
    return -1;
  }
  memcpy(subpath,path,pathc);
  if (path[pathc-1]!='/') subpath[pathc++]='/';
  
  struct dirent *de;
  while (de=readdir(dir)) {
    if (de->d_type!=DT_CHR) continue;
    const char *base=de->d_name;
    int basec=0;
    while (base[basec]) basec++;
    if ((basec<5)||memcmp(base,"card",4)) continue;
    if (pathc>=sizeof(subpath)-basec) continue;
    int cardid=0,basep=4;
    for (;basep<basec;basep++) {
      int digit=base[basep]-'0';
      if ((digit<0)||(digit>9)||(cardid>9999)) {
        cardid=-1;
        break;
      }
      cardid*=10;
      cardid+=digit;
    }
    if (cardid<0) continue;
    memcpy(subpath+pathc,base,basec+1);
    int err=akdrm_list_configs_in_file(subpath,cb,userdata);
    if (err) {
      closedir(dir);
      return err;
    }
  }
  closedir(dir);
  return 0;
}

/* Find best device: context.
 */
 
struct akdrm_find_device_ctx {
  struct akdrm_config *best;
  int rate;
  int w,h;
  int fbw,fbh;
};

/* Compare device to request.
 * Zero to reject it entirely, nonzero to proceed.
 */

static int akdrm_find_device_qualify(
  const struct akdrm_find_device_ctx *ctx,
  const struct akdrm_config *config
) {

  // Sanity checks
  if (!config->path) return 0;
  if (config->rate<AKDRM_RATE_MIN) return 0;
  if (config->rate>AKDRM_RATE_MAX) return 0;
  if (config->w<AKDRM_SIZE_MIN) return 0;
  if (config->w>AKDRM_SIZE_MAX) return 0;
  if (config->h<AKDRM_SIZE_MIN) return 0;
  if (config->h>AKDRM_SIZE_MAX) return 0;
  
  // Reject interlaced and stereo modes.
  if (config->flags&DRM_MODE_FLAG_INTERLACE) return 0;
  if (config->flags&DRM_MODE_FLAG_3D_MASK) return 0;

  return 1;
}

/* Compare two configs.
 * (a) can be full zeroes.
 */
 
static int akdrm_find_device_compare(
  const struct akdrm_config *a,
  const struct akdrm_config *b,
  const struct akdrm_find_device_ctx *ctx
) {

  // If (a) is unset, prefer whatever (b) is.
  if (!a->path) return 1;
  
  // When rate is specified, assume it's important.
  // An exact match, when the other doesn't match, end of discussion.
  double rateq=0.0;
  if (ctx->rate&&(a->rate!=b->rate)) {
    if ((a->rate==ctx->rate)&&(b->rate!=ctx->rate)) return -1;
    if ((a->rate!=ctx->rate)&&(b->rate==ctx->rate)) return 1;
    int da=a->rate-ctx->rate; if (da<0) da=-da;
    int db=b->rate-ctx->rate; if (db<0) db=-db;
    rateq=(double)(da-db)/(double)ctx->rate;
    if (rateq<-1.0) rateq=-1.0;
    else if (rateq>1.0) rateq=1.0;
  }
  
  // If they requested a particular size, again, stop at exact match.
  // Otherwise establish a preference by adding width and height.
  double sizeq=0.0;
  if (ctx->w&&ctx->h&&((a->w!=b->w)||(a->h!=b->h))) {
    int amatch=((a->w==ctx->w)&&(a->h==ctx->h));
    int bmatch=((b->w==ctx->w)&&(b->h==ctx->h));
    if (amatch&&!bmatch) return -1;
    if (!amatch&&bmatch) return 1;
    int asum=a->w+a->h,bsum=b->w+b->h,csum=ctx->w+ctx->h;
    int da=asum-csum; if (da<0) da=-da;
    int db=bsum-csum; if (db<0) db=-db;
    sizeq=(double)(da-db)/(double)csum;
    if (sizeq<-1.0) sizeq=-1.0;
    else if (sizeq>1.0) sizeq=1.0;
  }
  
  // If a framebuffer size was provided, compare aspect ratios.
  double aspq=0.0;
  if (ctx->fbw&&ctx->fbh&&((a->w!=b->w)||(a->h!=b->h))) {
    double aspa=(double)a->w/(double)a->h;
    double aspb=(double)b->w/(double)b->h;
    double aspc=(double)ctx->fbw/(double)ctx->fbh;
    double pa=(aspa<aspc)?(aspa/aspc):(aspc/aspa);
    double pb=(aspb<aspc)?(aspb/aspc):(aspc/aspb);
    aspq=pb-pa;
  }
  
  // (rateq,sizeq,aspq) are all theoretically in -1..1, though in practice they should be very close to zero.
  // Multiply each by some opinionated weight, add them up, and check which side of zero it falls on.
  double score=
    rateq*0.900+
    sizeq*0.500+
    aspq *0.700;
  if (score<0.0) return -1;
  if (score>0.0) return 1;
  
  // All else being equal, if one has the PREFERRED flag, prefer it.
  // (this means it's the one already enabled).
  int aprefer=(a->type&DRM_MODE_TYPE_PREFERRED);
  int bprefer=(b->type&DRM_MODE_TYPE_PREFERRED);
  if (aprefer&&!bprefer) return -1;
  if (!aprefer&&bprefer) return 1;

  return 0;
}

/* Overwrite one config with another.
 */
 
static int akdrm_config_replace(
  struct akdrm_config *dst,
  const struct akdrm_config *src
) {
  akdrm_config_cleanup(dst);
  memcpy(dst,src,sizeof(struct akdrm_config));
  int err=0;
  if (src->path&&!(dst->path=strdup(src->path))) err=-1;
  if (src->mode_name&&!(dst->mode_name=strdup(src->mode_name))) err=-1;
  return err;
}

/* Find device: iterate.
 */

static int akdrm_find_device_cb(struct akdrm_config *config,void *userdata) {
  struct akdrm_find_device_ctx *ctx=userdata;
  
  if (!akdrm_find_device_qualify(ctx,config)) {
    return 0;
  }
  
  if (akdrm_find_device_compare(ctx->best,config,ctx)>0) {
    if (akdrm_config_replace(ctx->best,config)<0) return -1;
    return 0;
  }
  
  return 0;
}

int akdrm_find_device(
  struct akdrm_config *config,
  const char *path, // usually null
  int rate, // preferred refresh rate in hz, or 0
  int w,int h, // preferred absolute size in pixels, or 0
  int fbw,int fbh // size of your framebuffer, to guide aspect comparison, or 0
) {
  if (!config) return -1;
  memset(config,0,sizeof(struct akdrm_config));
  struct akdrm_find_device_ctx ctx={
    .best=config,
    .rate=rate,
    .w=w,
    .h=h,
    .fbw=fbw,
    .fbh=fbh,
  };
  akdrm_list_devices(path,akdrm_find_device_cb,&ctx);
  if (!config->path) return -1;
  return 0;
}
