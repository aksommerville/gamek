#include "akdrm_internal.h"

/* Initialize newly opened device, after having found the connector and mode.
 */
 
static int akdrm_apply_mode(
  struct akdrm *akdrm,
  const struct akdrm_config *config,
  const drmModeConnectorPtr connector,
  const drmModeModeInfoPtr mode
) {

  akdrm->connid=connector->connector_id;
  akdrm->encid=connector->encoder_id;
  
  drmModeEncoderPtr encoder=drmModeGetEncoder(akdrm->fd,akdrm->encid);
  if (!encoder) return -1;
  akdrm->crtcid=encoder->crtc_id;
  drmModeFreeEncoder(encoder);
  
  // Store the current CRTC so we can restore it at quit.
  if (!(akdrm->crtc_restore=drmModeGetCrtc(akdrm->fd,akdrm->crtcid))) return -1;
  
  return 0;
}

/* Having located a DRM mode by index, confirm it's the one being asked for.
 */
 
static int akdrm_is_correct_mode(
  const struct akdrm_config *config,
  const drmModeModeInfoPtr mode
) {
  if (mode->hdisplay!=config->w) return 0;
  if (mode->vdisplay!=config->h) return 0;
  if (mode->vrefresh!=config->rate) return 0;
  if (mode->flags!=config->flags) return 0;
  if (mode->type!=config->type) return 0;
  return 1;
}

/* Open device for new context.
 */
 
int akdrm_open(struct akdrm *akdrm,const struct akdrm_config *config) {
  if (!config||!config->path) return -1;
  
  if ((akdrm->fd=open(config->path,O_RDWR))<0) {
    fprintf(stderr,"%s:open: %m\n",config->path);
    return -1;
  }
  
  // Find the connector and mode. (config) identifies them.
  drmModeResPtr res=drmModeGetResources(akdrm->fd);
  if (!res) {
    fprintf(stderr,"%s:drmModeGetResources: %m\n",config->path);
    return -1;
  }
  drmModeConnectorPtr connector=0;
  int conni=0;
  for (;conni<res->count_connectors;conni++) {
    drmModeConnectorPtr q=drmModeGetConnector(akdrm->fd,res->connectors[conni]);
    if (!q) continue;
    if (q->connector_id==config->connector_id) {
      connector=q;
      break;
    }
    drmModeFreeConnector(q);
  }
  drmModeFreeResources(res);
  if (!connector) {
    return -1;
  }
  if ((config->mode_index<0)||(config->mode_index>=connector->count_modes)) {
    drmModeFreeConnector(connector);
    return -1;
  }
  drmModeModeInfoPtr mode=connector->modes+config->mode_index;
  if (!akdrm_is_correct_mode(config,mode)) {
    drmModeFreeConnector(connector);
    return -1;
  }
  if (akdrm_apply_mode(akdrm,config,connector,mode)<0) {
    drmModeFreeConnector(connector);
    return -1;
  }
  drmModeFreeConnector(connector);
  
  return 0;
}
