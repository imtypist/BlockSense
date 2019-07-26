# -*- coding:utf8 -*-

import csv
f = open('LTE_input','w+')

with open('LTE.csv','r') as csvFile:
	readcsv = csv.reader(csvFile)
	for i, row in enumerate(readcsv):
		if i == 100:
			break
		f.write(str(i) + " " + str(hex(abs(int(row[9].strip())))) + "\n")

f.write("100 1")
f.close()
