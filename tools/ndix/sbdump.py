import sys, struct
img = sys.argv[1]; base = int(sys.argv[2])*1024 if len(sys.argv)>2 else 90*1024
f=open(img,'rb'); f.seek(base+8192); b=f.read(8192)
names="sblkno cblkno iblkno dblkno cgoffset cgmask time size dsize ncg bsize fsize frag minfree rotdelay rps bmask fmask bshift fshift maxcontig maxbpg fragshift fsbtodb sbsize csmask csshift nindir inopb nspf".split()
off=8
for n in names:
    v=struct.unpack('>i',b[off:off+4])[0]; print("%-10s %d"%(n,v)); off+=4
off += 6*4
for n in "csaddr cssize cgsize ntrak nsect spc ncyl cpg ipg fpg ndir nbfree nifree nffree".split():
    v=struct.unpack('>i',b[off:off+4])[0]; print("%-10s %d"%(n,v)); off+=4
print("magic %x"%struct.unpack('>I',b[1372:1376])[0])
print("fsmnt %r"%b[212:212+40])
