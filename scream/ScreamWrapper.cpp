#include "ScreamWrapper.h"
#include "ScreamRx.h"
#include <cassert>

int screamInit(){
	  ScreamRx* screamRx = new ScreamRx(10, 10, 10);
	  assert(screamRx != 0);
	  return 0;
};