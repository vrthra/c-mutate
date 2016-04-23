import glob

allm = open("allmutants.c",'w')

for f in glob.glob("mutant*.c"):
    fnum = str(int((f.split("_")[0])[7:]))
    for l in open(f):
        allm.write(l.replace("Horspool","Horspool" + fnum))
    allm.write("\n")
    allm.write("\n")

allm.close()
