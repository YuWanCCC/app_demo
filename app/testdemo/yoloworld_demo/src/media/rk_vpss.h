#pragma once

#include "rk_comm_mb.h"
#include "rk_common.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_vpss.h"

int rk_vpss_init(int width, int height, int src_fps, int dst_fps);
int rk_vpss_deinit();
