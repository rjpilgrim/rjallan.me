#include <vector>

/*
octave:16> fir1(8,200000/2205000 * 2, hamming(9))
ans =
*/
const double fir1_8_18140589569[] = {
    0.007109195748391492  ,   0.03289048591280717  ,   0.1133549246781276   ,   0.2153076828628636   ,   0.2626754215956201   
    ,   0.2153076828628637   ,   0.1133549246781276   ,  0.03289048591280719  ,  0.007109195748391493
};

std::array<double,9> normalized_fir1_8_18140589569() 
{
  std::array<double,9> normalized_fir1_8_18140589569;
  double fmax = 0;
  for (int i = 0; i < 9; i++) {
    fmax += fir1_8_18140589569[i];
  }
  double gain = 1.0 / fmax;

  for (int i = 0; i < 9; i++) {
      normalized_fir1_8_18140589569[i] = gain * fir1_8_18140589569[i];
  }
  return normalized_fir1_8_18140589569;
};