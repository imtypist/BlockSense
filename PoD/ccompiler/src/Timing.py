from datetime import datetime

class Timing:
	def __init__(self, label, enabled):
		self.label = label
		self.enabled = enabled
		self.prev_time = datetime.now()
		self.prev_phase = "init"

	def phase(self, phase):
		if (not self.enabled):
			return
		cur_time = datetime.now()
		elapsed_time = cur_time-self.prev_time
		elapsed_sec = elapsed_time.seconds + (elapsed_time.microseconds/1000000.)
		print "timing: %s %s %.6f" % (
			self.label,
			self.prev_phase,
			elapsed_sec)
		self.prev_time = cur_time
		self.prev_phase = phase
		self.prev_elapsed_sec = elapsed_sec
