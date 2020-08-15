import time
import string
import sys
import commands
import psutil

def main():
    #print(sys.argv[1])
    p = psutil.Process(int(sys.argv[1]))
    f = open("./cpu.log", "w+")
    while True:
	cpu	= p.cpu_percent()
	print(cpu)
	f.write(str(cpu)+"\n")
	time.sleep(1)

if __name__ == "__main__":
    main()
