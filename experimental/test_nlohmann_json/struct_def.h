#define STRUCT_BEGIN() \
    const Struct *GetStruct(const STRUCT_NAME *) {

#define STRUCT_FIELD(...)

#define STRUCT_END() \
    return nullptr;  \
    }
