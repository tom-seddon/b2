#include "structs.h"

#define STRUCT_BEGIN() struct STRUCT_NAME {

#define STRUCT_FIELD(FIELD_NAME, ...) \
    STRUCT__FIELD(FIELD_NAME, __VA_ARGS__)

#define STRUCT_OPTIONAL_FIELD(FIELD_NAME, ...) \
    STRUCT__FIELD(FIELD_NAME, __VA_ARGS__)

#define STRUCT_END() \
    }                \
    ;

#define STRUCT__FIELD(FIELD_NAME, ...) \
    __VA_ARGS__ FIELD_NAME;
