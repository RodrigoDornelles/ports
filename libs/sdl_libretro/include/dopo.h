#ifndef DOPO_H
#define DOPO_H

#define DOPO_NAME       "dopo"
#define DOPO_ENV_PREFIX "DOPO"
#define DOPO_DRIVER    DOPO_NAME
#define DOPO_SHIM_ID   DOPO_NAME "-ipc-shim"

#define DOPO_SOCK_FMT  "/tmp/" DOPO_NAME "-%d-%u.sock"
#define DOPO_SHM_FMT   "/" DOPO_NAME "-fb-%d"

#endif
