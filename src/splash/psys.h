#ifndef PSYS_H_
#define PSYS_H_

struct particle {
	float x, y, vx, vy;
	long life;
	int col;
};

struct emitter {
	float x, y;
	float fx, fy;

	struct particle *plist;
	int pcount, pmax;
	long spawn_rate, spawn_acc;

	long prev_upd;

	long plife;
	int pcol_start, pcol_end;
};

#define SPAWN_PER_SEC(x, y)	(((x) << 8) / ((y) == 0 ? 1 : (y)))

int create_emitter(struct emitter *ps, int count);
void destroy_emitter(struct emitter *ps);

void update_psys(struct emitter *ps, long msec);

#endif	/* PSYS_H_ */
