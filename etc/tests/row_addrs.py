#!/usr/bin/python3

def main():
    for num_columns in [40,80]:
        for num_rows in [25,32]:
            for addr in range(0x3000,0x8000,64):
                #print('start: $%04x'%addr)
                row_addr=addr
                for row in range(num_rows):
                    for raster in range(8):
                        fetch_addr=row_addr+raster
                        for column in range(num_columns):
                            if (fetch_addr&(0xf<<11))==0:
                                fetch_addr|=0x3000
                            fetch_addr&=0x7fff
                            assert fetch_addr>=0x3000 and fetch_addr<0x8000,'fetch_addr=$%x num_columns=%d row_addr=$%x row=%d raster=%d column=%d'%(fetch_addr,num_columns,row_addr,row,raster,column)
                            fetch_addr+=8

            row_addr+=num_columns*8

    x15=0
    x16=0
    for stride in [320,640]:
        for i in range(200):
            x15+=stride
            x15&=0x7fff
            
            x16+=stride

            assert x15==(x16&0x7fff)

    print('done')

if __name__=='__main__': main()
