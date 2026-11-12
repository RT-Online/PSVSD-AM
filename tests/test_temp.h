#ifndef AM_TEST_TEMP_H
#define AM_TEST_TEMP_H
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void test_temp_dir(char *path,size_t capacity,const char *name) {
    const char *parent=getenv("TMPDIR");
    if (!parent || !*parent) parent="/tmp";
    int length=snprintf(path,capacity,"%s/%s-XXXXXX",parent,name);
    assert(length>0 && (size_t)length<capacity);
    assert(mkdtemp(path));
}
#endif
