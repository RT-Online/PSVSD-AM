#ifndef PSVSD_AM_PLATFORM_H
#define PSVSD_AM_PLATFORM_H
#ifdef PSVSD_AM_HOST_TEST
#include "mock_manager.h"
#else
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/io/devctl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/vshbridge.h>
#endif
#endif
