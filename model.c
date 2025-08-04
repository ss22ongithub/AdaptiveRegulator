#include "kernel_headers.h"
#include "time_series_model.h"
//#include "eml_trees.h"
//#include <asm-generic/bug.h>

#define float_len 8

void print_float(char* buf, float value)
{
	int digits;
	int i;
	int k;
	
	digits = 1;
	while (value >= 10) value /= 10, digits++;
	
//	WARN_ON(digits <= float_len - 2);
	
	i = 0;
	for (k = 0; k < float_len - 2; k++)
	{
		buf[i] = '0';
		while (value >= 1 && buf[i] < '9')
		{
			buf[i]++;
			value -= 1;
		}
		i++;
		digits--;
		if (digits == 0)
		{
			buf[i] = '.';
			i++;
		}
		value *= 10;
	}
//	WARN_ON(i == float_len - 1);
	buf[i] = 0;
}


float lms_predict(const float* feat, u8 feat_len){

    const float weights[]={0.17017401, 0.19412817, 0.17890527, 0.21347153, 0.25384151};
    const float bias = 0.0;
//    pr_info("feat_len = %d", feat_len);
    float sum = 0.0;
    for (u8 i = 0; i < feat_len; i++){
        sum += weights[i] * feat[i];
    }
    sum+= bias;
    return sum;
}

float avg(const u64 * f , u8 len ){
    float sum = 0.0;
    for (u8 i = 0; i < len; i++){
        sum += f[i];
    }
    return (sum/len);
}


u64 estimate(u64* feat, u8 feat_len) {
    char buf[50];
//    float feat[] = {3.14, 6.28, 3.33, 4.44, 5.55};
	kernel_fpu_begin();
	float x = avg(feat,feat_len);
    print_float(buf,x);
    kernel_fpu_end();
    trace_printk("%s: %s \n",__func__,buf);
    return (u64)x;
}

