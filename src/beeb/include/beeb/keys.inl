// All BeebKey values fit into a int8_t.
//
// Valid values are between 0 and 127, with bits 0-3 being the column
// and bits 4-6 the row. Invalid values are <0.
//
// Columns 0-9 are shared between BBC and Master. Columns 10-14 are
// the Master keypad. Column 15 - never used on the original hardware
// - is used to encode special keys, which so far means Break.
//
//
//
// Rows 10-14 are
#define ENAME BeebKey
EBEGIN()
// Main keys.
EPNV(Space, 0x62)
EPNV(Comma, 0x66)
EPNV(Minus, 0x17)
EPNV(Stop, 0x67)
EPNV(Slash, 0x68)
EPNV(0, 0x27)
EPNV(1, 0x30)
EPNV(2, 0x31)
EPNV(3, 0x11)
EPNV(4, 0x12)
EPNV(5, 0x13)
EPNV(6, 0x34)
EPNV(7, 0x24)
EPNV(8, 0x15)
EPNV(9, 0x26)
EPNV(Colon, 0x48)
EPNV(Semicolon, 0x57)
EPNV(At, 0x47)
EPNV(A, 0x41)
EPNV(B, 0x64)
EPNV(C, 0x52)
EPNV(D, 0x32)
EPNV(E, 0x22)
EPNV(F, 0x43)
EPNV(G, 0x53)
EPNV(H, 0x54)
EPNV(I, 0x25)
EPNV(J, 0x45)
EPNV(K, 0x46)
EPNV(L, 0x56)
EPNV(M, 0x65)
EPNV(N, 0x55)
EPNV(O, 0x36)
EPNV(P, 0x37)
EPNV(Q, 0x10)
EPNV(R, 0x33)
EPNV(S, 0x51)
EPNV(T, 0x23)
EPNV(U, 0x35)
EPNV(V, 0x63)
EPNV(W, 0x21)
EPNV(X, 0x42)
EPNV(Y, 0x44)
EPNV(Z, 0x61)
EPNV(LeftSquareBracket, 0x38)
EPNV(Backslash, 0x78)
EPNV(RightSquareBracket, 0x58)
EPNV(Caret, 0x18)
EPNV(Underline, 0x28)
EPNV(Escape, 0x70)
EPNV(Tab, 0x60)
EPNV(CapsLock, 0x40)
EPNV(Ctrl, 0x1)
EPNV(ShiftLock, 0x50)
EPNV(Shift, 0x0)
EPNV(Delete, 0x59)
EPNV(Copy, 0x69)
EPNV(Return, 0x49)
EPNV(Up, 0x39)
EPNV(Down, 0x29)
EPNV(Left, 0x19)
EPNV(Right, 0x79)
EPNV(f0, 0x20)
EPNV(f1, 0x71)
EPNV(f2, 0x72)
EPNV(f3, 0x73)
EPNV(f4, 0x14)
EPNV(f5, 0x74)
EPNV(f6, 0x75)
EPNV(f7, 0x16)
EPNV(f8, 0x76)
EPNV(f9, 0x77)
EPNV(Keylinks, 0x2)

// Master 128 numeric keypad.
EPNV(Keypad4, 0x7A)
EPNV(Keypad5, 0x7B)
EPNV(Keypad2, 0x7C)
EPNV(Keypad0, 0x6A)
EPNV(Keypad1, 0x6B)
EPNV(Keypad3, 0x6C)
EPNV(KeypadHash, 0x5A)
EPNV(KeypadStar, 0x5B)
EPNV(KeypadComma, 0x5C)
EPNV(KeypadSlash, 0x4A)
EPNV(KeypadDelete, 0x4B)
EPNV(KeypadStop, 0x4C)
EPNV(KeypadPlus, 0x3A)
EPNV(KeypadMinus, 0x3B)
EPNV(KeypadReturn, 0x3C)
EPNV(Keypad8, 0x2A)
EPNV(Keypad9, 0x2B)
EPNV(Keypad6, 0x1A)
EPNV(Keypad7, 0x1B)

// Break.
EPNV(Break, 0x7f)

// Any negative value counts as no key, and here's a specific enum
// that counts as that.
EPNV(None, -1)
EEND()
#undef ENAME
