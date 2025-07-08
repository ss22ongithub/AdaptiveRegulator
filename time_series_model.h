
#include "eml_net.h"
static const float time_series_model_layer_0_biases[12] = { -1.405830f, 0.000000f, 1.058175f, 0.000000f, 0.000000f, 0.883271f, 1.050414f, 0.000000f, 1.075633f, -1.103938f, 1.017943f, 0.000000f };
static const float time_series_model_layer_0_weights[36] = { 0.424910f, -0.617231f, 0.462157f, -0.500200f, 0.366649f, -0.515248f, 0.238410f, -0.318677f, 0.384231f, -0.539888f, 0.259226f, 0.509375f, 0.078963f, -0.311670f, -0.342137f, 0.223903f, -0.045316f, -0.024355f, -0.193913f, 0.133534f, -0.430114f, 0.485252f, 0.465357f, -0.602262f, -0.424655f, -0.165646f, 0.093303f, -0.559803f, -0.464996f, 0.672417f, 0.533383f, -0.359063f, 0.193969f, 0.164395f, 0.365934f, -0.018932f };
static const float time_series_model_layer_1_biases[8] = { 1.066216f, 0.000000f, 1.065705f, 0.000000f, 0.000000f, 0.562958f, 1.069483f, 1.061676f };
static const float time_series_model_layer_1_weights[96] = { -0.388685f, 0.228887f, -0.274922f, -0.086670f, 0.514503f, -0.315424f, -0.084950f, -0.680914f, -0.069866f, 0.408404f, 0.077253f, 0.120301f, -0.250704f, -0.111704f, 0.491162f, -0.031630f, 0.400442f, -0.171415f, -0.278877f, 0.340885f, 0.466023f, -0.591862f, 0.502298f, 0.048959f, 0.481344f, 0.045574f, 0.475672f, 0.251822f, 0.032510f, -0.436725f, 0.409355f, -0.220379f, 0.059033f, -0.135041f, 0.195123f, -0.440642f, -0.417909f, 0.338670f, -0.286996f, 0.225764f, 0.368916f, 0.034778f, 0.569974f, -0.173145f, 0.377049f, 0.693104f, 0.142321f, 0.144574f, 0.163349f, -0.157750f, 0.560225f, 0.125229f, -0.542759f, -0.444043f, -0.135313f, 0.163877f, -0.322093f, -0.106746f, -0.228970f, -0.542709f, -0.416493f, -0.378669f, -0.175765f, -0.063774f, 0.286647f, 0.097500f, 0.200775f, 0.492067f, 0.167972f, -0.286313f, 0.631575f, 0.138774f, -0.387800f, 0.159547f, 0.247101f, -0.236238f, -0.241766f, -0.094046f, -0.090957f, 0.294720f, 0.208120f, -0.189444f, 0.011783f, -0.290365f, -0.382227f, 0.328126f, 0.152854f, 0.389092f, 0.157585f, -0.127660f, -0.352016f, -0.223722f, 0.247997f, -0.010638f, 0.145549f, -0.098987f };
static const float time_series_model_layer_2_biases[1] = { 1.072221f };
static const float time_series_model_layer_2_weights[8] = { 0.706443f, -0.155747f, 0.474218f, 0.698342f, 0.245595f, -0.189082f, 0.651963f, 0.538827f };
static float time_series_model_buf1[12];
static float time_series_model_buf2[12];
static const EmlNetLayer time_series_model_layers[3] = { 
{ 12, 3, time_series_model_layer_0_weights, time_series_model_layer_0_biases, EmlNetActivationRelu }, 
{ 8, 12, time_series_model_layer_1_weights, time_series_model_layer_1_biases, EmlNetActivationRelu }, 
{ 1, 8, time_series_model_layer_2_weights, time_series_model_layer_2_biases, EmlNetActivationIdentity } };
static EmlNet time_series_model = { 3, time_series_model_layers, time_series_model_buf1, time_series_model_buf2, 12 };

    int32_t
    time_series_model_predict(const float *features, int32_t n_features)
    {
        return eml_net_predict(&time_series_model, features, n_features);
    }
    

    int32_t
    time_series_model_regress(const float *features, int32_t n_features, float *out, int32_t out_length)
    {
        return eml_net_regress(&time_series_model, features, n_features, out, out_length);
    }
    

    float
    time_series_model_regress1(const float *features, int32_t n_features)
    {
        return eml_net_regress1(&time_series_model, features, n_features);
    }
    