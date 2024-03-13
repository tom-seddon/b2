#define ENAME PCD8572State
EBEGIN()
EPN(Idle)
EPN(StartReceiveSlaveAddress)
EPN(ReceiveSlaveAddress)
EPN(ReceiveWordAddress)
EPN(SendAcknowledge)
EPN(ReceiveData)
EPN(SendData)
EPN(ReceiveAcknowledge)
EEND()
#undef ENAME
