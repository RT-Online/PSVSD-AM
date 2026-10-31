#ifndef ACCESSORY_SESSION_H
#define ACCESSORY_SESSION_H
#include <stddef.h>
#include <stdint.h>
#include "../common/paths.h"
typedef struct {
    int attempted, started;
    int stage_result, load_result, start_result, entry_result;
    int log_result;
    uint32_t request_id;
    size_t used, kernel_offset;
    char text[24576];
} AccessorySession;
void accessory_session_run(AccessorySession *s);
void accessory_session_refresh(AccessorySession *s);
void accessory_session_clear_log(AccessorySession *s);
typedef struct { int load,start,entry,unload; } AccessoryQuery;
int accessory_status_query(uint32_t request_id,AccessoryQuery *result);
#endif
