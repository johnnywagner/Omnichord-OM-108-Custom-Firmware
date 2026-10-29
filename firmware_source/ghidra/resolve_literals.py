import re,struct
D=open('../images/stock.bin','rb').read()
s=open('decomp.c').read()
def rep(m):
    a=int(m.group(1),16)
    if 0x60000000<=a<0x60040000 and a%4==0:
        v=struct.unpack('<I',D[a-0x60000000:a-0x60000000+4])[0]
        return 'P_%08x'%v
    return m.group(0)
s=re.sub(r'DAT_(600[0-3][0-9a-f]{4})',rep,s)
open('decomp2.c','w').write(s)
