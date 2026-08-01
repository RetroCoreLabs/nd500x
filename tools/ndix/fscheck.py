# fscheck.py <image> [blkzero] - list every directory in an NDIX FFS image
import sys, struct
IMG=sys.argv[1]; BLKZERO=int(sys.argv[2]) if len(sys.argv)>2 else 90
f=open(IMG,'rb')
f.seek(BLKZERO*1024+8192); sb=f.read(8192)
g=lambda o: struct.unpack('>i',sb[o:o+4])[0]
iblkno=g(8+8); ipg=g(8+36*4+6*4+8*4)  # not used; compute base directly
INO_BASE=(BLKZERO+iblkno)*1024
IFMT,IFCHR,IFBLK,IFDIR,IFREG=0o170000,0o020000,0o060000,0o040000,0o100000
def dinode(n):
    f.seek(INO_BASE+n*128); b=f.read(128)
    mode,nlink,uid,gid=struct.unpack('>HHHH',b[0:8])
    size=struct.unpack('>I',b[8:12])[0]
    db=[struct.unpack('>I',b[40+4*k:44+4*k])[0] for k in range(12)]
    return mode,nlink,size,db
def walk(frag):
    f.seek((frag+BLKZERO)*1024); blk=f.read(1024); out=[];off=0
    while off<1024:
        ino,rl,nl=struct.unpack('>IHH',blk[off:off+8])
        if rl==0: break
        nm=blk[off+8:off+8+nl].decode('ascii','replace')
        if ino: out.append((nm,ino))
        off+=rl
    return out
bad=0
def show(path,ino,depth):
    global bad
    mode,nlink,size,db=dinode(ino)
    ents=walk(db[0])
    names=[n for n,i in ents]
    if len(set(names))!=len(names): print("DUPLICATE entries in",path); bad+=1
    print("%s: %d entries: %s"%(path,len(ents)-2," ".join(n for n in names if n not in('.','..'))))
    for n,i in ents:
        if n in ('.','..'): continue
        m,_,sz,_=dinode(i)
        if (m&IFMT)==IFDIR and depth<3: show(path.rstrip('/')+'/'+n,i,depth+1)
        elif (m&IFMT)==IFREG and sz==0: print("  ZERO-LENGTH FILE",path+'/'+n); bad+=1
print("iblkno=%d INO_BASE=0x%X"%(iblkno,INO_BASE))
show('/',2,0)
print("BAD: %d"%bad)
