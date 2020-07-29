#include "image-kernel-ifc.h"

#define RANDOM_SIZE (IMAGE_H*IMAGE_W)
#define RANDOM_REDUCE 27
#include "random-pragma.h"
#define image data

int ik_abs(int a)
{
#if 1
	int result;
	if (a<0)
	{
		result = -a;
	}
	else
	{
		result = a;
	}
	return result;
#else
	return a;
#endif
}

int diff(int* kernel, struct Point* ip)
{
	struct Point kp;
	int total_diff = 0;
	for (kp.y = 0; kp.y < KERNEL_H; kp.y+=1)
	{
		for (kp.x = 0; kp.x < KERNEL_W; kp.x+=1)
		{
			int image_pixel = IMAGE_PIXEL(image, ip, &kp);
			int kernel_pixel = KERNEL_PIXEL(kernel, &kp);
			int local_diff = image_pixel - kernel_pixel;
			total_diff += ik_abs(local_diff);
//			printf("  kp %d,%d: ld %d total=>%d\n",
//				kp.x, kp.y, local_diff, total_diff);
		}
	}
	return total_diff;
}

void outsource(struct Input *in, struct Output *out)
{
	out->min_loc.x = 0;
	out->min_loc.y = 0;
	out->min_delta = diff(in->kernel, &out->min_loc);

	struct Point ip;
	for (ip.y=0; ip.y < IMAGE_H - KERNEL_H + 1; ip.y+=1)
	{
		for (ip.x=0; ip.x < IMAGE_W - KERNEL_W + 1; ip.x+=1)
		{
			int delta;
			delta = diff(in->kernel, &ip);
#ifdef QSP_TEST
//			printf("At %d,%d: %d\n", ip.x, ip.y, delta);
#endif
			if (delta < out->min_delta)
			{
				out->min_loc.x = ip.x;
				out->min_loc.y = ip.y;
				out->min_delta = delta;
			}
		}
	}
}
