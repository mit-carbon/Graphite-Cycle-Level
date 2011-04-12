#pragma once

class RandNum
{
public:
   RandNum() : _start(0), _end(1) {}
   RandNum(double start, double end, long int seed = 0)
      : _start(start), _end(end)
   { 
      srand48_r(seed, &rand_buffer); 
   }
   ~RandNum() {}

   double next()
   {
      double result;
      drand48_r(&rand_buffer, &result);
      double num = result * (_end - _start) + _start;
      return num;
   }

private:
   struct drand48_data rand_buffer;
   double _start;
   double _end;
};
