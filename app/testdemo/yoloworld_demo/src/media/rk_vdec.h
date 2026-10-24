#pragma once

#include "rk_comm_mb.h"
#include "rk_common.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_vdec.h"

int rk_vdec_init(int chn, RK_CODEC_ID_E enCodecId, int width, int height);
