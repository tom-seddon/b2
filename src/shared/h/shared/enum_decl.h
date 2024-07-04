#if EINTERNAL
#define EPREFIX static
#else
#define EPREFIX
#endif

#define EBEGIN() enum ENAME {
#define EBEGIN_DERIVED(BASE_NAME) enum ENAME : BASE_NAME {

#define EN(NAME) NAME,
#define ENV(NAME, VALUE) NAME = (VALUE),
#define EPN(NAME) EN(CONCAT3(ENAME, _, NAME))
#define EPNV(NAME, VALUE) ENV(CONCAT3(ENAME, _, NAME), VALUE)

#define EQN(NAME) EN(NAME)
#define EQNV(NAME, VALUE) ENV(NAME, VALUE)
#define EQPN(NAME) EPN(NAME)
#define EQPNV(NAME, VALUE) EPNV(NAME, VALUE)

#define EEND()                \
    }                         \
    ;                         \
    typedef enum ENAME ENAME; \
    EPREFIX const char *UNUSED CONCAT3(Get, ENAME, EnumName)(int value);
//#define EOVERLOAD() const char *GetEnumValueName(ENAME value);

#define NBEGIN(NAME) EPREFIX const char *UNUSED CONCAT3(Get, NAME, EnumName)(ENAME value);
#define NEND()
#define NN(NAME)
//#define NOVERLOAD() const char *GetEnumValueName(ENAME value);
