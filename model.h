//
// Created by ss22 on 25/9/25.
//

#ifndef ADAPTIVEREGULATOR_MODEL_H
#define ADAPTIVEREGULATOR_MODEL_H


#define INITIAL_WEIGHT  0.1f
void initialize_weight_matrix(struct core_info *cinfo);
void update_weight_matrix(s64 error, struct core_info *cinfo );
u64 estimate(u64* feat, u8 feat_len, float *wm, u8 wm_len, u8 index);
#endif //ADAPTIVEREGULATOR_MODEL_H
