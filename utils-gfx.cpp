#include <algorithm>
#include <stdlib.h>

#include "error.h"

#define max3(a, b, c) std::max(std::max(a, b), c)
#define min3(a, b, c) std::min(std::min(a, b), c)

void rgb_to_hls(double r, double g, double b, double *H, double *L, double *S)
{
	double cmax = max3(r, g, b);
	double cmin = min3(r, g, b);

	*L = (cmax + cmin) / 2.0;

	if (cmax == cmin)
		*S = *H = 0.0; /* undefined */
	else
	{
		if (*L < 0.5)
			*S = (cmax - cmin) / (cmax + cmin);
		else
			*S = (cmax - cmin) / (2.0 - cmax - cmin);

		double delta = cmax - cmin;

		if (r == cmax)
			*H = (g - b)  /delta;
		else 
		{
			if (g == cmax)
				*H = 2.0 + (b - r) / delta;
	  		else
				*H = 4.0 + (r - g) / delta;
		}

	  	*H /= 6.0;
	  	if (*H < 0.0)
			 *H += 1.0;
	}
}

double hue_to_rgb(double m1, double m2, double h)
{
	if (h < 0.0)
		h += 1.0;

	if (h > 1.0)
		h -= 1.0;

	if (6.0 * h < 1.0)
		return (m1 + (m2 - m1) * h * 6.0);

	if (2.0 * h < 1.0)
		return m2;

	if (3.0 * h < 2.0)
		return (m1 + (m2 - m1) * ((2.0 / 3.0) - h) * 6.0);

	return m1;
}

void hls_to_rgb(double H, double L, double S, double *r, double *g, double *b)
{
	if (S == 0)
		*r = *g = *b = L;
	else
	{
		double m2;

		if (L <=0.5)
			m2 = L*(1.0+S);
		else
			m2 = L+S-L*S;

		double m1 = 2.0 * L - m2;

		*r = hue_to_rgb(m1, m2, H + 1.0/3.0);
		*g = hue_to_rgb(m1, m2, H);
		*b = hue_to_rgb(m1, m2, H - 1.0/3.0);
	}
}
