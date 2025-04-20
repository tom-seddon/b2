#include "enums.h"

#if EINTERNAL
#define EPREFIX static
#else
#define EPREFIX
#endif

#define EBEGIN__BODY(COLON_BASE_TYPE, BASE_TYPE)                               \
    enum ENAME COLON_BASE_TYPE;                                                \
                                                                               \
    template <>                                                                \
    struct EnumBaseType<ENAME> {                                               \
        typedef BASE_TYPE Type;                                                \
    };                                                                         \
                                                                               \
    EPREFIX const char *UNUSED CONCAT3(Get, ENAME, EnumName)(BASE_TYPE value); \
                                                                               \
    enum ENAME COLON_BASE_TYPE {

#define EBEGIN() EBEGIN__BODY(, int)
#define EBEGIN_DERIVED(BASE_NAME) EBEGIN__BODY( \
    : BASE_NAME, BASE_NAME)

#define EN(NAME) NAME,
#define ENV(NAME, VALUE) NAME = (VALUE),
#define EPN(NAME) EN(CONCAT3(ENAME, _, NAME))
#define EPNV(NAME, VALUE) ENV(CONCAT3(ENAME, _, NAME), VALUE)

#define EQN(NAME) EN(NAME)
#define EQNV(NAME, VALUE) ENV(NAME, VALUE)
#define EQPN(NAME) EPN(NAME)
#define EQPNV(NAME, VALUE) EPNV(NAME, VALUE)

#define EEND()                                                                \
    }                                                                         \
    ;                                                                         \
    typedef enum ENAME ENAME;                                                 \
                                                                              \
    template <>                                                               \
    struct EnumTraits<ENAME> {                                                \
        typedef const char *(*GetNameFn)(typename EnumBaseType<ENAME>::Type); \
        static const GetNameFn GET_NAME_FN;                                   \
        static const char NAME[];                                             \
    };

#define NBEGIN(NAME) EPREFIX const char *UNUSED CONCAT3(Get, NAME, EnumName)(ENAME value);
#define NEND()
#define NN(NAME)
