#pragma once

#include "rk_comm_mb.h"
#include "rk_common.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_vo.h"

int rk_vo_init(int width, int height);
int rk_vo_deinit();
