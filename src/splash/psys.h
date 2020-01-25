#ifndef PSYS_H_
#define PSYS_H_

struct particle {
	float x, y, vx, vy;
	long life, max_life;
	int col;
};

struct emitter {
	float x, y, x_range, y_range;
	float vx, vy, vx_range, vy_range;
	float grav_x, grav_y;
	float fx, fy;

	struct particle *plist;
	int pcount, pmax;
	long spawn_rate, spawn_acc;
	float damping;

	long prev_upd;

	long plife, plife_range;
	int pcol_start, pcol_end;

	float *curve_cv;
	int curve_num_cv;
	float curve_scale_x, curve_scale_y;
	float curve_tbeg, curve_tend;
};

#define SPAWN_PER_SEC(x, y)	(((x) << 8) / ((y) == 0 ? 1 : (y)))

int create_emitter(struct emitter *ps, int count);
void destroy_emitter(struct emitter *ps);

void update_psys(struct emitter *ps, long msec);

#endif	/* PSYS_H_ */
