#!/usr/bin/python3

def main():
    divs=[1,2,4,8,16,64,128,256]

    print('| ULA baud | ULA div | real ACIA Hz? | /16 baud | /64 baud | b2 ACIA Hz | /16 baud | /64 baud')
    print('|---')
        
    for div_index in range(len(divs)):
        div=divs[div_index]
        baud=19200/div
        hz=16e6/13/div
        b2_hz=4e6/13/max(div//4,1)
        if b2_hz<hz:
            b2_div16=hz/16
            b2_div64=hz/64
        else:
            b2_div16=b2_hz/16
            b2_div64=b2_hz/64
        print('| %d | %d | %f | %f | %f|  %f%s|%f|%f'%(baud,
                                          div,
                                          hz,
                                          hz/16,
                                          hz/64,
                                          b2_hz,
                                          " :(" if b2_hz<hz else "",
                                          b2_div16,
                                          b2_div64))

if __name__=='__main__': main()
