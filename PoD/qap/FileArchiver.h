#pragma once

#include <string>
#include <iostream>
#include <fstream>

#include "Archiver.h"

using namespace::std;

class FileArchiver : public Archiver {
public:
	enum Mode {Read, Write};

	FileArchiver(string filename, Mode mode);	
	~FileArchiver();

	void write(unsigned char *buf, int len);
	void read(unsigned char *buf, int len);	

private:
	Mode mode;

	fstream fs;	
};