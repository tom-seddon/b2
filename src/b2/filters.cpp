#include <shared/system.h>
#include "filters.h"
#include <shared/debug.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <vector>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const size_t MAX_FILTER_WIDTH=1024;

static std::vector<float> g_filters[MAX_FILTER_WIDTH];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static double sinc(double x) {
    if(x==0) {
        return 1.;
    } else {
        return sin(x)/x;
    }
}

static double L(double x,int a) {
    if(x==0) {
        return 1.;
    } else if(x>-a&&x<a) {
        return sinc(x)/sinc(x/a);
    } else {
        return 0.;
    }
}

struct InitFilters {
    InitFilters() {
        for(size_t i=1;i<MAX_FILTER_WIDTH;++i) {
            g_filters[i].resize(i);

            for(size_t j=0;j<i;++j) {
                double x=j+.5-i/2.;

                g_filters[i][j]=(float)L(x,(int)i);
            }
        }
    }
};

static InitFilters g_init_filters;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void GetFilterForWidth(const float **values,size_t *num_values,size_t width) {
    ASSERT(width>0);

    if(width>=MAX_FILTER_WIDTH) {
        width=MAX_FILTER_WIDTH-1;
    }

    std::vector<float> *filter=&g_filters[width];

    *values=filter->data();
    *num_values=filter->size();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
