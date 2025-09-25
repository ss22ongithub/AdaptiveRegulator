#include "kernel_headers.h"
#include "ar.h"


/** Function Prototypes **/
u64 estimate(u64* feat, u8 feat_len, double *wm, u8 wm_len, u8 index);
double lms_predict(const u64* feat, u8 feat_len,double *wm, u8 wm_len, u8 ri);
void update_weight_matrix(s64 error, struct core_info *cinfo );
void print_double(char* buf, double value);
double avg(const u64 * f , u8 len );
void init_weight_matrix(struct core_info *cinfo);
const double  LRATE = 0.0000001;

#define double_len 10

void print_double(char* buf, double value)
{
	int digits;
	int i;
	int k;
	
	digits = 1;
	while (value >= 10) value /= 10, digits++;
	
//	WARN_ON(digits <= double_len - 2);
	
	i = 0;
	for (k = 0; k < double_len - 2; k++)
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
//	WARN_ON(i == double_len - 1);
	buf[i] = 0;
}



double lms_predict(const u64* feat, u8 feat_len,double *wm, u8 wm_len, u8 ri){
    const double bias = 0.0;
    double sum = 0.0f;
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

double avg(const u64 * f , u8 len ){
//    pr_info("%x %d",f,len);
    double sum = 0.0f;
    u8 i = 0;
    if (len == 0) {
        return 0.0f;
    }
    for (i = 0; i < len; i++){
        sum += f[i];
    }
    return (sum/len);
}


u64 estimate(u64* feat, u8 feat_len, double *wm, u8 wm_len, u8 index) {
    kernel_fpu_begin();
    // double result = lms_predict(feat,feat_len,wm , wm_len, index);
    double result = lms_predict(feat,feat_len, wm ,  wm_len, index);
    // Convert the double to an integer representation for printk. Preserves 6 decimal places.
    kernel_fpu_end();
    u64 integer_part = (int)result;
    //u64 fractional_part = (int)((result - integer_part) * 1000000);
    //trace_printk(" %lld.%llu \n", integer_part, fractional_part);
    return integer_part;
}

static u64 l2_norm(u64* feature, u8 feat_len){
    u64 norm_sq = 0;
    for (u8 i = 0; i < feat_len; ++i) {
        norm_sq += mul_u64_u64_shr(feature[i],feature[i], 16) ;
    }
    return norm_sq;
}

void 
update_weight_matrix(s64 error,struct core_info* cinfo ){
    
    // Avoid Divide by zero error
    u64 norm_sq = l2_norm(cinfo->read_event_hist, HIST_SIZE);
    if ( 0 == norm_sq){
        // trace_printk("CPU(%d): Norm Square=0, skipping weight update\n", cinfo->cpu_id);
        return;
    }
    // sign_bit: 1 = +ve , -1 = -ve. Convert error to a +ve value
    const s8 sign_bit = (error < 0)?-1:1;
    error = error * sign_bit;
    // At this point error is always +ve


    double  product[HIST_SIZE] = {0};

    kernel_fpu_begin();
    for (u8 i = 0; i <HIST_SIZE ; ++i) {
        u64 t1 = mul_u64_u64_shr(error,cinfo->read_event_hist[i],0);
        double  t2 = t1 / norm_sq;
        product[i] = t2 * LRATE;
        // Sign bit is used while updating the weight vector
        cinfo->weight_matrix[i] = cinfo->weight_matrix[i] + (sign_bit * product[i]);
    }
    kernel_fpu_end();
    

    char buf[HIST_SIZE][51]={0};
    char buf2[HIST_SIZE][51]={0};
    for (u8 i = 0; i < HIST_SIZE; i++){
        print_double(buf[i],cinfo->weight_matrix[i]);
        // print_double(buf2[i],product[i]);
    }

    trace_printk("\n CPU(%u) | HIST_SIZE=%d Weights ( %s %s %s %s %s) \n",
                 cinfo->cpu_id, HIST_SIZE,
                 buf[0],buf[1],buf[2],buf[3], buf[4]);
#if 0
    // trace_printk("\n CPU(%u)| read_event_hist( %llu, %llu, %llu, %llu, %llu)|ri=%u |\n error=%lld | norm_sq=%llu\n",
    //     cinfo->cpu_id,
    //     cinfo->read_event_hist[0],cinfo->read_event_hist[1],cinfo->read_event_hist[2],
    //     cinfo->read_event_hist[3],cinfo->read_event_hist[4],
    //     cinfo->ri,
    //     error, norm_sq);
    
    // trace_printk("\n CPU(%u) | Product term ( %s %s %s %s %s) \n",
    //              cinfo->cpu_id,
    //              buf2[0],buf2[1],buf2[2],buf2[3], buf2[4]);
	//
#endif

}

void init_weight_matrix(struct core_info *cinfo){
//    const double init_weights[HIST_SIZE]={0.17017401, 0.19412817, 0.17890527, 0.21347153, 0.25384151};
    const double init_weights[HIST_SIZE]={0.1, 0.1, 0.1, 0.1, 0.1};

    for(u8 i =0 ; i < HIST_SIZE; i++){
        cinfo->weight_matrix[i] = init_weights[i];
    }
}
