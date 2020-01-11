import os
import sys

def main(argv):
	size= 1<<int(argv[1])
	num = int(argv[2])
	f = open('schdl.txt','w+')
	f.write(str(num)+'\n')
	f.write('1 0 ' + str(size)+' 0 0 \n')
	f.close()

if __name__ == '__main__':
	main(sys.argv)
