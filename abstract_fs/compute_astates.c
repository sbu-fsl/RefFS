#include "fileutil.h"

// Only calculate fuse-cpp-ramfs abstract states
#define V_N_FS 1

char *v_basepaths[V_N_FS];
const char *v_fslist[] = {"verifs2"};
absfs_state_t v_absfs[V_N_FS];

int main()
{
  for (int i = 0; i < V_N_FS; ++i) {
    size_t len = snprintf(NULL, 0, "/mnt/test-%s", v_fslist[i]);
    v_basepaths[i] = calloc(1, len + 1);
    snprintf(v_basepaths[i], len + 1, "/mnt/test-%s", v_fslist[i]);
  }

  for (int i = 0; i < V_N_FS; ++i) {
    compute_abstract_state(v_basepaths[i], v_absfs[i]);
  }

  printf("Current VeriFS2 Abstract State is: \n");
  for (int i = 0; i < 16; i++){
    printf ("%02x", v_absfs[0][i]);
  }
  putchar ('\n');

  return 0;
}