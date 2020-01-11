import sys 
pp=1
class IphMsg:
    source = -1
    target = -1
    size   = 0
    round  = 0 
    delay  = 0
    key    = 0
    gid    = -1
    def __init__(self,gid = -1,source = -1,target = -1,size = 0,delay = 0,round = 0,key = 0):
        self.source = source
        self.target = target
        self.size   = size
        self.delay  = delay
        self.round  = round
        self.key    = key
        self.gid    = gid
def Stencil_Gen(x,y,msg_size):
    re = []
    for i in range(0,x):
        for j in range(0,y):
            if i+1<x :
                re.append([i*y+j,(i+1)*y+j,msg_size])
            if i-1 >= 0:
                re.append([i*y+j,(i-1)*y+j,msg_size])
            if j+1 < y:
                re.append([i*y+j,(i)*y+j+1,msg_size])
            if j-1 >= 0:
                re.append([i*y+j,(i)*y+j-1,msg_size])
    return re
def Stencil_Round_Gen(x,y,msg_size):
    re = []
    for i in range(0,x):
        for j in range(0,y):
            if i+1<x :
                re.append([i*y+j,(i+1)*y+j,msg_size])
            else:
                re.append([i*y+j,j,msg_size])
            if i-1 >= 0:
                re.append([i*y+j,(i-1)*y+j,msg_size])
            else:
                re.append([i*y+j,(x-1)*y+j,msg_size])
            if j+1 < y:
                re.append([i*y+j,(i)*y+j+1,msg_size])
            else:
                re.append([i*y+j,(i)*y,msg_size])
            if j-1 >= 0:
                re.append([i*y+j,(i)*y+j-1,msg_size])
            else:
                re.append([i*y+j,(i)*y+y-1,msg_size])
    return re
def all_to_all_Gen(nproc,msg_size,proc_per_node=1):
    re = []
    for i in range(0,nproc):
        for j in range(0,nproc):
            if i != j:
                a = int(i/proc_per_node)
                b = int(j/proc_per_node)
                if a!=b:
                    re.append([i,j,msg_size])
    return re
def read_file(Id):
    f = open("./applications/"+str(Id)+"-before.txt")
    msgn = int((f.readline()).strip())
    msgs = []
    for i in range(0,msgn):
        line = f.readline()
        lst = line.split()
        msgs.append([
            int((lst[0]).strip()),
            int((lst[1]).strip()),
            int((lst[2]).strip())
        ])
    f.close()
    return msgs
#return msg vec [[source,target,size],[source,target,size],,,,,[source,target,size]]
def Gentrace(name,size = 0,nproc = 0,X = 0,Y = 0):
    if(name == "2d-stencil"):
        return Stencil_Gen(X,Y,size)
