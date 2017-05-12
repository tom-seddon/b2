import sys

print

for crtc_addr in range(0x2000,0x3000,128):
    r12=crtc_addr>>8
    r13=crtc_addr&255

    addr=crtc_addr&0x1fff       # strip off the mode 7 addressing bit

    offset=crtc_addr&0x3ff

    mem_addr=0x3c00|offset
    if crtc_addr&0x800:
        # 0x4000 = 0x800<<3
        #
        # is the <<3 there for a reason???
        mem_addr|=0x4000

    # seems like 0x1000 and 0x400 are ignored
    
    print "%04x %04x %04x %04x"%(crtc_addr,addr,offset,mem_addr)
    
    
