def gcd(a,b):
    if b==0: return a
    else: return gcd(b,a%b)

##########################################################################
##########################################################################

class Remapper:
    def __init__(self,
                 num_steps,
                 num_items):
        self._num_steps=num_steps
        self._num_items=num_items
        self._acc=0

    def step(self):
        self._acc+=self._num_items
        n=self._acc//self._num_steps
        self._acc%=self._num_steps
        return n

    def get_units_from_steps(self,num_steps):
        acc=self._acc+num_steps*self._num_items
        n=acc//self._num_steps
        return n

##########################################################################
##########################################################################

class E:
    # walk through input in units of num/den
    def __init__(self,num,den):
        f=gcd(num,den)
        self._num=num/f
        self._den=den/f
        self._acc=0
        
    def next_input(self):
        self._acc+=self._num
        n=self._acc//self._den
        self._acc%=self._den
        return n
        
##########################################################################
##########################################################################

def main():
    e=E(1000000,44100)#ms per output sample

    e=Remapper(48000,1000000)

    num_steps=1000

    ntotal=e.get_units_from_steps(num_steps)
    
    total=0
    for i in range(num_steps):
        n=e.step()

        total+=n
        print "%d: %d"%(i,n)

    print "total=%d acc=%d"%(total,e._acc)
    print "ntotal=%d"%ntotal
        
##########################################################################
##########################################################################

if __name__=="__main__": main()
