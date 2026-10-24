"""SWF tag and affine placement encoders used by local extraction."""
import struct

def tag(code,data):
    return struct.pack('<H',code*64+63)+struct.pack('<I',len(data))+data
def matrix(values):
    sx,r0,r1,sy,tx,ty=values;bits=''
    def put(n,w):return format(n & ((1<<w)-1),f'0{w}b')
    def signed(v):return max(1,max(abs(x).bit_length()+1 for x in v))
    if sx!=1 or sy!=1:
        nums=[round(sx*65536),round(sy*65536)];n=signed(nums);bits+='1'+put(n,5)+''.join(put(x,n) for x in nums)
    else:bits+='0'
    if r0 or r1:
        nums=[round(r0*65536),round(r1*65536)];n=signed(nums);bits+='1'+put(n,5)+''.join(put(x,n) for x in nums)
    else:bits+='0'
    nums=[round(tx*20),round(ty*20)];n=signed(nums);bits+=put(n,5)+''.join(put(x,n) for x in nums)
    bits+='0'*(-len(bits)%8);return int(bits,2).to_bytes(len(bits)//8,'big')
def place(cid,depth,transform):return tag(26,b'\x06'+struct.pack('<HH',depth,cid)+matrix(transform))
