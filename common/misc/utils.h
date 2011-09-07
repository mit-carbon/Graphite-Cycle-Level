#ifndef UTILS_H
#define UTILS_H

#include "fixed_types.h"
#include <assert.h>
#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <vector>
#include <string>
#include <typeinfo>

using namespace std;

inline unsigned long long rdtscll(void)
{
   unsigned int _a, _d;
   asm volatile("rdtsc" : "=a"(_a), "=d"(_d));
   return ((unsigned long long) _a) | (((unsigned long long) _d) << 32);
}


string myDecStr(UInt64 v, UInt32 w);


#define safeFDiv(x) (x ? (double) x : 1.0)

// Checks if n is a power of 2.
// returns true if n is power of 2

bool isPower2(UInt32 n);


// Computes floor(log2(n))
// Works by finding position of MSB set.
// returns -1 if n == 0.

SInt32 floorLog2(UInt32 n);


// Computes floor(log2(n))
// Works by finding position of MSB set.
// returns -1 if n == 0.

SInt32 ceilLog2(UInt32 n);

// Checks if (n) is a perfect square

bool isPerfectSquare(UInt32 n);

// Is Even and Is Odd ?

bool isEven(UInt32 n);
bool isOdd(UInt32 n);

// Max and Min functions
template <class T>
T getMin(T v1, T v2)
{
   return (v1 < v2) ? v1 : v2;
}

template <class T>
T getMin(T v1, T v2, T v3)
{
   if ((v1 < v2) && (v1 < v3))
      return v1;
   else if (v2 < v3)
      return v2;
   else
      return v3;
}

template <class T>
T getMax(T v1, T v2)
{
   return (v1 > v2) ? v1 : v2;
}

// Use this only for basic data types
// char, int, float, double

template <class T>
void convertFromString(T& t, const string& s)
{
   istringstream iss(s);
   if ((iss >> t).fail())
   {
      fprintf(stderr, "Conversion from (std::string) -> (%s) FAILED\n", typeid(t).name());
      exit(EXIT_FAILURE);
   }
}

// Trim the beginning and ending spaces in a string

string trimSpaces(string& str);

// Parse an arbitrary list separated by arbitrary delimiters
// into a vector of strings

void parseList(string& list, vector<string>& vec, string delim);

// Concatenate strings

char* sstrcat(char* str, const char* fmt, ...);

#endif
