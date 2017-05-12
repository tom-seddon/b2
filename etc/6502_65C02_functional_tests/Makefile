AS65:=as65/as65 -l -m -w -h0

.PHONY:all
all:
	$(AS65) -od0.bin -ld0.lst -Dcputype=0 6502_decimal_test.a65
	$(AS65) -od1.bin -ld1.lst -Dcputype=1 6502_decimal_test.a65

	$(AS65) -o6502.bin -l6502.lst 6502_functional_test.a65

	$(AS65) -o65c02.bin -l65c02.lst -Dwdc_op=0 -Drkwl_wdc_op=0 -x 65C02_extended_opcodes_test.a65c
	$(AS65) -o65c02_rockwell.bin -l65c02_rockwell.lst -Dwdc_op=0 -Drkwl_wdc_op=1 -x 65C02_extended_opcodes_test.a65c
