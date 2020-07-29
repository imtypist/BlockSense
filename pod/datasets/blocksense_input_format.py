# -*- coding:utf8 -*-
import csv

# prepare input files for zkSNARK

LEN = 500
f = open('LTE_input_p' + str(LEN),'w+')

with open('LTE.csv','r') as csvFile:
	readcsv = csv.reader(csvFile)
	for i, row in enumerate(readcsv):
		if i == LEN:
			break
		f.write(str(i) + " " + str(hex(abs(int(row[9].strip())))) + "\n")

f.write(str(LEN) + " 1")
f.close()
