#ifndef __UI_DETECT_TYPE_H_
#define __UI_DETECT_TYPE_H_

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*detect_type_operation_callback)( void *ctx, char *classname);

void set_detect_type_add_class_callback(detect_type_operation_callback func);
void set_detect_type_delete_class_callback(detect_type_operation_callback func);

#ifdef __cplusplus
}
#endif

#endif