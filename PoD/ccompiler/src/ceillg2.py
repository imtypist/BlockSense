def ceillg2(val):
	for i in range(254):
		if (val < (1<<i)):
			return i
	raise Exception("Overflow")

