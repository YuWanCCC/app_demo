#pragma once

#include "rk_comm_mb.h"
#include "rk_common.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_rgn.h"
#include "rk_mpi_sys.h"

int rk_rgn_init(int width, int height);
int rk_rgn_deinit();
