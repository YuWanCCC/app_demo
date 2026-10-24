// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef __SAMPLE_COMM_AOV_H__
#define __SAMPLE_COMM_AOV_H__

#include <stdint.h>
#include <stdbool.h>

int SAMPLE_COMM_AOV_BindSdcard();
int SAMPLE_COMM_AOV_UnbindSdcard();
int SAMPLE_COMM_AOV_BindEthernet();
int SAMPLE_COMM_AOV_UnbindEthernet();
int SAMPLE_COMM_AOV_BindSoundcard();
int SAMPLE_COMM_AOV_UnbindSoundcard();
int SAMPLE_COMM_AOV_BindEmmc();
int SAMPLE_COMM_AOV_UnbindEmmc();

int SAMPLE_COMM_AOV_DisableNonBootCPUs();
int SAMPLE_COMM_AOV_EnableNonBootCPUs();

int SAMPLE_COMM_AOV_BindUsb();
int SAMPLE_COMM_AOV_UnbindUsb();

int SAMPLE_COMM_AOV_UnbindSDIO();
int SAMPLE_COMM_AOV_BindSDIO();

#endif // #ifndef __SAMPLE_COMM_AOV_H__
