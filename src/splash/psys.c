/*
256boss - bootable launcher for 256byte intros
Copyright (C) 2018-2020  John Tsiombikas <nuclear@member.fsf.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY, without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <stdlib.h>
#include "psys.h"

static void evalcurve(float *cv, int numcv, float t, float *xret, float *yret);
static inline float bspline(float a, float b, float c, float d, float t);

int create_emitter(struct emitter *ps, int count)
{
	if(!(ps->plist = calloc(count, sizeof *ps->plist))) {
		return -1;
	}
	ps->pmax = count;
	ps->plife = 1000;
	ps->pcol_start = 0;
	ps->pcol_end = 255;
	ps->spawn_rate = SPAWN_PER_SEC(10, 1);
	ps->damping = 0.9999;
	ps->grav_y = 9;
	ps->curve_scale_x = ps->curve_scale_y = 1.0f;
	ps->curve_tend = 1.0f;
	return 0;
}

void destroy_emitter(struct emitter *ps)
{
	free(ps->plist);
}

#define frand()		((float)rand() / (float)RAND_MAX)

void update_psys(struct emitter *ps, long msec)
{
	int i;
	struct particle *p;
	long dtms;
	float dt;

	if(ps->prev_upd <= 0) {
		ps->prev_upd = msec;
		return;
	}
	dtms = msec - ps->prev_upd;
	dt = (float)dtms / 1000.0f;
	ps->prev_upd = msec;

	ps->spawn_acc += ps->spawn_rate * dtms / 1000;
	while(ps->spawn_acc >= 0x100) {
		p = 0;
		for(i=0; i<ps->pmax; i++) {
			if(ps->plist[i].life <= 0) {
				p = ps->plist + i;
				break;
			}
		}

		if(p) {
			p->x = ps->x;
			p->y = ps->y;
			if(ps->x_range != 0.0f) p->x += ps->x_range * frand() - ps->x_range * 0.5f;
			if(ps->y_range != 0.0f) p->y += ps->y_range * frand() - ps->y_range * 0.5f;
			p->vx = ps->vx;
			p->vy = ps->vy;
			if(ps->vx_range != 0.0f) p->vx += ps->vx_range * frand() - ps->vx_range * 0.5f;
			if(ps->vy_range != 0.0f) p->vy += ps->vy_range * frand() - ps->vy_range * 0.5f;
			p->life = ps->plife;
			if(ps->plife_range) p->life += rand() % ps->plife_range - ps->plife_range / 2;
			p->max_life = p->life;
			p->col = ps->pcol_start;

			if(ps->curve_cv) {
				float cx, cy, t, tlen;
				tlen = ps->curve_tend - ps->curve_tbeg;
				t = ps->curve_tbeg + frand() * tlen;
				evalcurve(ps->curve_cv, ps->curve_num_cv, t, &cx, &cy);
				p->x += cx * ps->curve_scale_x;
				p->y += cy * ps->curve_scale_y;
			}
		}

		ps->spawn_acc -= 0x100;
	}

	ps->fx = ps->grav_x;
	ps->fy = ps->grav_y;

	p = ps->plist;
	for(i=0; i<ps->pmax; i++) {
		if(p->life > 0) {
			p->life -= dtms;
			if(p->life > 0) {
				p->x += p->vx * dt;
				p->y += p->vy * dt;
				p->vx = p->vx * ps->damping + ps->fx * dt;
				p->vy = p->vy * ps->damping + ps->fy * dt;

				/* lerp is backwards because life starts max and decreases */
				p->col = ps->pcol_end + (ps->pcol_start - ps->pcol_end) * p->life / p->max_life;
			} else {
				p->life = 0;
			}
		}
		p++;
	}

	ps->fx = ps->fy = 0;
}

void evalcurve(float *cv, int numcv, float t, float *xret, float *yret)
{
	int idx0, idx1, prev, next;
	float dt, t0, t1, fnseg;

	if(numcv < 1) {
		*xret = *yret = 0;
		return;
	}
	if(numcv == 1) {
		*xret = cv[0];
		*yret = cv[1];
		return;
	}

	if(t < 0.0f) t = 0.0f;
	if(t > 1.0f) t = 1.0f;

	fnseg = (float)(numcv - 1);
	idx0 = (int)(t * fnseg);
	if(idx0 > numcv - 2) idx0 = numcv - 2;
	idx1 = idx0 + 1;

	dt = 1.0f / fnseg;
	t0 = (float)idx0 * dt;
	t1 = (float)idx1 * dt;

	t = (t - t0) / (t1 - t0);

	if(numcv == 2) {
		*xret = cv[0] + (cv[2] - cv[0]) * t;
		*yret = cv[1] + (cv[3] - cv[1]) * t;
		return;
	}

	prev = idx0 <= 0 ? idx0 : idx0 - 1;
	next = idx1 >= numcv - 1 ? idx1 : idx1 + 1;

	prev <<= 1;
	next <<= 1;
	idx0 <<= 1;
	idx1 <<= 1;

	*xret = bspline(cv[prev++], cv[idx0++], cv[idx1++], cv[next++], t);
	*yret = bspline(cv[prev], cv[idx0], cv[idx1], cv[next], t);
}

static inline float bspline(float a, float b, float c, float d, float t)
{
	static const float m[] = {
		-1, 3, -3, 1,
		3, -6, 0, 4,
		-3, 3, 3, 1,
		1, 0, 0, 0
	};
	float x, y, z, w;
	float tsq = t * t;
	float tcb = tsq * t;

	x = (a * m[0] + b * m[4] + c * m[8] + d * m[12]) * (1.0f / 6.0f);
	y = (a * m[1] + b * m[5] + c * m[9] + d * m[13]) * (1.0f / 6.0f);
	z = (a * m[2] + b * m[6] + c * m[10] + d * m[14]) * (1.0f / 6.0f);
	w = (a * m[3] + b * m[7] + c * m[11] + d * m[15]) * (1.0f / 6.0f);
	return x * tcb + y * tsq + z * t + w;
}

