# -*- coding:utf8 -*-
import timeit
import random
import json

WINDOW_SIZE = 3
SIGMA = 5
DATA_LENGTH = 500

def outsource():
	output = []
	_input = []
	with open('_input','r') as f:
		_input = json.loads(f.read())
	# with open('_input','r') as f:
	# 	for dp in f.readlines():
	# 		_input.append(float(dp.strip('\n')))
	print(_input)
	for i in range(0,int(WINDOW_SIZE/2)):
		output.append(0)
	for i in range(int(WINDOW_SIZE / 2), DATA_LENGTH - int(WINDOW_SIZE / 2)):
		temp = []
		for j in range(0,WINDOW_SIZE):
			temp.append(_input[i - int(WINDOW_SIZE / 2) + j])
		for m in range(0,WINDOW_SIZE-1):
			for n in range(0,WINDOW_SIZE-m-1):
				if temp[n] > temp[n+1]:
					temp[n+1] = temp[n] + temp[n+1]
					temp[n] = temp[n+1] - temp[n]
					temp[n+1] = temp[n+1] - temp[n]
		median = temp[int(WINDOW_SIZE/2)]
		if _input[i] < (median - SIGMA):
			output.append(1)
		elif _input[i] > (median + SIGMA):
			output.append(1)
		else:
			output.append(0)
	for i in range(DATA_LENGTH-int(WINDOW_SIZE/2),DATA_LENGTH):
		output.append(0)
	return output

if __name__ == '__main__':
	_input = []
	for i in range(0,DATA_LENGTH):
		_input.append(random.random()*100)
	with open('_input','w+') as f:
		f.write(json.dumps(_input))
	# with open('_input','w+') as f:
	# 	for i in range(0,DATA_LENGTH):
	# 		f.write(str(random.random()*100) + "\n")
	start = timeit.default_timer()
	output = outsource()
	elapsed = (timeit.default_timer() - start)
	print(output)
	print(elapsed)