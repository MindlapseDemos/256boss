/*
pcboot - bootable PC demo/game kernel
Copyright (C) 2018-2019  John Tsiombikas <nuclear@member.fsf.org>

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
#ifndef TIME_H_
#define TIME_H_

typedef long time_t;

struct tm {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

time_t time(time_t *tp);
char *asctime(struct tm *tm);
char *asctime_r(struct tm *tm, char *buf);

time_t mktime(struct tm *tm);
struct tm *gmtime(time_t *tp);
struct tm *gmtime_r(time_t *tp, struct tm *tm);

/* non-standard helpers */
int day_of_year(int year, int mon, int day);


#endif	/* TIME_H_ */
