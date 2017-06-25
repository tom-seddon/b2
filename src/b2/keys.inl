//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This can be combined with a SDL_Keycode value to produce a combined
// modifier+keys value.
//
// (SDL_Keycode values seem to have bits 9-29 free... there's plenty
// of space.)

#define ENAME PCKeyModifier
EBEGIN()
EPNV(Shift,1<<24)
EPNV(Ctrl,1<<25)
EPNV(Alt,1<<26)
EPNV(Gui,1<<27)
EPNV(AltGr,1<<28)

// not sure if I'm going to bother to support this, since it's
// effectively got 3 states (on/off/don't care)
EPNV(NumLock,1<<29)

EQPNV(Begin,1<<24)
EQPNV(End,1<<30)
// Don't use 1<<30 - it's SDLK_SCANCODE_MASK
EEND()
#undef ENAME


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BeebCharKeyFlag
EBEGIN()
// (skipping bit 31 to avoid annoying warning)
EPNV(CapsShift,1<<30)
EPNV(SymbolShift,1<<29)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BeebShiftState
EBEGIN()
EPN(Any)
EPN(Off)
EPN(On)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// All BeebKeySym values fit in an int8_t.
#define ENAME BeebKeySym
EBEGIN()
EPNV(None,-1)
EPN(f0)
EPN(f1)
EPN(f2)
EPN(f3)
EPN(f4)
EPN(f5)
EPN(f6)
EPN(f7)
EPN(f8)
EPN(f9)
EPN(Escape)
EPN(1)
EPN(ExclamationMark)
EPN(2)
EPN(Quotes)
EPN(3)
EPN(Hash)
EPN(4)
EPN(Dollar)
EPN(5)
EPN(Percent)
EPN(6)
EPN(Ampersand)
EPN(7)
EPN(Apostrophe)
EPN(8)
EPN(LeftBracket)
EPN(9)
EPN(RightBracket)
EPN(0)
EPN(Minus)
EPN(Equals)
EPN(Caret)
EPN(Tilde)
EPN(Backslash)
EPN(Pipe)
EPN(Tab)
EPN(Q)
EPN(W)
EPN(E)
EPN(R)
EPN(T)
EPN(Y)
EPN(U)
EPN(I)
EPN(O)
EPN(P)
EPN(At)
EPN(LeftSquareBracket)
EPN(LeftCurlyBracket)
EPN(Underline)
EPN(Pound)
EPN(CapsLock)
EPN(Ctrl)
EPN(A)
EPN(S)
EPN(D)
EPN(F)
EPN(G)
EPN(H)
EPN(J)
EPN(K)
EPN(L)
EPN(Semicolon)
EPN(Plus)
EPN(Colon)
EPN(Star)
EPN(RightSquareBracket)
EPN(RightCurlyBracket)
EPN(Return)
EPN(ShiftLock)
EPN(Shift)
EPN(Z)
EPN(X)
EPN(C)
EPN(V)
EPN(B)
EPN(N)
EPN(M)
EPN(Comma)
EPN(LessThan)
EPN(Stop)
EPN(GreaterThan)
EPN(Slash)
EPN(QuestionMarke)
EPN(Delete)
EPN(Copy)
EPN(Up)
EPN(Down)
EPN(Left)
EPN(Right)
EPN(KeypadPlus)
EPN(KeypadMinus)
EPN(KeypadSlash)
EPN(KeypadStar)
EPN(Keypad7)
EPN(Keypad8)
EPN(Keypad9)
EPN(KeypadHash)
EPN(Keypad4)
EPN(Keypad5)
EPN(Keypad6)
EPN(KeypadDelete)
EPN(Keypad1)
EPN(Keypad2)
EPN(Keypad3)
EPN(KeypadComma)
EPN(Keypad0)
EPN(KeypadStop)
EPN(KeypadReturn)
EPN(Space)
EPN(Break)
EEND()
#undef ENAME
