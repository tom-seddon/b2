import sys,os,os.path
if os.getenv("INSIDE_EMACS"): os.chdir(os.path.expanduser("~/b2/etc/cmos_adc_sbc/"))

##########################################################################
##########################################################################

def load(fname):
    with open(fname,"rb") as f: return [ord(x) for x in f.read()]

def load_results(instr,carry,decimal):
    name="%sC%dD%d"%(instr,carry,decimal)
    
    aresults=load("A"+name)
    assert len(aresults)==65536
    
    presults=load("P"+name)
    assert len(presults)==65536
    
    result=(load("A"+name),load("P"+name))
    print name

    return (aresults,presults)

##########################################################################
##########################################################################

def check_adc_results(results):
    for a in range(256):
        for b in range(256):
            i=a*256+b
            j=b*256+a
            assert results[i]==results[j]

##########################################################################
##########################################################################

def test_adc_binary(carry):
    aresults,presults=load_results("ADC",carry,False)

    check_adc_results(aresults)
    check_adc_results(presults)

    for a in range(256):
        for b in range(256):
            i=a+b*256
            
            result=a+b+(1 if carry else 0)
            assert (result&255)==aresults[i]

            assert ((result&256)!=0)==((presults[i]&1)!=0)
            assert ((result&255)==0)==((presults[i]&2)!=0)
            assert ((result&128)!=0)==((presults[i]&128)!=0)
            assert ((~(a^b)&(a^(result&255))&128)!=0)==((presults[i]&64)!=0)

def test_adc_bcd(carry):
    aresults,presults=load_results("ADC",carry,True)

    check_adc_results(aresults)
    check_adc_results(presults)

    for a in range(256):
        for b in range(256):
            i=a+b*256

            # add logic is same as NMOS
            tmp=(a&15)+(b&15)+(1 if carry else 0)
            if tmp>9: tmp+=6
            
            if tmp<=15: tmp=(tmp&15)+(a&0xf0)+(b&0xf0)
            else: tmp=(tmp&15)+(a&0xf0)+(b&0xf0)+0x10

            # V logic is same as NMOS
            #v=((a^tmp)&0x80)!=0 and ((a^b)&0x80)==0

            v=((a^tmp)&(~a^b)&0x80)!=0

            # C logic is different
            c=False
            if (tmp&0x1f0)>0x90:
                tmp+=0x60
                c=True

            assert (tmp&255)==aresults[i]

            assert c==((presults[i]&1)!=0)
            assert ((tmp&255)==0)==((presults[i]&2)!=0)
            assert ((tmp&128)!=0)==((presults[i]&128)!=0)
            assert v==((presults[i]&64)!=0)
                
            
def test_adc_legit_bcd(carry):
    aresults,presults=load_results("ADC",carry,True)

    check_adc_results(aresults)
    check_adc_results(presults)

    for a in range(256):
        for b in range(256):
            i=a+b*256
            
            ah=a>>4
            al=a&15

            bh=b>>4
            bl=b&15

            if ah>=10 or al>=10 or bh>=10 or bl>=10:
                # ignore illegitimate BCD values
                continue

            rl=al+bl+(1 if carry else 0)
            rh=ah+bh
            if rl>=10:
                rl-=10
                rh+=1

            if rh<10:
                r=rh*16+rl
                assert r==aresults[i],(hex(a),hex(b),carry,hex(r),hex(aresults[i]))

##########################################################################
##########################################################################

def test_sbc_binary(carry):
    aresults,presults=load_results("SBC",carry,False)

    for a in range(256):
        for b in range(256):
            i=a+b*256

            result=a+(~b&255)+(1 if carry else 0)
            assert (result&255)==aresults[i]

            assert ((result&256)!=0)==((presults[i]&1)!=0),(result,a,b,carry,hex(presults[i]))
            assert ((result&255)==0)==((presults[i]&2)!=0)
            assert ((result&128)!=0)==((presults[i]&128)!=0)
            assert (((a^b)&(a^(result&255))&128)!=0)==((presults[i]&64)!=0)

def test_sbc_bcd(carry):
    aresults,presults=load_results("SBC",carry,True)

    mask=0xfff

    for a in range(256):
        for b in range(256):
            # if (a&0xf0)>=0x10 or (a&0x0f)>=10 or (b&0xf0)>=0x10 or (b&0x0f)>=10: continue

            # http://www.6502.org/tutorials/decimal_mode.html
            
            #print "a=%d b=%d"%(a,b)
            i=a+b*256

            al=(a&15)-(b&15)-(0 if carry else 1)
            r=a-b-(0 if carry else 1)
            v=(((a^b)&(a^(r&255))&128)!=0)
            c=(r&256)==0
            if r<0: r=r-0x60
            if al<0: r=r-0x06
            r&=255

            assert r==aresults[i],(hex(a),hex(b),carry,r,aresults[i])
            
            assert c==((presults[i]&1)!=0),(hex(a),hex(b),carry,r,aresults[i],c,presults[i])
            assert (r==0)==((presults[i]&2)!=0)
            assert v==((presults[i]&64)!=0)
            assert ((r&0x80)!=0)==((presults[i]&0x80)!=0)

##########################################################################
##########################################################################

def test(carry,decimal):
    assert not decimal

    test_adc_binary(carry)
    test_sbc_binary(carry)

    test_adc_bcd(carry)
    test_sbc_bcd(carry)

##########################################################################
##########################################################################

def main():
    test(False,False)
    test(True,False)

##########################################################################
##########################################################################

if __name__=="__main__": main()
