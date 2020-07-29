#include "CircuitMul2.h"

CircuitMul2::CircuitMul2(Field* field) : Circuit(field) {
	freshWires(inputs, 4);
	freshWires(outputs, 2);

	GateMul2* mul = new GateMul2(field, inputs[0], inputs[1], outputs[0]);
	addMulGate(mul);
	mul = new GateMul2(field, inputs[2], inputs[3], outputs[1]);
	addMulGate(mul);

	assignIDs();

	for (int i = 0; i < inputs.size(); i++) {
		inputs[i]->trueInput = true;
	}

	for (int i = 0; i < outputs.size(); i++) {
		outputs[i]->trueOutput = true;
	}

	/*
	inputs.resize(2);
	outputs.resize(1);

	GateMul2* mul = new GateMul2(field, &inputs[0], &inputs[1], &outputs[0]);
	addMulGate(mul);
	assignIDs();
	*/
}

void CircuitMul2::directEval() {
	//field->mul(inputs[0].value, inputs[1].value, outputs[0].value);
	field->mul(inputs[0]->value, inputs[1]->value, outputs[0]->value);
	field->mul(inputs[2]->value, inputs[3]->value, outputs[1]->value);
}