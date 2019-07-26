#pragma once

#define DATA_LENGTH 300
#define SIGMA 5
#define WINDOW_SIZE 5

struct Input {
	int t[DATA_LENGTH];
};

struct Output {
	int x[DATA_LENGTH];
};

void outsource(struct Input *input, struct Output *output);