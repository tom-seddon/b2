#define ENAME TestEnum
EBEGIN()
EN(A)
ENV(B, 50)
EPN(C)
EPNV(D, 100)
EQN(E)
EQNV(F, 150)
EQPN(G)
EQPNV(H, 200)
EEND()
#undef ENAME

#define ENAME enum OtherEnum
NBEGIN(OtherEnum)
NN(OE_1)
NN(OE_2)
NN(OE_5)
NEND()
#undef ENAME
