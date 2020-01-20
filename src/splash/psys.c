#include <stdlib.h>
#include "psys.h"


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
	return 0;
}

void destroy_emitter(struct emitter *ps)
{
	free(ps->plist);
}

void update_psys(struct emitter *ps, long msec)
{
	int i;
	struct particle *p;
	long dtms = msec - ps->prev_upd;
	float dt = (float)dtms / 1000.0f;
	ps->prev_upd = msec;

	ps->spawn_acc += dtms << 8;
	while(ps->spawn_acc > ps->spawn_rate) {
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
			p->vx = (float)rand() / (float)RAND_MAX;
			p->vy = (float)rand() / (float)RAND_MAX;
			p->life = ps->plife;
			p->col = ps->pcol_start;
		}

		ps->spawn_acc -= ps->spawn_rate;
	}

	ps->fy = -9;

	p = ps->plist;
	for(i=0; i<ps->pmax; i++) {
		if(p->life > 0) p->life -= dtms;
		if(p->life > 0) {
			p->x += p->vx * dt;
			p->y += p->vy * dt;
			p->vx = p->vx * 0.9999 + ps->fx * dt;
			p->vy = p->vy * 0.9999 + ps->fy * dt;
		}
	}

	ps->fx = ps->fy = 0;
}
