>RUN
************************************************************
Testing PH3 in One Byte Mode (Type 0)
************************************************************

status                  hdav=1 hsav=1 pnmi=0 psav=0
TUBE RESET

status                  hdav=1 hsav=1 pnmi=0 psav=0
host read       data=FF hdav=0 hsav=1 pnmi=1 psav=1

status                  hdav=0 hsav=1 pnmi=1 psav=1
parasite write  data=11 hdav=1 hsav=1 pnmi=0 psav=0
host read       data=11 hdav=0 hsav=1 pnmi=1 psav=1
host read       data=96 hdav=0 hsav=1 pnmi=1 psav=1

status                  hdav=0 hsav=1 pnmi=1 psav=1
parasite write  data=22 hdav=1 hsav=1 pnmi=0 psav=0
parasite write  data=33 hdav=1 hsav=1 pnmi=0 psav=0
host read       data=22 hdav=1 hsav=1 pnmi=0 psav=0
host read       data=33 hdav=0 hsav=1 pnmi=1 psav=1
host read       data=96 hdav=0 hsav=1 pnmi=1 psav=1

status                  hdav=0 hsav=1 pnmi=1 psav=1
parasite write  data=44 hdav=1 hsav=1 pnmi=0 psav=0
parasite write  data=55 hdav=1 hsav=1 pnmi=0 psav=0
parasite write  data=66 hdav=1 hsav=1 pnmi=0 psav=0
host read       data=44 hdav=1 hsav=1 pnmi=0 psav=0
host read       data=55 hdav=1 hsav=1 pnmi=0 psav=0
host read       data=96 hdav=0 hsav=1 pnmi=1 psav=1
host read       data=96 hdav=0 hsav=1 pnmi=1 psav=1

status                  hdav=0 hsav=1 pnmi=1 psav=1
parasite write  data=77 hdav=1 hsav=1 pnmi=0 psav=0
host read       data=77 hdav=0 hsav=1 pnmi=1 psav=1
parasite write  data=88 hdav=1 hsav=1 pnmi=0 psav=0
host read       data=88 hdav=0 hsav=1 pnmi=1 psav=1
host read       data=96 hdav=0 hsav=1 pnmi=1 psav=1

status                  hdav=0 hsav=1 pnmi=1 psav=1
parasite write  data=99 hdav=1 hsav=1 pnmi=0 psav=0
parasite write  data=AA hdav=1 hsav=1 pnmi=0 psav=0
host read       data=99 hdav=1 hsav=1 pnmi=0 psav=0
parasite write  data=BB hdav=1 hsav=1 pnmi=0 psav=0
host read       data=AA hdav=1 hsav=1 pnmi=0 psav=0
host read       data=BB hdav=0 hsav=1 pnmi=1 psav=1
host read       data=96 hdav=0 hsav=1 pnmi=1 psav=1


************************************************************
Testing HP3 in One Byte Mode (Type 1)
************************************************************

status                  hdav=0 hsav=1 pnmi=1 psav=1
TUBE RESET

status                  hdav=1 hsav=1 pnmi=0 psav=0
parasite read   data=E4 hdav=1 hsav=1 pnmi=0 psav=0

status                  hdav=1 hsav=1 pnmi=0 psav=0
host write      data=11 hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=11 hdav=1 hsav=1 pnmi=0 psav=0
parasite read   data=E4 hdav=1 hsav=1 pnmi=0 psav=0

status                  hdav=1 hsav=1 pnmi=0 psav=0
host write      data=22 hdav=1 hsav=0 pnmi=1 psav=0
host write      data=33 hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=22 hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=33 hdav=1 hsav=1 pnmi=0 psav=0
parasite read   data=E4 hdav=1 hsav=1 pnmi=0 psav=0

status                  hdav=1 hsav=1 pnmi=0 psav=0
host write      data=44 hdav=1 hsav=0 pnmi=1 psav=0
host write      data=55 hdav=1 hsav=0 pnmi=1 psav=0
host write      data=66 hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=44 hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=55 hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=E4 hdav=1 hsav=1 pnmi=0 psav=0
parasite read   data=E4 hdav=1 hsav=1 pnmi=0 psav=0

status                  hdav=1 hsav=1 pnmi=0 psav=0
host write      data=77 hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=77 hdav=1 hsav=1 pnmi=0 psav=0
host write      data=88 hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=88 hdav=1 hsav=1 pnmi=0 psav=0
parasite read   data=E4 hdav=1 hsav=1 pnmi=0 psav=0

status                  hdav=1 hsav=1 pnmi=0 psav=0
host write      data=99 hdav=1 hsav=0 pnmi=1 psav=0
host write      data=AA hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=99 hdav=1 hsav=0 pnmi=1 psav=0
host write      data=BB hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=AA hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=BB hdav=1 hsav=1 pnmi=0 psav=0
parasite read   data=E4 hdav=1 hsav=1 pnmi=0 psav=0


************************************************************
Testing PH3 in Two Byte Mode (Type 2)
************************************************************

status                  hdav=1 hsav=1 pnmi=0 psav=0
TUBE RESET

status                  hdav=1 hsav=1 pnmi=0 psav=0
host read       data=0D hdav=0 hsav=1 pnmi=1 psav=1

status                  hdav=0 hsav=1 pnmi=1 psav=1
parasite write  data=11 hdav=0 hsav=1 pnmi=1 psav=1
host read       data=11 hdav=0 hsav=1 pnmi=1 psav=1
host read       data=96 hdav=0 hsav=1 pnmi=1 psav=1

status                  hdav=0 hsav=1 pnmi=1 psav=1
parasite write  data=22 hdav=0 hsav=1 pnmi=1 psav=1
parasite write  data=33 hdav=1 hsav=1 pnmi=0 psav=0
host read       data=22 hdav=1 hsav=1 pnmi=0 psav=0
host read       data=33 hdav=0 hsav=1 pnmi=1 psav=1
host read       data=96 hdav=0 hsav=1 pnmi=1 psav=1

status                  hdav=0 hsav=1 pnmi=1 psav=1
parasite write  data=44 hdav=0 hsav=1 pnmi=1 psav=1
parasite write  data=55 hdav=1 hsav=1 pnmi=0 psav=0
parasite write  data=66 hdav=1 hsav=1 pnmi=0 psav=0
host read       data=44 hdav=1 hsav=1 pnmi=0 psav=0
host read       data=55 hdav=1 hsav=1 pnmi=0 psav=0
host read       data=96 hdav=0 hsav=1 pnmi=1 psav=1
host read       data=96 hdav=0 hsav=1 pnmi=1 psav=1

status                  hdav=0 hsav=1 pnmi=1 psav=1
parasite write  data=77 hdav=0 hsav=1 pnmi=1 psav=1
host read       data=77 hdav=0 hsav=1 pnmi=1 psav=1
parasite write  data=88 hdav=0 hsav=1 pnmi=1 psav=1
host read       data=88 hdav=0 hsav=1 pnmi=1 psav=1
host read       data=96 hdav=0 hsav=1 pnmi=1 psav=1

status                  hdav=0 hsav=1 pnmi=1 psav=1
parasite write  data=99 hdav=0 hsav=1 pnmi=1 psav=1
parasite write  data=AA hdav=1 hsav=1 pnmi=0 psav=0
host read       data=99 hdav=1 hsav=1 pnmi=0 psav=0
parasite write  data=BB hdav=1 hsav=1 pnmi=0 psav=0
host read       data=AA hdav=1 hsav=1 pnmi=0 psav=0
host read       data=BB hdav=0 hsav=1 pnmi=1 psav=1
host read       data=96 hdav=0 hsav=1 pnmi=1 psav=1


************************************************************
Testing HP3 in Two Byte Mode (Type 3)
************************************************************

status                  hdav=0 hsav=1 pnmi=1 psav=1
TUBE RESET

status                  hdav=1 hsav=1 pnmi=0 psav=0
parasite read   data=E4 hdav=1 hsav=1 pnmi=0 psav=0

status                  hdav=1 hsav=1 pnmi=0 psav=0
host write      data=11 hdav=1 hsav=1 pnmi=0 psav=0
parasite read   data=11 hdav=1 hsav=1 pnmi=0 psav=0
parasite read   data=E4 hdav=1 hsav=1 pnmi=0 psav=0

status                  hdav=1 hsav=1 pnmi=0 psav=0
host write      data=22 hdav=1 hsav=1 pnmi=0 psav=0
host write      data=33 hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=22 hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=33 hdav=1 hsav=1 pnmi=0 psav=0
parasite read   data=E4 hdav=1 hsav=1 pnmi=0 psav=0

status                  hdav=1 hsav=1 pnmi=0 psav=0
host write      data=44 hdav=1 hsav=1 pnmi=0 psav=0
host write      data=55 hdav=1 hsav=0 pnmi=1 psav=0
host write      data=66 hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=44 hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=55 hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=E4 hdav=1 hsav=1 pnmi=0 psav=0
parasite read   data=E4 hdav=1 hsav=1 pnmi=0 psav=0

status                  hdav=1 hsav=1 pnmi=0 psav=0
host write      data=77 hdav=1 hsav=1 pnmi=0 psav=0
parasite read   data=77 hdav=1 hsav=1 pnmi=0 psav=0
host write      data=88 hdav=1 hsav=1 pnmi=0 psav=0
parasite read   data=88 hdav=1 hsav=1 pnmi=0 psav=0
parasite read   data=E4 hdav=1 hsav=1 pnmi=0 psav=0

status                  hdav=1 hsav=1 pnmi=0 psav=0
host write      data=99 hdav=1 hsav=1 pnmi=0 psav=0
host write      data=AA hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=99 hdav=1 hsav=0 pnmi=1 psav=0
host write      data=BB hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=AA hdav=1 hsav=0 pnmi=1 psav=0
parasite read   data=BB hdav=1 hsav=1 pnmi=0 psav=0
parasite read   data=E4 hdav=1 hsav=1 pnmi=0 psav=0


>*SPOOL
