#pragma once

struct Input {
	int a;
};

struct NIZKInput {
	int b1;
	int b2;
	int b3;
	int b4;
};

struct Output {
	int c1;
	int c2;
	int c3;
};

void outsource(struct Input *input, struct NIZKInput *nizkinput, struct Output *output);
