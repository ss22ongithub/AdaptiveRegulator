#include "kernel_headers.h"
#include "ar.h"


/** Function Prototypes **/
u64 estimate(u64* feat, u8 feat_len, float *wm, u8 wm_len, u8 index);
float lms_predict(const u64* feat, u8 feat_len,float *wm, u8 wm_len, u8 ri);
void update_weight_matrix(u64 error, struct core_info *cinfo );
void print_float(char* buf, float value);
float avg(const u64 * f , u8 len );
void init_weight_matrix(struct core_info *cinfo);


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


float lms_predict(const u64* feat, u8 feat_len,float *wm, u8 wm_len, u8 ri){
    const float bias = 0.0;
    float sum = 0.0f;
    int i =0;
    for (int j = ri; i < wm_len && j >= 0; i++, j--){
        sum += wm[i] * feat[j];
//        trace_printk("wm[%d](%f) * feat[%d](%lu) = %f\n",i,wm[i],j,feat[j],wm[i] * feat[j] );
    }
    for (int j=feat_len-1; i < wm_len && j > ri ; i++,j--) {
        sum += wm[i] * feat[j];
//        trace_printk("wm[%d](%f) * feat[%d](%lu) = %f\n",i,wm[i],j,feat[j],wm[i] * feat[j] );
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


u64 estimate(u64* feat, u8 feat_len, float *wm, u8 wm_len, u8 index) {

    kernel_fpu_begin();
    float result = lms_predict(feat,feat_len,wm , wm_len, index);
    // Convert the float to an integer representation for printk. Preserves 6 decimal places.
    u64 integer_part = (int)result;
    u64 fractional_part = (int)((result - integer_part) * 1000000);
    kernel_fpu_end();
//    trace_printk(" %llu.%llu \n", integer_part, fractional_part);

    return integer_part;
}

    static u64 l2_norm(u64* feature, u8 feat_len){
    u64 norm_sq = 0;
    for (u8 i = 0; i < feat_len; ++i) {
        norm_sq += feature[i] * feature[i] ;
    }
    return norm_sq;
}

void update_weight_matrix(u64 error, struct core_info *cinfo ){
    
    char buf[HIST_SIZE][51];
    char buf2[HIST_SIZE][51];
    u64 norm_sq = l2_norm(cinfo->read_event_hist, HIST_SIZE);
    if ( 0 == norm_sq){
//        trace_printk("Norm Square=0, skipping weight update\n");
        return;
    }
    kernel_fpu_begin();
    const float lrate = 0.005;
    float product[HIST_SIZE] = {0};
    for (u8 i = 0; i <HIST_SIZE ; ++i) {
        product[i] = error * lrate * cinfo->read_event_hist[i];
        cinfo->weight_matrix[i] = cinfo->weight_matrix[i] + (product[i]);//(product[i]/norm_sq);
    }
    kernel_fpu_end();
    for (u8 i = 0; i < HIST_SIZE; i++){
        print_float(buf[i],cinfo->weight_matrix[i]);
//        print_float(buf2[i],product[i]);
    }
    trace_printk("Weights( %s %s %s %s %s) | error=%lld | norm_sq=%llu\n",
        buf[0],buf[1],buf[2],buf[3], buf[4],
        error, norm_sq);
//    trace_printk("Product term ( %s %s %s %s %s) \n",
//                 buf2[0],buf2[1],buf2[2],buf2[3], buf2[4]);

}

void init_weight_matrix(struct core_info *cinfo){
//    const float init_weights[HIST_SIZE]={0.17017401, 0.19412817, 0.17890527, 0.21347153, 0.25384151};
    const float init_weights[HIST_SIZE]={0.1, 0.1, 0.1, 0.1, 0.1};

    for(u8 i =0 ; i < HIST_SIZE; i++){
        cinfo->weight_matrix[i] = init_weights[i];
    }
}
