/**
    @file   main.cpp
    @author Ryan Allan
    @brief  the main loop for server: reads lime samples, writes samples to buffer for web socket, and converts samples to streamable audio format
 */
#include "lime/LimeSuite.h"
#include <iostream>
#include <chrono>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <AudioSink.hpp>
#include <WAVWriter.hpp>
#ifdef __linux__
#include <AlsaStreamer.hpp>
#endif
#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#endif
#include <RadioStreamer.hpp>
#include <LimeStreamer.hpp>
#include <HackRFStreamer.hpp>
#include <IQWebSocketServer.hpp>
#include <CommandLine.hpp>
#include <fir1_1062_18140589569.hpp>
#include <fir1_100000_17640000_1062.hpp>
#include <thread>
#include <gnuradio/filter/firdes.h>
#include <gnuradio/filter/fir_filter.h>
#include <sin1024.hpp>

#define MAX_PCM 32767

using namespace std;

static int my_channel = 0;
static double mono_band = 15000;
static double tunedFrequnecy = 93.3e6;
static double sample_rate = 17640000;
static int sample_rate_int = sample_rate;
static int frequencyShift = 2000000; 
static double max_difference = ((mono_band/sample_rate) * 2 * M_PI);
static std::unique_ptr<AudioSink> audioWriter;
static std::unique_ptr<RadioStreamer> radioStreamer;


/*static double filter_coeff_one[] = {   //fir1(31, 0.20, hann(32)) - 200 khz LPF for 2205000 Hz sample rate

    0 ,  0.00007909990831961303 ,  0.0007979711131756353 ,  0.002287550802041069  , 0.00340186492199966  , 0.001992922319136589 , -0.003692853174558785 ,
    -0.013137559823147 , -0.02236573965074354 , -0.02449486179409259 , -0.01225211332268774  , 0.01843075743907188,
   0.06543407938285016 ,  0.1197465378212038  , 0.1677775120631539 ,  0.1959948319942774 ,  0.1959948319942774 ,  0.1677775120631539  ,
   0.1197465378212038 ,  0.06543407938285017  , 0.01843075743907188 , -0.01225211332268774 , -0.02449486179409261 , -0.02236573965074356 ,
  -0.013137559823147 , -0.003692853174558785  , 0.001992922319136589 ,  0.003401864921999662  , 0.002287550802041069 ,  0.0007979711131756353 ,  0.00007909990831961388,      0
};
*/

#define TAN_MAP_RES 0.003921569 /* (smallest non-zero value in table) */
#define RAD_PER_DEG 0.017453293
#define TAN_MAP_SIZE 255

static float fast_atan_table[257] = {
    0.000000e+00, 3.921549e-03, 7.842976e-03, 1.176416e-02, 1.568499e-02, 1.960533e-02,
    2.352507e-02, 2.744409e-02, 3.136226e-02, 3.527947e-02, 3.919560e-02, 4.311053e-02,
    4.702413e-02, 5.093629e-02, 5.484690e-02, 5.875582e-02, 6.266295e-02, 6.656816e-02,
    7.047134e-02, 7.437238e-02, 7.827114e-02, 8.216752e-02, 8.606141e-02, 8.995267e-02,
    9.384121e-02, 9.772691e-02, 1.016096e-01, 1.054893e-01, 1.093658e-01, 1.132390e-01,
    1.171087e-01, 1.209750e-01, 1.248376e-01, 1.286965e-01, 1.325515e-01, 1.364026e-01,
    1.402496e-01, 1.440924e-01, 1.479310e-01, 1.517652e-01, 1.555948e-01, 1.594199e-01,
    1.632403e-01, 1.670559e-01, 1.708665e-01, 1.746722e-01, 1.784728e-01, 1.822681e-01,
    1.860582e-01, 1.898428e-01, 1.936220e-01, 1.973956e-01, 2.011634e-01, 2.049255e-01,
    2.086818e-01, 2.124320e-01, 2.161762e-01, 2.199143e-01, 2.236461e-01, 2.273716e-01,
    2.310907e-01, 2.348033e-01, 2.385093e-01, 2.422086e-01, 2.459012e-01, 2.495869e-01,
    2.532658e-01, 2.569376e-01, 2.606024e-01, 2.642600e-01, 2.679104e-01, 2.715535e-01,
    2.751892e-01, 2.788175e-01, 2.824383e-01, 2.860514e-01, 2.896569e-01, 2.932547e-01,
    2.968447e-01, 3.004268e-01, 3.040009e-01, 3.075671e-01, 3.111252e-01, 3.146752e-01,
    3.182170e-01, 3.217506e-01, 3.252758e-01, 3.287927e-01, 3.323012e-01, 3.358012e-01,
    3.392926e-01, 3.427755e-01, 3.462497e-01, 3.497153e-01, 3.531721e-01, 3.566201e-01,
    3.600593e-01, 3.634896e-01, 3.669110e-01, 3.703234e-01, 3.737268e-01, 3.771211e-01,
    3.805064e-01, 3.838825e-01, 3.872494e-01, 3.906070e-01, 3.939555e-01, 3.972946e-01,
    4.006244e-01, 4.039448e-01, 4.072558e-01, 4.105574e-01, 4.138496e-01, 4.171322e-01,
    4.204054e-01, 4.236689e-01, 4.269229e-01, 4.301673e-01, 4.334021e-01, 4.366272e-01,
    4.398426e-01, 4.430483e-01, 4.462443e-01, 4.494306e-01, 4.526070e-01, 4.557738e-01,
    4.589307e-01, 4.620778e-01, 4.652150e-01, 4.683424e-01, 4.714600e-01, 4.745676e-01,
    4.776654e-01, 4.807532e-01, 4.838312e-01, 4.868992e-01, 4.899573e-01, 4.930055e-01,
    4.960437e-01, 4.990719e-01, 5.020902e-01, 5.050985e-01, 5.080968e-01, 5.110852e-01,
    5.140636e-01, 5.170320e-01, 5.199904e-01, 5.229388e-01, 5.258772e-01, 5.288056e-01,
    5.317241e-01, 5.346325e-01, 5.375310e-01, 5.404195e-01, 5.432980e-01, 5.461666e-01,
    5.490251e-01, 5.518738e-01, 5.547124e-01, 5.575411e-01, 5.603599e-01, 5.631687e-01,
    5.659676e-01, 5.687566e-01, 5.715357e-01, 5.743048e-01, 5.770641e-01, 5.798135e-01,
    5.825531e-01, 5.852828e-01, 5.880026e-01, 5.907126e-01, 5.934128e-01, 5.961032e-01,
    5.987839e-01, 6.014547e-01, 6.041158e-01, 6.067672e-01, 6.094088e-01, 6.120407e-01,
    6.146630e-01, 6.172755e-01, 6.198784e-01, 6.224717e-01, 6.250554e-01, 6.276294e-01,
    6.301939e-01, 6.327488e-01, 6.352942e-01, 6.378301e-01, 6.403565e-01, 6.428734e-01,
    6.453808e-01, 6.478788e-01, 6.503674e-01, 6.528466e-01, 6.553165e-01, 6.577770e-01,
    6.602282e-01, 6.626701e-01, 6.651027e-01, 6.675261e-01, 6.699402e-01, 6.723452e-01,
    6.747409e-01, 6.771276e-01, 6.795051e-01, 6.818735e-01, 6.842328e-01, 6.865831e-01,
    6.889244e-01, 6.912567e-01, 6.935800e-01, 6.958943e-01, 6.981998e-01, 7.004964e-01,
    7.027841e-01, 7.050630e-01, 7.073330e-01, 7.095943e-01, 7.118469e-01, 7.140907e-01,
    7.163258e-01, 7.185523e-01, 7.207701e-01, 7.229794e-01, 7.251800e-01, 7.273721e-01,
    7.295557e-01, 7.317307e-01, 7.338974e-01, 7.360555e-01, 7.382053e-01, 7.403467e-01,
    7.424797e-01, 7.446045e-01, 7.467209e-01, 7.488291e-01, 7.509291e-01, 7.530208e-01,
    7.551044e-01, 7.571798e-01, 7.592472e-01, 7.613064e-01, 7.633576e-01, 7.654008e-01,
    7.674360e-01, 7.694633e-01, 7.714826e-01, 7.734940e-01, 7.754975e-01, 7.774932e-01,
    7.794811e-01, 7.814612e-01, 7.834335e-01, 7.853982e-01, 7.853982e-01
};


/*****************************************************************************
 Function: Arc tangent

 Syntax: angle = fast_atan2(y, x);
 float y y component of input vector
 float x x component of input vector
 float angle angle of vector (x, y) in radians

 Description: This function calculates the angle of the vector (x,y)
 based on a table lookup and linear interpolation. The table uses a
 256 point table covering -45 to +45 degrees and uses symmetry to
 determine the final angle value in the range of -180 to 180
 degrees. Note that this function uses the small angle approximation
 for values close to zero. This routine calculates the arc tangent
 with an average error of +/- 3.56e-5 degrees (6.21e-7 radians).
*****************************************************************************/

float fast_atan2f(float y, float x)
{
    float x_abs, y_abs, z;
    float alpha, angle, base_angle;
    int index;

    /* normalize to +/- 45 degree range */
    y_abs = fabsf(y);
    x_abs = fabsf(x);
    /* don't divide by zero! */
    if (!((y_abs > 0.0f) || (x_abs > 0.0f)))
        return 0.0;

    if (y_abs < x_abs)
        z = y_abs / x_abs;
    else
        z = x_abs / y_abs;

    /* when ratio approaches the table resolution, the angle is */
    /* best approximated with the argument itself... */
    if (z < TAN_MAP_RES)
        base_angle = z;
    else {
        /* find index and interpolation value */
        alpha = z * (float)TAN_MAP_SIZE;
        index = ((int)alpha) & 0xff;
        alpha -= (float)index;
        /* determine base angle based on quadrant and */
        /* add or subtract table value from base angle based on quadrant */
        base_angle = fast_atan_table[index];
        base_angle += (fast_atan_table[index + 1] - fast_atan_table[index]) * alpha;
    }

    if (x_abs > y_abs) { /* -45 -> 45 or 135 -> 225 */
        if (x >= 0.0) {  /* -45 -> 45 */
            if (y >= 0.0)
                angle = base_angle; /* 0 -> 45, angle OK */
            else
                angle = -base_angle; /* -45 -> 0, angle = -angle */
        } else {                     /* 135 -> 180 or 180 -> -135 */
            angle = 3.14159265358979323846;
            if (y >= 0.0)
                angle -= base_angle; /* 135 -> 180, angle = 180 - angle */
            else
                angle = base_angle - angle; /* 180 -> -135, angle = angle - 180 */
        }
    } else {            /* 45 -> 135 or -135 -> -45 */
        if (y >= 0.0) { /* 45 -> 135 */
            angle = 1.57079632679489661923;
            if (x >= 0.0)
                angle -= base_angle; /* 45 -> 90, angle = 90 - angle */
            else
                angle += base_angle; /* 90 -> 135, angle = 90 + angle */
        } else {                     /* -135 -> -45 */
            angle = -1.57079632679489661923;
            if (x >= 0.0)
                angle += base_angle; /* -90 -> -45, angle = -90 + angle */
            else
                angle -= base_angle; /* -135 -> -90, angle = -90 - angle */
        }
    }

#ifdef ZERO_TO_TWOPI
    if (angle < 0)
        return (angle + TWOPI);
    else
        return (angle);
#else
    return (angle);
#endif
}

//octave:5> fir1(31,0.18140589569)
const double fir1_31_18140589569[] = { 
   0.0009770276662241166  ,  0.001828764662393893,    0.002702261934747691
    ,0.002966420113660258  ,  0.001436593359110279,   -0.002918947888720856
    ,-0.00994080766670102  ,  -0.01747103249692836,    -0.02135629892974791
    ,-0.01653538826051314  ,  0.001028630436679646,     0.03229171895800226
    , 0.07386627817995932  ,    0.1182546256077054,      0.1557091751030727
    ,  0.1771609792210559  ,    0.1771609792210559,      0.1557091751030727
    ,  0.1182546256077054  ,   0.07386627817995932,     0.03229171895800225
    ,0.001028630436679646  ,  -0.01653538826051315,    -0.02135629892974793
    ,-0.01747103249692836  ,  -0.00994080766670102,   -0.002918947888720857
    ,0.001436593359110279  ,  0.002966420113660259,     0.00270226193474769
    ,0.001828764662393895  , 0.0009770276662241166

};
/*
octave:16> fir1(8,200000/2205000 * 2, hamming(9))
ans =
*/

const double fir1_8_18140589569[] = {
    0.007109195748391492  ,   0.03289048591280717  ,   0.1133549246781276   ,   0.2153076828628636   ,   0.2626754215956201   
    ,   0.2153076828628637   ,   0.1133549246781276   ,  0.03289048591280719  ,  0.007109195748391493
};
/*static double filter_coeff_one[] = {   //fir1(31, 0.40, hann(32)) - 200 khz LPF for 1102500 Hz sample rate
    0 , -0.0001570993780150857 , 0.0003381932094938418 , 0.002275299083516856 , 0.00208612366364782 , -0.004208593800586378 ,
    -0.01072130155763298 , -0.003737411426019404 , 0.01745049151523847 , 0.02736395513988654 , -0.00129673748333856 , -0.05244322844396793 ,
    -0.05938547198644741 , 0.03522995587486178 , 0.2054523332385446 , 0.3417534923508179 , 0.3417534923508179 , 0.2054523332385446 ,
    0.03522995587486179 , -0.05938547198644741 , -0.05244322844396794 , -0.001296737483338559 , 0.02736395513988656 , 0.01745049151523848 ,
    -0.003737411426019404 , -0.01072130155763298 , -0.004208593800586378 , 0.00208612366364782 , 0.002275299083516856 , 0.0003381932094938418 , -0.0001570993780150874 , -0
};*/

const double fir1_31_01360544217[] = {   //b= fir1(31, 0.01360544217, hann(32)) - 15 khz LPF for  2205000 Hz sample rate

    0  , 0.0006297659406518796  ,  0.002511986542558811  ,   0.00559420103232427 ,   0.009769777358151056   ,  0.01488202164192106  ,   
    0.02073063182807278  ,   0.02708021746023739  ,   0.03367050686658642  ,   0.04022777987048493 ,
    0.04647700264810423  ,   0.05215410466516634  ,   0.05701782766715997  ,   0.06086059408579131   ,  0.06351788622624033   ,
    0.0648756961665491   ,  0.06487569616654912   ,  0.06351788622624033   ,  0.06086059408579132  ,   0.05701782766715998  ,
    0.05215410466516635  ,   0.04647700264810422   ,  0.04022777987048495   ,  0.03367050686658645  ,    0.0270802174602374  ,
    0.02073063182807278  ,   0.01488202164192106  ,  0.009769777358151061   ,  0.00559420103232427  ,  0.002511986542558811  ,
    0.0006297659406518865    ,    0
};

/*static double filter_coeff_two[] = {   //b= fir1(31, 0.02721088435, hann(32)) - 15 khz LPF for 1102500 Hz sample rate
    0 , 0.0005324855786808849 , 0.00218305759384411 , 0.004984952569753191 , 0.008906106000274685 , 0.01384819725874714 ,
    0.01964974236607389 , 0.02609314054247059 , 0.03291532251550697 , 0.03982140981843261 , 0.04650059106374654 ,
    0.05264326744191319 , 0.05795842626130267 , 0.06219017499587139 , 0.06513241132751785 , 0.06664071466586417 ,
    0.06664071466586417 , 0.06513241132751786 , 0.06219017499587139 , 0.0579584262613027 , 0.05264326744191319 , 0.04650059106374654 ,
    0.03982140981843263 , 0.032915322515507 , 0.02609314054247059 , 0.0196497423660739 , 0.01384819725874714 , 0.008906106000274687 , 
    0.004984952569753191 , 0.002183057593844111 , 0.0005324855786808907 , 0
};*/

/*octave:3> fir1(32,15000 / 11025000 * 2, hamming(33))
ans =
*/
const double fir1_32_00272108843[] = { 
    0.004600133021839709  ,  0.005109690169428322 ,   0.006616868293422096  ,  0.009064426222919026 ,     0.0123588526562288 ,    0.01637395508531743 ,    0.02095571127705509  ,   0.02592819647844212  ,   0.03110035786248094 ,    0.03627337490338771,
    0.04124832163849204   ,  0.04583383502277232  ,   0.04985349326868679   ,   0.0531526192039059  ,   0.05560424584410847  ,   0.05711401371132122  ,   0.05762381068038389   ,  0.05711401371132123   ,  0.05560424584410847  ,    0.0531526192039059,
    0.04985349326868681   ,  0.04583383502277232  ,   0.04124832163849206   ,  0.03627337490338773  ,   0.03110035786248095  ,   0.02592819647844212  ,   0.02095571127705509   ,  0.01637395508531745   ,   0.0123588526562288  ,  0.009064426222919026,
    0.006616868293422102  ,  0.005109690169428325 ,   0.004600133021839709 
};

/*
octave:8> fir1(32,200000 / 11025000 * 2, hamming(33))
ans =

*/

const double fir1_32_03628117913[] = { 
    0.002765254304032368  ,  0.003330610357653158  ,  0.004639647812226206  ,  0.006788077418317072  ,  0.009819659349730687  ,   0.01371951785536016  ,   0.01841145293950171  ,    0.0237595798913968  ,    0.0295743072063702  ,   0.03562233612311923,
       0.041640056185582  ,   0.04734944531763791  ,   0.05247538115021944  ,   0.05676314918445385  ,   0.05999490302260862  ,   0.06200389549045749  ,   0.06268545278266617  ,   0.06200389549045749  ,   0.05999490302260862  ,   0.05676314918445385,
     0.05247538115021945  ,   0.04734944531763791  ,   0.04164005618558202  ,   0.03562233612311925  ,   0.02957430720637021  ,    0.0237595798913968  ,   0.01841145293950171  ,   0.01371951785536017  ,  0.009819659349730689  ,  0.006788077418317072,
     0.00463964781222621  ,   0.00333061035765316  ,  0.002765254304032367  
};

/*
octave:9> fir1(32, 200000 / 17640000 * 2  , hamming(33))
ans =
*/

const double fir1_32_02267573696[] = { 
    0.003868267330699852  ,  0.004409262231117821  ,  0.005848072806672679  ,  0.008189895432565177   ,  0.01139476035988034   ,   0.0153780328267767  ,   0.02001329095866513   ,  0.02513747420941909  ,   0.03055806761694293  ,   0.03606197025034354,
     0.04142559781276156  ,    0.0464256953117383  ,   0.05085029051617159  ,   0.05450920542713553   ,  0.05724356220525168   ,  0.05893377102092068  ,   0.05950556736587481   ,  0.05893377102092069  ,   0.05724356220525168  ,   0.05450920542713553,
      0.0508502905161716  ,    0.0464256953117383  ,   0.04142559781276158  ,   0.03606197025034355   ,  0.03055806761694293   ,  0.02513747420941909  ,   0.02001329095866513   ,  0.01537803282677671  ,   0.01139476035988035  ,  0.008189895432565179,
    0.005848072806672685  ,  0.004409262231117823  ,  0.003868267330699852
};
/*
octave:10> fir1(32, 15000 / 17640000 * 2  , hamming(33))
ans =
*/
const double fir1_32_00170068027[] = { 
    0.004608294930875577  ,  0.005117440726642514 ,   0.006625311926544246  ,  0.009073961885992465  ,    0.0123692903602643  ,   0.01638465972413496  ,   0.02096576158479601   ,  0.02593654676685374  ,   0.03110599078341014  ,   0.03627543479996653,
     0.04124621998202427  ,   0.04582732184268531 ,    0.04984269120655598  ,   0.05313801968082782  ,   0.05558666964027603  ,   0.05709454084017777  ,    0.0576036866359447   ,  0.05709454084017777  ,   0.05558666964027603  ,   0.05313801968082782,
     0.04984269120655599  ,   0.04582732184268531 ,    0.04124621998202429  ,   0.03627543479996655  ,   0.03110599078341015  ,   0.02593654676685374  ,   0.02096576158479601   ,  0.01638465972413498  ,    0.0123692903602643  ,  0.009073961885992465,
    0.006625311926544253  ,  0.005117440726642517 ,   0.004608294930875577
};
/*
octave:14>   fir1(39, 15000 / 17640000 * 2  , hamming(40))
ans =
*/
const double fir1_39_00170068027[] = { 
    0.003784295175023653   , 0.004066077539572379  ,  0.004904126617990215  ,  0.006276737379353051  ,  0.008148359925940846  ,   0.01047052021691506  ,   0.01318307552051411  ,   0.01621577207921453  ,   0.01949006464514815  ,   0.02292115076076403,
      0.0264201670979343   ,  0.02989649097147495  ,   0.03326008741909019  ,   0.03642384105960266  ,    0.0393058123355664  ,   0.04183135970476379  ,   0.04393507281694162  ,    0.0455624666075241  ,   0.04667139243216576  ,   0.04723312969450032,
     0.04723312969450032   ,  0.04667139243216576  ,    0.0455624666075241  ,   0.04393507281694163  ,    0.0418313597047638  ,    0.0393058123355664  ,   0.03642384105960265  ,   0.03326008741909019  ,   0.02989649097147496  ,    0.0264201670979343,
     0.02292115076076404   ,  0.01949006464514816  ,   0.01621577207921454  ,   0.01318307552051412  ,   0.01047052021691506  ,  0.008148359925940846  ,  0.006276737379353048  ,  0.004904126617990212  ,  0.004066077539572379  ,  0.003784295175023653
};

void unwrap_array(double *in, double *out, int len) {
    out[0] = in[0];
    for (int i = 1; i < len; i++) {
        double d = in[i] - in[i-1];
        d = d > M_PI ? d - 2 * M_PI : (d < -M_PI ? d + 2 * M_PI : d);
        out[i] = out[i-1] + d;
    }
}

//Device structure, should be initialize to NULL
lms_device_t* device = NULL;

int error()
{
    audioWriter.reset();
    if (device != NULL)
        radioStreamer->closeDevice();
    exit(-1);
}

int main(int argc, char** argv)
{
	CommandLine commandLine(argc, argv);
    const int sampleCnt = 10400; 

#ifdef _linux_

	if (commandLine.audio == wav) {
		audioWriter = std::make_unique<WAVWriter>("./radio.wav", 44100, sampleCnt/(sample_rate/44100));
	}
	else {
		audioWriter = std::make_unique<AlsaStreamer>("hw:0,1,0", 44100, sampleCnt/(sample_rate/44100));
	}
#else
        audioWriter = std::make_unique<WAVWriter>("./radio.wav", 44100, sampleCnt/(sample_rate/44100));
#endif

    if (commandLine.radio == lime) {
        radioStreamer = std::make_unique<LimeStreamer>();
    }
    else {
        radioStreamer = std::make_unique<HackRFStreamer>();
    }
	

    //open radio device
    if (radioStreamer->openDevice() < 0)
        error();

    //Set center frequency to 800 MHz
    if (radioStreamer->setFrequency(tunedFrequnecy/*89.7e6*/) < 0)
        error();

    if (radioStreamer->setSampleRate(sample_rate) < 0)
        error(); 

    if (radioStreamer->setFilterBandwidth(10e6) < 0)
    	error();


    auto my_server = std::make_unique<IQWebSocketServer>(8079, "^/socket/?$");
    if (commandLine.server == on) {
        my_server->run();
    }


    //Initialize stream
    if (radioStreamer->setupStream() < 0)
        error();

    int indexSignal = 0;
    float sineSignal = 0;

    double max_Sample = 0;
    double min_Sample = 0;
    
    FILE * rawCSV;
    FILE * shiftedCSV;
    FILE * filteredCSV;
    FILE * sineCSV;
    FILE * rawFFT;
    FILE * shiftedFFT;
    int csvCounter = 0;
    int filteredCSVCounter = 0;
    int sineCSVCounter = 0;
    bool rawFFTWritten = false;
    bool shiftedFFTWritten = false;
    printf("OPEN FILES\n");
    rawCSV = fopen("./raw.csv","w");
    shiftedCSV = fopen("./shifted.csv","w");
    filteredCSV = fopen("./filtered.csv","w");
    sineCSV = fopen("./sine.csv", "w");
    rawFFT = fopen("./rawFFT.csv","w");
    shiftedFFT = fopen("./shiftedFFT.csv","w");
    printf("OPENED FILES\n");

    double normalized_fir1_1062_18140589569[1063];
    double fmax = fir1_1062_18140589569[531];

    for (int i = 532; i < 1063; i++) {
        fmax += 2 * fir1_1062_18140589569[i];
    }

    double gain = 1.0 / fmax;

    for (int i = 0; i < 1063; i++) {
        normalized_fir1_1062_18140589569[i] = gain * fir1_1062_18140589569[i];
    }

    double normalized_fir1_100000_17640000_1062[1063];
    fmax = fir1_100000_17640000_1062[531];

    for (int i = 532; i < 1063; i++) {
        fmax += 2 * fir1_100000_17640000_1062[i];
    }

    gain = 1.0 / fmax;

    for (int i = 0; i < 1063; i++) {
        normalized_fir1_100000_17640000_1062[i] = gain * fir1_100000_17640000_1062[i];
    }

    double normalized_fir1_8_18140589569[9];
    fmax = fir1_8_18140589569[4];

    for (int i = 5; i < 9; i++) {
        fmax += 2 * fir1_8_18140589569[i];
    }

    gain = 1.0 / fmax;

    for (int i = 0; i < 9; i++) {
        normalized_fir1_8_18140589569[i] = gain * fir1_8_18140589569[i];
    }

    double normalized_fir1_31_18140589569[32];
    fmax = 0;

    for (int i = 0; i < 32; i++) {
        fmax += fir1_31_18140589569[i];
    }

    gain = 1.0 / fmax;

    for (int i = 0; i < 32; i++) {
        normalized_fir1_31_18140589569[i] = gain * fir1_31_18140589569[i];
    }

    double normalized_fir1_31_01360544217[32];
    fmax = 0;

    for (int i = 0; i < 32; i++) {
        fmax += fir1_31_01360544217[i];
    }

    gain = 1.0 / fmax;

    for (int i = 0; i < 32; i++) {
        normalized_fir1_31_01360544217[i] = gain * fir1_31_01360544217[i];
    }

    double normalized_fir1_32_02267573696[33];
    fmax = 0;

    for (int i = 0; i < 33; i++) {
        fmax += fir1_32_02267573696[i];
    }

    gain = 1.0 / fmax;

    for (int i = 0; i < 33; i++) {
        normalized_fir1_32_02267573696[i] = gain * fir1_32_02267573696[i];
    }

    double normalized_fir1_32_00170068027[33];
    fmax = 0;

    for (int i = 0; i < 33; i++) {
        fmax += fir1_32_00170068027[i];
    }

    gain = 1.0 / fmax;

    for (int i = 0; i < 33; i++) {
        normalized_fir1_32_00170068027[i] = gain * fir1_32_00170068027[i];
    }

    double normalized_fir1_39_00170068027[40];
    fmax = 0;

    for (int i = 0; i < 40; i++) {
        fmax += fir1_39_00170068027[i];
    }

    gain = 1.0 / fmax;

    for (int i = 0; i < 40; i++) {
        normalized_fir1_39_00170068027[i] = gain * fir1_39_00170068027[i];
    }

    std::vector gnuradio_filter_taps = gr::filter::firdes::low_pass(1.0,
                                 17640000, // Hz
                                 200000,   // Hz BEGINNING of transition band
                                 5000, // Hz width of transition band
                                 gr::fft::window::win_type::WIN_HAMMING,
                                 6.76);



    double * gnuradio_filter_double = (double *) malloc(gnuradio_filter_taps.size() * sizeof(double));

    for (int i = 0; i<gnuradio_filter_taps.size(); i++) {
        //printf("Here is tap: %f\n", gnuradio_filter_taps[i]);
        gnuradio_filter_double[i] = (double)gnuradio_filter_taps[i];
        //printf("Here is tap: %f\n", gnuradio_filter_double[i]);
    }

    gr::filter::kernel::fir_filter gnuradio_filter = gr::filter::kernel::fir_filter<float,float,float>(gnuradio_filter_taps);

    double * filter_coeff_one = normalized_fir1_32_02267573696;
    double * filter_coeff_two = normalized_fir1_32_00170068027;
    const int filterLengthOne = 33;
    const int filterLengthTwo = 33;

    const int stageOneDecimation = 40;
    const int stageTwoDecimation = 10;

    bool sinFlipper = false;

    //Initialize data buffers
    //complex samples per buffer
    const int afterFirstFilterCnt = sampleCnt/stageOneDecimation;
    const int downSampleCnt = (sampleCnt/stageOneDecimation)/stageTwoDecimation;
    int bufferCount = sampleCnt * 2 + 2 * filterLengthOne + 2 * filterLengthTwo * stageOneDecimation;
    int bufferOffset = 2 * filterLengthOne + 2 * filterLengthTwo * stageOneDecimation;
    int16_t buffer[bufferCount]; //buffer to hold complex values (2*samples))
    printf("Here is last index: %d\n", sampleCnt * 2 + 2 * filterLengthOne + 2 * filterLengthTwo * stageOneDecimation);

    

    double bufferShifted[bufferCount];
    
    uint32_t audio_sample_count = 0;

    fftw_complex *fft_in;
    fftw_complex *fft_out;
    fftw_plan fft_plan;

    fft_in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * 2048);
    fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * 2048);
    fft_plan = fft_plan = fftw_plan_dft_1d(2048, fft_in, fft_out, FFTW_FORWARD, FFTW_MEASURE);

    //Start streaming
    if (radioStreamer->startStream() < 0)
        error();

    double bufferFiltered[sampleCnt * 2 + 2 * filterLengthTwo * stageOneDecimation + 2 * stageOneDecimation];
    double decimatedFilter[afterFirstFilterCnt * 2 + filterLengthTwo * 2 + 1 * 2];
    double realDecimatedFilter[afterFirstFilterCnt + filterLengthTwo + 1];
    double imagDecimatedFilter[afterFirstFilterCnt + filterLengthTwo + 1];
    double wrapped[afterFirstFilterCnt + filterLengthTwo + 1];
    double unwrapped[afterFirstFilterCnt + filterLengthTwo + 1];
    double difference[afterFirstFilterCnt + filterLengthTwo];
    double demodded[afterFirstFilterCnt + filterLengthTwo];
    double convert_to_pcm[afterFirstFilterCnt];
    double compare_to_pcm[afterFirstFilterCnt];
    int16_t downsampled_audio[downSampleCnt];

    const float shiftFactor = frequencyShift/sample_rate;
    printf("Here is shiftfactor: %f\n", shiftFactor);

    auto t1 = chrono::high_resolution_clock::now();
    while ( ((commandLine.audio == alsa) ? true : false) || (chrono::high_resolution_clock::now() - t1 < chrono::seconds(12)) )
    {   
        //Receive samples
        int samplesRead = radioStreamer->receiveSamples(&(buffer[bufferOffset]), sampleCnt);

        //printf("Here is realCosine: %f\n", cos(2*M_PI*((double) cosineIndexInphase/shifter)));
        //printf("Here is imagCosine: %f\n", cos(2*M_PI*((double) cosineIndexQuadrature/shifter)));
        if (!rawFFTWritten) {
            for (int i = 0; i<2048; i++) {
                fft_in[i][0] = buffer[bufferOffset + 2*i] * hann2048[i];
                fft_in[i][1] = 1 * buffer[bufferOffset + 2*i + 1] * hann2048[i];

            }
            fftw_execute(fft_plan);
            for (int i = 0; i<2048; i++) {
                std::complex<double> bin(fft_out[i][0], fft_out[i][1]);
                double modulus = std::abs(bin);
                fprintf(rawFFT, "%d,%f,\n",i,modulus);
            }
            rawFFTWritten = true;
        }
        for (int i = 0; i < (samplesRead); ++i) {
            float junk;
            sineSignal = modf( sineSignal, &junk); //remainder(((float) indexSignal) * shiftFactor, 1);
            int inPhaseIndex =  1024 * sineSignal;
            int quadratureIndex = ((int)(1024 * sineSignal) + 256) % 1024;
            if (inPhaseIndex >= 1024) {
                inPhaseIndex = 1023;
            }
            if (quadratureIndex >= 1024) {
                quadratureIndex = 1023;
            }

            //double realCosine = sinFlipper ? -1.0 : 1.0;
            const float & imagCosine = sin1024[inPhaseIndex];
            //double imagCosine = 0.0;
            const float & realCosine = sin1024[quadratureIndex];
            /*if (sineCSVCounter < 50000) {
                fprintf(sineCSV, "%d,%f,%f\n",sineCSVCounter,realCosine, imagCosine);
                sineCSVCounter++;
            }*/
            //printf("BEFORE BUFFER ACESS\n");
            float  bufferReal = ((float) buffer[bufferOffset + 2*i])/128.0;
            float  bufferImag = ((float) buffer[bufferOffset + 2*i+1])/128.0;
            //printf("AFTER BUFFER ACESS\n");
            
            /*if (csvCounter < 50000) {
                fprintf(rawCSV, "%d,%f,%f\n",csvCounter,bufferReal, bufferImag);
            }*/
            
            //printf("Here is bufferReal: %f\n", bufferReal);
            //printf("Here is bufferImag: %f\n", bufferImag);

            bufferShifted[bufferOffset + 2*i] = bufferReal * realCosine - bufferImag * imagCosine;
            bufferShifted[bufferOffset + 2*i+1] = bufferReal * imagCosine + bufferImag * realCosine;
            
            /*if (csvCounter < 50000) {
                fprintf(shiftedCSV,"%d,%f,%f\n",csvCounter,bufferShifted[2 * filterLengthOne + 2 * filterLengthTwo + 2 + 2*i], bufferShifted[2 * filterLengthOne + 2 * filterLengthTwo + 2 + 2*i+1]);
                csvCounter++;
            }*/
            
            //cosineIndexInphase = (cosineIndexInphase + 1) % sample_rate;
            //cosineIndexQuadrature = (cosineIndexQuadrature + 1) % sample_rate;
            //sinFlipper = !sinFlipper;
            //indexSignal = (indexSignal + 1) % sample_rate_int;
            sineSignal += shiftFactor;
        }

        if (!shiftedFFTWritten) {
            for (int i = 0; i<2048; i++) {
                fft_in[i][0] = bufferShifted[bufferOffset + 2*i] * hann2048[i];
                fft_in[i][1] = 1 * bufferShifted[bufferOffset + 2*i]  * hann2048[i];
            }
            fftw_execute(fft_plan);
            for (int i = 0; i<2048; i++) {
                std::complex<double> bin(fft_out[i][0], fft_out[i][1]);
                double modulus = std::abs(bin);
                fprintf(shiftedFFT, "%d,%f,\n",i,modulus);
            }
            shiftedFFTWritten = true;
        }
        
        //printf("HERE IS BUFFERSHIFTED: %f and imag: %f\n", bufferShifted[4 * filter_order + 2], bufferShifted[4 * filter_order + 2+1]);

        if (samplesRead != sampleCnt) {
            printf("Unexpected sample read, here is read: %d\n", samplesRead);
            break;
        }
        else if (chrono::high_resolution_clock::now() - t1 < chrono::seconds(3)) {
            memcpy(buffer, &(buffer[sampleCnt * 2]), (bufferOffset) * sizeof(int16_t));
            memcpy(bufferShifted, &(bufferShifted[sampleCnt * 2]), (bufferOffset)* sizeof(double));
        }
        else {  
        //printf("START REAL LOOP\n");
        memset(bufferFiltered, 0, (sampleCnt * 2 + 2 * filterLengthTwo * stageOneDecimation + 2 * stageOneDecimation) * sizeof(double));

        memset(decimatedFilter, 0, (afterFirstFilterCnt * 2 + filterLengthTwo * 2 + 2) * sizeof(double));

        memset(realDecimatedFilter, 0, (afterFirstFilterCnt  + filterLengthTwo + 1) * sizeof(double));

        memset(imagDecimatedFilter, 0, (afterFirstFilterCnt + filterLengthTwo + 1) * sizeof(double));

        memset(wrapped, 0, (afterFirstFilterCnt+filterLengthTwo + 1) * sizeof(double));

        memset(unwrapped, 0, (afterFirstFilterCnt+filterLengthTwo + 1) * sizeof(double));

        memset(difference, 0, (afterFirstFilterCnt+filterLengthTwo) * sizeof(double));

        memset(demodded, 0, (afterFirstFilterCnt+filterLengthTwo) * sizeof(double));

        memset(convert_to_pcm, 0, afterFirstFilterCnt * sizeof(double));

        memset(downsampled_audio, 0, downSampleCnt * sizeof(int16_t));
    
        //printf("Received buffer with %d samples with first sample %lu microseconds\n", samplesRead, my_stream_data.timestamp);
        try {
            //printf("Here is buffer value: %d\n", buffer[0]);
            //printf("here is buffer imag value: %d\n", buffer[1]);
            //printf("Here is bufferShifted value: %lf\n", bufferShifted[0]);
/*
	        for (int i = filterLengthOne; i < (samplesRead+((2 * filterLengthOne + 2 * filterLengthTwo * stageOneDecimation + 2 * stageOneDecimation)/2)); ++i) {
#ifdef __APPLE__
                vDSP_dotprD(&bufferShifted[2*i-2*filterLengthOne], 2, filter_coeff_one, 1, &bufferFiltered[2*i-(2*filterLengthOne)], filterLengthOne);
                vDSP_dotprD(&bufferShifted[2*i + 1 -2*filterLengthOne], 2, filter_coeff_one, 1, &bufferFiltered[2*i + 1-(2*filterLengthOne)], filterLengthOne);
#else
	            for (int j = i-filterLengthOne, x = 0; j < i; ++j, ++x) {
	                bufferFiltered[2*i-(2*filterLengthOne)] = bufferFiltered[2*i-(2*filterLengthOne)] + bufferShifted[2*j] * filter_coeff_one[x];
	                bufferFiltered[2*i+1-(2*filterLengthOne)] = bufferFiltered[2*i+1-(2*filterLengthOne)] + bufferShifted[2*j+1] * filter_coeff_one[x];
                    
	            }
#endif
                filteredCSVCounter++;
	        }

for (int i = 0, j = 0;
                i < (samplesRead * 2 + (2 * filterLengthTwo * stageOneDecimation + 2 * stageOneDecimation)), j < afterFirstFilterCnt * 2 + filterLengthTwo * 2 + 2;
                i = i + stageOneDecimation, j++) {
                decimatedFilter[j] = bufferFiltered[i];
            }
*/
/*
#ifdef __APPLE__
            vDSP_desampD(bufferShifted, 2*stageOneDecimation, filter_coeff_one, realDecimatedFilter, afterFirstFilterCnt  + filterLengthTwo + 1, filterLengthOne);
            vDSP_desampD(&bufferShifted[1], 2*stageOneDecimation, filter_coeff_one, imagDecimatedFilter, afterFirstFilterCnt  + filterLengthTwo + 1, filterLengthOne);
#else
            
            for (int n = 0; n < afterFirstFilterCnt  + filterLengthTwo + 1; n++) {
                double realSum = 0;
                double imagSum = 0;
                for (p = 0;  p < filterLengthOne; ++p) {
                    realSum += bufferShifted[n * stageOneDecimation * 2 + 2*p] * filter_coeff_one[p];
                    imagSum =+ bufferShifted[n * stageOneDecimation * 2 + 1 + 2*p] * filter_coeff_one[p];
                }
                decimatedFilter[2*n] = realSum;
                decimatedFilter[2*n+1] = imagSum;
            }
#endif
*/
            for (int n = 0; n < afterFirstFilterCnt  + filterLengthTwo + 1; n++) {
#ifdef __APPLE__
                vDSP_dotprD(&bufferShifted[n * stageOneDecimation * 2], 2, filter_coeff_one, 1, &realDecimatedFilter[n], filterLengthOne);
                vDSP_dotprD(&bufferShifted[n * stageOneDecimation * 2 + 1] , 2, filter_coeff_one, 1, &imagDecimatedFilter[n], filterLengthOne);
#else
                double realSum = 0;
                double imagSum = 0;
                for (int p = 0;  p < filterLengthOne; ++p) {
                    realSum += bufferShifted[n * stageOneDecimation * 2 + 2*p] * filter_coeff_one[p];
                    imagSum += bufferShifted[n * stageOneDecimation * 2 + 1 + 2*p] * filter_coeff_one[p];
                }
                decimatedFilter[2*n] = realSum;
                decimatedFilter[2*n+1] = imagSum;
#endif
            }
            if (commandLine.server == on) {
	           my_server->writeToBuffer((&(bufferFiltered[2 * filterLengthTwo + 2])));
            }

	        int samplesFiltered = afterFirstFilterCnt+filterLengthTwo + 1;
            /*

	        for (int i = 0; i < (samplesFiltered); ++i) {
	            double first = atan2( (double) bufferFiltered[2*i+1], (double) bufferFiltered[2*i]);
	            wrapped[i] = first;
	        }

	        unwrap_array(wrapped, unwrapped, (samplesFiltered));

	        for (int i = 0; i < (samplesFiltered-1); ++i) {
	            difference[i] = unwrapped[i+1] - unwrapped[i];
	        }
            */
#ifdef __APPLE__
            for (int i = 0; i < (samplesFiltered-1); ++i) {
                if (filteredCSVCounter < 1000) {
                    fprintf(filteredCSV, "%d,%f,%f\n",filteredCSVCounter,realDecimatedFilter[i], imagDecimatedFilter[i]);
                    filteredCSVCounter++;
                }
                double &undelayedReal = realDecimatedFilter[i];
                double &undelayedComplex = imagDecimatedFilter[i];
                double &delayedReal = realDecimatedFilter[i+1];
                double &delayedComplex = imagDecimatedFilter[i+1];
                double productReal = undelayedReal * delayedReal - ((-undelayedComplex) * delayedComplex);
                double productComplex = undelayedReal * delayedComplex + ((-undelayedComplex) * delayedReal);
                demodded[i] = (double) fast_atan2f((float)productComplex, (float)productReal);
            }
#else       
            for (int i = 0; i < (samplesFiltered-1); ++i) {
                double &undelayedReal = decimatedFilter[2*i];
                double &undelayedComplex = decimatedFilter[2*i+1];
                double &delayedReal = decimatedFilter[2*i+2];
                double &delayedComplex = decimatedFilter[2*i+3];
                double productReal = undelayedReal * delayedReal - (-1 * undelayedComplex) * delayedComplex;
                double productComplex = undelayedReal * delayedComplex + (-1 * undelayedComplex) * delayedReal;
                demodded[i] = (double) fast_atan2f((float)productComplex, (float)productReal);
            }
#endif
            //printf("Here demodded: %lf\n", demodded[0]);

	        for (int i = filterLengthTwo; i < (samplesFiltered-1); ++i) {
#ifdef __APPLE__
                vDSP_dotprD(&demodded[i-filterLengthTwo],1,filter_coeff_two, 1, &convert_to_pcm[i-filterLengthTwo], filterLengthTwo);
#else
	            for (int j = i-filterLengthTwo, x = 0; j < i; ++j, ++x) {
	                convert_to_pcm[i-filterLengthTwo] =  convert_to_pcm[i-filterLengthTwo] + demodded[j] * filter_coeff_two[x];
	            }
#endif
	        }

            //printf("Here convert to pcm: %lf\n", convert_to_pcm[0]);

	        for (int i = 0; i < ((afterFirstFilterCnt)/stageTwoDecimation); ++i) {
	            downsampled_audio[i] = (int16_t) ((convert_to_pcm[i * stageTwoDecimation]) * (MAX_PCM/2));
	            if (downsampled_audio[i] >= MAX_PCM) {
                    printf("GOT MAX PCM\n");
	                downsampled_audio[i] = MAX_PCM;
	            }
	        }
	        if (audioWriter->writeSamples((int16_t *) downsampled_audio) < 0) {
	            break;
	        }

        }
        catch (int e) {
            printf("I got an exception: %d\n", e);
            break;
        }
            memcpy(buffer, &(buffer[sampleCnt * 2]), (bufferOffset) * sizeof(int16_t));
            memcpy(bufferShifted, &(bufferShifted[sampleCnt * 2]), (bufferOffset)* sizeof(double));
        }
    
    }
    printf("STOP STREAM\n");
    printf("here is max_Sample: %f\n", max_Sample);
    printf("here is min_Sample: %f\n", min_Sample);
    //Stop streaming
    radioStreamer->stopStream();
    radioStreamer->destroyStream();

    //Close device
    radioStreamer->closeDevice();
    radioStreamer.reset();

    audioWriter.reset();
    fclose(rawCSV);
    fclose(shiftedCSV);
    fclose(filteredCSV);
    fclose(sineCSV);
    fclose(rawFFT);
    fclose(shiftedFFT);
    free(gnuradio_filter_double);
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(100ms);

    return 0;
}