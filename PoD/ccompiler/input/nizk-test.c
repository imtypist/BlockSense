#include "nizk-test-ifc.h"

#if 17
void outsource(struct Input *input, struct NIZKInput *nizkinput, struct Output *output)
{
	output->c1 = input->a + nizkinput->b1;
	output->c3 = input->a + nizkinput->b3;
}
#else
void outsource(struct Input *input, struct Output *output)
{
	output->c1 = input->a;
	output->c3 = input->a + 3;
}
#endif
