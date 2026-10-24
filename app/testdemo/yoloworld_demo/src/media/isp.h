#pragma once

// fake cam id
int rk_isp_init(int cam_id, const char *iqfile_path);
int rk_isp_deinit(int cam_id);
void *rk_isp_get_ctx(int cam_id);
