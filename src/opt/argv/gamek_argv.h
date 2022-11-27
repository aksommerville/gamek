/* gamek_argv.h
 * Helpers for uniform processing of argv.
 * All PC platforms should use this.
 */
 
#ifndef GAMEK_ARGV_H
#define GAMEK_ARGV_H

/* Usually argv[0], but if that's missing try some other things.
 * Never returns empty or null.
 */
const char *gamek_argv_exename(int argc,char **argv);

/* Trigger (cb) for each option, or until it returns nonzero.
 * We blindly skip (argv[0]).
 * Empty or null entries are skipped; you can preprocess things that way.
 * Short options: "-KVV" "-K VV" "-K". Not combinable.
 * Long options: "--KK=VV" "--KK VV" "--KK" "--no-KK"
 * Anything else is positional, we report as (v) with empty (k).
 * (vn) is (v) evaluated as integer, or zero if it failed.
 */
int gamek_argv_read(
  int argc,char **argv, // Exactly as supplied to main().
  int (*cb)(const char *k,int kc,const char *v,int vc,int vn,void *userdata),
  void *userdata
);

#endif
