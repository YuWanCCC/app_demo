#pragma once

#include "rk_comm_mb.h"
#include "rk_common.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_venc.h"

int rk_venc_init(int chnId, int width, int height, RK_CODEC_ID_E enType);
int rk_venc_deinit(int chnId);
