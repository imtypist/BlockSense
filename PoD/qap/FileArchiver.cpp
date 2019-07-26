#include "FileArchiver.h"
#include <assert.h>

FileArchiver::FileArchiver(string filename, Mode mode) {
	this->mode = mode;

	if (mode == Read) {
		fs.open(filename.c_str(), ios::in | ios::binary);
	} else {
		fs.open(filename.c_str(), ios::out | ios::binary);
	}
}

FileArchiver::~FileArchiver() {
	fs.close();	
}

void FileArchiver::write(unsigned char *buf, int len) {
	assert(mode == Write);

	fs.write((char*)buf, len);
}

void FileArchiver::read(unsigned char *buf, int len) {
	assert(mode == Read);

	fs.read((char*)buf, len);
}