print()
print('| Region | Addr | Offset')
for region in range(16):
    print('|--')
    for addr in range(0,4):
        offset=addr*0x1000|region<<13
        assert (offset&0xfff)==0
        print('| %d (%c) | %xxxx | +%xxxx'%(region,65+region,(addr+8),offset>>12))
print()
