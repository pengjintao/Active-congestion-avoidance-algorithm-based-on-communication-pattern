#coding:utf-8
import sys 
import Gentrace as GT
import pprint
import Iph_IO as Iphio
import random
import copy
def read_config():
    f = open("./schdule_config.pjt")
    PackUpLimit = int((f.readline()).strip())
    f.close()
    return PackUpLimit
def path(nodeO,Match,used,NodeOutMsgs):
    for msg in NodeOutMsgs[nodeO]:
        if(not msg.target in used):
            used.add(msg.target)
            if(not msg.target in Match) or path(Match[msg.target].source,Match,used,NodeOutMsgs):
                Match[msg.target] = msg
                return 1
    return 0
def time_control(Groups,procn):
    #time control algorithm
    #pprint.pprint(Groups)
    #try make at least one node full loaded all the time
    #For each out node and in node , find the minimum total time gap
    schdl = []
    NodeOutGap = [0]*procn
    NodeInGap  = [0]*procn
    #get nodes out gap and key node
    for i in range(0,len(Groups)):
        NodeOutLoads = [0]*procn
        NodeInLoads  = [0]*procn
        m = 0
        for msg in Groups[i]:
            NodeOutLoads[msg.source] += msg.size
            NodeInLoads[msg.target]  += msg.size
        m = max(max(NodeOutLoads),max(NodeInLoads))
        for j in range(0,procn):
            NodeOutGap[j] += (m - NodeOutLoads[j])
            NodeInGap[j] += (m - NodeInLoads[j])
    mina = min(NodeOutGap)
    minb = min(NodeInGap)

    if(mina <= minb):
        #key node is send node
        key_proc = NodeOutGap.index(min(NodeOutGap))
        key_proc_load_prev = 0
        for group in Groups:
            key_proc_load = 0
            for msg in group:
                if msg.source == key_proc:
                    key_proc_load+=msg.size
            for msg in group:
                msg.delay = key_proc_load_prev
                schdl.append(msg)
            key_proc_load_prev +=key_proc_load
    else:
        #key node is recv node
        key_proc = NodeInGap.index(min(NodeInGap))
        key_proc_load_prev = 0
        for group in Groups:
            key_proc_load = 0
            for msg in group:
                if msg.target == key_proc:
                    key_proc_load+=msg.size
            for msg in group:
                msg.delay = key_proc_load_prev
                schdl.append(msg)   
            key_proc_load_prev +=key_proc_load
    return schdl
def distribute_schedulor(Man,Women,m):
    Partition = []
    return Partition
def main(argv):
    PackUpLimit = read_config()
    traceN = argv[1]
    x = 0
    y = 0
    size = 0
    procn = 0
    #msgs        = GT.Stencil_Round_Gen(x,y,size)
    msgs = None
    repeate = 1
    if traceN == "Stencil_Gen":
        x       = int(argv[2])
        y       = int(argv[3])
        size    = 1<<int(argv[4])
        procn = x*y
        msgs        = GT.Stencil_Gen(x,y,size)
    elif traceN == "Stencil_Round_Gen":
        x       = int(argv[2])
        y       = int(argv[3])
        size    = 1<<int(argv[4])
        procn = x*y
        msgs        = GT.Stencil_Round_Gen(x,y,size)
    elif traceN == "all_to_all_Gen":
        procn =   int(argv[2])
        size    = 1<<int(argv[3])
        msgs        = GT.all_to_all_Gen(procn,size)
    elif traceN == "applications":
        Id     = int(argv[2])
        msgs   = GT.read_file(Id)
        procn  = 16
        #pprint.pprint(msgs)
    MsgOrigin   = []
    MsgSymmetric = []
    i = 0
    #现假设所有消息都是对称的即(a,b,size)存在那么一定存在(b,a,size)
    temp = []
    for i in range(0,repeate):
        temp += msgs
    msgs = temp
    for msg in msgs:
        #msg[2] = 1<<22
        t = None
        if (msg[1] > msg[0]):
            t = GT.IphMsg(i,msg[0],msg[1],msg[2],0,0,msg[1] - msg[0])
        else:
            t = GT.IphMsg(i,msg[0],msg[1],msg[2],0,0,procn + msg[1] - msg[0])
        #再假设不存在自我发送的消息(a,a,size)
        #并且所有消息都是对称的
        if(msg[1] > msg[0]):
            MsgSymmetric.append(t)
        MsgOrigin.append(t)
        i+=1

    #MsgOrigin.sort(key=lambda msg: msg.size)
    ##pprint.pprint(MsgOrigin)
    Iphio.writeF(MsgOrigin,"origin.txt")
    #Iphio.writeFseperate(MsgOrigin,"after1.txt")
    NodeOutMsgs = []
    NodeInMsgs  = []
    for proc in range(0,procn):
        vecT = []
        vecT1 = []
        NodeOutMsgs.append(vecT)
        NodeInMsgs.append(vecT1)
    #init
    for msg in MsgOrigin:
        NodeOutMsgs[msg.source].append(msg)
        NodeInMsgs[msg.target].append(msg)
    #sort locally
    NodesOut = []
    i=0
    for vec in NodeOutMsgs:
        #pprint.pprint(vec)
        vec.sort(key=lambda msg: msg.key,reverse=False)
        if len(vec) > 0:
            NodesOut.append(vec[0].source)
    #     print(i,end = "\t--> ")
    #     i+=1
    #     for msg in vec:
    #         print(msg.target,end =" ")
    #     print("")
    # print("-----------------------------------")
    i = 0
    NodesIn = []  
    for vec in NodeInMsgs:
        vec.sort(key=lambda msg: msg.key,reverse=False)
        if len(vec) > 0:
            NodesIn.append(vec[0].target)
    #   print(i,end = "\t<-- ")
    #     i+=1
    #     for msg in vec:
    #         print(msg.source,end =" ")
    #     print("")
    # print("-----------------------------------")
    #print some information
    #try 最大二分匹配法
    #for each send nodes
    Groups = []
    while len(NodesOut) > 0:
        #print("")
        Match = {} #Match[RecvNode] = msgGid
        for nodeO in NodesOut:
            used = set()
            path(nodeO,Match,used,NodeOutMsgs)
            #print(nodeO)
        tmp = []
        for key,msg in Match.items():
            #print(msg.source,msg.target,end = "\t")
            #将消息从NodeOutMsgs，NodesOut中除去
            NodeOutMsgs[msg.source].remove(msg)
            tmp.append(msg)
            if len(NodeOutMsgs[msg.source]) == 0:
                NodesOut.remove(msg.source)
        Groups.append(tmp)
    #每PackUpLimit个匹配合并为一组
    Partition = []
    for i in range(0,len(Groups),PackUpLimit):
        tmp = []
        for j in range(0,PackUpLimit):
            if(j == 0):
                for msg in Groups[i+j]:
                    msg.round = 1
            if i+j < len(Groups):
                tmp+= Groups[i+j]
        Partition.append(tmp)
    MsgAfter = time_control(Partition,procn)
    Iphio.writeF(MsgAfter,"after.txt")

    #分布式GS-调度
    Partition = []
    for i in range(0,procn):
        Partition += distribute_schedulor(NodeOutMsgs,NodeInMsgs,i)
    Iphio.writeF(MsgAfter,"after1.txt")

if __name__ == "__main__":
    main(sys.argv)

