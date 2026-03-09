#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/keys.h>

#include <shared/enum_def.h>
#include <beeb/keys.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int IsNumericKeypadKey(BeebKey beeb_key) {
    ASSERT((int)beeb_key >= 0 && (int)beeb_key < 128);

    if (beeb_key < 0) {
        return 0;
    }

    int8_t column = beeb_key & 0xf;

    if (column >= 10 && column <= 14) {
        return true;
    } else {
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ELECTRON_KEYS(KEY) \
    KEY(Right)             \
    KEY(Copy)              \
    KEY(Space)             \
    KEY(Left)              \
    KEY(Down)              \
    KEY(Return)            \
    KEY(Delete)            \
    KEY(Minus)             \
    KEY(Up)                \
    KEY(Colon)             \
    KEY(0)                 \
    KEY(P)                 \
    KEY(Semicolon)         \
    KEY(Slash)             \
    KEY(9)                 \
    KEY(O)                 \
    KEY(L)                 \
    KEY(Stop)              \
    KEY(8)                 \
    KEY(I)                 \
    KEY(K)                 \
    KEY(Comma)             \
    KEY(7)                 \
    KEY(U)                 \
    KEY(J)                 \
    KEY(M)                 \
    KEY(6)                 \
    KEY(Y)                 \
    KEY(H)                 \
    KEY(N)                 \
    KEY(5)                 \
    KEY(T)                 \
    KEY(G)                 \
    KEY(B)                 \
    KEY(4)                 \
    KEY(R)                 \
    KEY(F)                 \
    KEY(V)                 \
    KEY(3)                 \
    KEY(E)                 \
    KEY(D)                 \
    KEY(C)                 \
    KEY(2)                 \
    KEY(W)                 \
    KEY(S)                 \
    KEY(X)                 \
    KEY(1)                 \
    KEY(Q)                 \
    KEY(A)                 \
    KEY(Z)                 \
    KEY(Escape)            \
    KEY(CapsLock)          \
    KEY(Ctrl)              \
    KEY(Shift)

#define ELECTRON_KEY_FROM_BEEB_KEY(X) \
    case BeebKey_##X:                 \
        return ElectronKey_##X;

#define BEEB_KEY_FROM_ELECTRON_KEY(X) \
    case ElectronKey_##X:             \
        return BeebKey_##X;

ElectronKey GetElectronKeyFromBeebKey(BeebKey beeb_key) {
    switch (beeb_key) {
    default:
        // Beeb key with no Electron equivalent.
        ASSERT(beeb_key >= 0);
        return ElectronKey_None;

    case BeebKey_None:
        return ElectronKey_None;

        ELECTRON_KEYS(ELECTRON_KEY_FROM_BEEB_KEY)
    }
}

BeebKey GetBeebKeyFromElectronKey(ElectronKey electron_key) {
    switch (electron_key) {
    default:
        // All Electron keys have Beeb equivalents.
        ASSERT(false);
        [[fallthrough]];
    case ElectronKey_None:
        return BeebKey_None;

        ELECTRON_KEYS(BEEB_KEY_FROM_ELECTRON_KEY)
    }
}
