#include "kernel_headers.h"
#include "time_series_model.h"
//#include "eml_trees.h"
//#include <asm-generic/bug.h>



/** Function Prototypes **/
u64 estimate(u64* feat, u8 feat_len);
float lms_predict(const u64* feat, u8 feat_len);
void update_weight_matrix(u64 error, struct core_info *cinfo );
void print_float(char* buf, float value);
float avg(const u64 * f , u8 len );

#define float_len 10

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


float lms_predict(const u64* feat, u8 feat_len){

    const float weights[]={0.17017401, 0.19412817, 0.17890527, 0.21347153, 0.25384151};
    const float bias = 0.0;

    float sum = 0.0;
    for (u8 i = 0; i < feat_len; i++){
        sum += weights[i] * feat[i];
    }
    sum+= bias;
    return sum;
}

float avg(const u64 * f , u8 len ){
//    pr_info("%x %d",f,len);
    float sum = 0.0f;
    u8 i = 0;
    if (len == 0) {
        return 0.0f;
    }
    for (i = 0; i < len; i++){
        sum += f[i];
    }
    return (sum/len);
}


u64 estimate(u64* feat, u8 feat_len) {
//    char buf[50];
    kernel_fpu_begin();
    float result = lms_predict(feat,feat_len);
    kernel_fpu_end();
//    print_float(buf,result);

    // Convert the float to an integer representation for printk. Preserves 6 decimal places.
    u64 integer_part = (int)result;
    u64 fractional_part = (int)((result - integer_part) * 1000000);
    pr_debug("estimate= %llu.%llu \n", integer_part, fractional_part);
//    pr_debug("%s", buf);

    return integer_part;
}

static u64 l2_norm(u64* feature, u8 feat_len){
    u64 norm = 0;
    for (u8 i = 0; i < feat_len; ++i) {
        norm += feature[i] * feature[i] ;
    }
    return norm;
}
void update_weight_matrix(u64 error, struct core_info *cinfo ){
    kernel_fpu_begin();
    const float lrate = 0.5;
    u64 norm = l2_norm(cinfo->read_event_hist, HIST_SIZE);


    float normalized_count[HIST_SIZE] = {0.0f};
    for (u8 i = 0; i <HIST_SIZE ; ++i) {
        normalized_count[i] = cinfo->read_event_hist[i] / norm;
        cinfo->weight_matrix[i] = cinfo->weight_matrix[i] + (error * lrate * normalized_count[i]);
    }
    kernel_fpu_end();
}


