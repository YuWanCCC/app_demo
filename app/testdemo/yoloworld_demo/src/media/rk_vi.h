#pragma once

#include "rk_comm_mb.h"
#include "rk_common.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_vi.h"

int rk_vi_init(int dev_id, int chn_id, int width, int height, int is_usbcam);
int rk_vi_deinit(int dev_id, int chn_id);
