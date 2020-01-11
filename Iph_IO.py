#coding:utf-8
import sys
def writeFsymmetric(Msgs,filename):
    f=open("./"+filename,"w")
    f.write(str(2*len(Msgs)) + "\n")
    for msg in Msgs:
        f.write(str(msg.source) + " " +
                str(msg.target) + " " +
                str(msg.size)   + " " +
                str(msg.round)  + " " +
                str(msg.delay)+ "\n")
        #对称的写消息，实验性
        f.write(str(msg.target) + " " +
                str(msg.source) + " " +
                str(msg.size)   + " " +
                str(msg.round)  + " " +
                str(msg.delay)+ "\n")
    f.close()
def writeFseperate(Msgs,filename):
    f=open("./"+filename,"w")
    f.write(str(len(Msgs)) + "\n")
    for msg in Msgs:
        f.write(str(msg.source) + " " +
                str(msg.target) + " " +
                str(msg.size)   + " " +
                "1"  + " " +
                str(msg.delay)+ "\n")
    f.close()
def writeF(Msgs,filename):
    f=open("./"+filename,"w")
    f.write(str(len(Msgs)) + "\n")
    for msg in Msgs:
        f.write(str(msg.source) + " " +
                str(msg.target) + " " +
                str(msg.size)   + " " +
                str(msg.round)  + " " +
                str(msg.delay)+ "\n")
        #对称的写消息，实验性
        # f.write(str(msg.target) + " " +
        #         str(msg.source) + " " +
        #         str(msg.size)   + " " +
        #         str(msg.round)  + " " +
        #         str(msg.delay)+ "\n")
    f.close()
    