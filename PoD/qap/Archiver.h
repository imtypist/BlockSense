#pragma once

// The superclass of various classes capable of reading and writing data out

class Archiver {
public:
	virtual void write(unsigned char *buf, int len) = 0;
	virtual void read(unsigned char *buf, int len) = 0;
};

