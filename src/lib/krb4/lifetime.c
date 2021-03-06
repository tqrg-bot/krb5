/*
 * Copyright 2000, 2001 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 */

#include "krb.h"

/*
 * Only lifetime bytes values less than 128 are on a linear scale.
 * The following table contains an exponential scale that covers the
 * lifetime values 128 to 191 inclusive (a total of 64 values).
 * Values greater than 191 get interpreted the same as 191, but they
 * will never be generated by the functions in this file.
 *
 * The ratio is approximately 1.069144898 (actually exactly
 * exp(log(67.5)/63), where 67.5 = 2592000/38400, and 259200 = 30
 * days, and 38400 = 128*5 minutes.  This allows a lifetime byte of
 * 191 to correspond to a ticket life of exactly 30 days and a
 * lifetime byte of 128 to correspond to exactly 128*5 minutes, with
 * the other values spread on an exponential curve fit in between
 * them.  This table should correspond exactly to the set of extended
 * ticket lifetime values used by AFS and CMU.
 *
 * The following awk script is sufficient to reproduce the table:
 * BEGIN {
 *     r = exp(log(2592000/38400)/63);
 *     x = 38400;
 *     for (i=0;i<64;i++) {
 *         printf("%d\n",x+0.5);
 *         x *= r;
 *     }
 * }
 */
#ifndef SHORT_LIFETIME
#define NLIFETIMES 64
static const KRB4_32 lifetimes[NLIFETIMES] = {
    38400, 41055,		/* 00:10:40:00, 00:11:24:15 */
    43894, 46929,		/* 00:12:11:34, 00:13:02:09 */
    50174, 53643,		/* 00:13:56:14, 00:14:54:03 */
    57352, 61318,		/* 00:15:55:52, 00:17:01:58 */
    65558, 70091,		/* 00:18:12:38, 00:19:28:11 */
    74937, 80119,		/* 00:20:48:57, 00:22:15:19 */
    85658, 91581,		/* 00:23:47:38, 01:01:26:21 */
    97914, 104684,		/* 01:03:11:54, 01:05:04:44 */
    111922, 119661,		/* 01:07:05:22, 01:09:14:21 */
    127935, 136781,		/* 01:11:32:15, 01:13:59:41 */
    146239, 156350,		/* 01:16:37:19, 01:19:25:50 */
    167161, 178720,		/* 01:22:26:01, 02:01:38:40 */
    191077, 204289,		/* 02:05:04:37, 02:08:44:49 */
    218415, 233517,		/* 02:12:40:15, 02:16:51:57 */
    249664, 266926,		/* 02:21:21:04, 03:02:08:46 */
    285383, 305116,		/* 03:07:16:23, 03:12:45:16 */
    326213, 348769,		/* 03:18:36:53, 04:00:52:49 */
    372885, 398668,		/* 04:07:34:45, 04:14:44:28 */
    426234, 455705,		/* 04:22:23:54, 05:06:35:05 */
    487215, 520904,		/* 05:15:20:15, 06:00:41:44 */
    556921, 595430,		/* 06:10:42:01, 06:21:23:50 */
    636601, 680618,		/* 07:08:50:01, 07:21:03:38 */
    727680, 777995,		/* 08:10:08:00, 09:00:06:35 */
    831789, 889303,		/* 09:15:03:09, 10:07:01:43 */
    950794, 1016537,		/* 11:00:06:34, 11:18:22:17 */
    1086825, 1161973,		/* 12:13:53:45, 13:10:46:13 */
    1242318, 1328218,		/* 14:09:05:18, 15:08:56:58 */
    1420057, 1518247,		/* 16:10:27:37, 17:13:44:07 */
    1623226, 1735464,		/* 18:18:53:46, 20:02:04:24 */
    1855462, 1983758,		/* 21:11:24:22, 22:23:02:38 */
    2120925, 2267576,		/* 24:13:08:45, 26:05:52:56 */
    2424367, 2592000		/* 28:01:26:07, 30:00:00:00 */
};
#define MINFIXED 0x80
#define MAXFIXED (MINFIXED + NLIFETIMES - 1)
#endif /* !SHORT_LIFETIME */

/*
 * krb_life_to_time
 *
 * Given a start date and a lifetime byte, compute the expiration
 * date.
 */
KRB4_32 KRB5_CALLCONV
krb_life_to_time(KRB4_32 start, int life)
{
    if (life < 0 || life > 255)	/* possibly sign botch in caller */
	return start;
#ifndef SHORT_LIFETIME
    if (life < MINFIXED)
	return start + life * 5 * 60;
    if (life > MAXFIXED)
	return start + lifetimes[NLIFETIMES - 1];
    return start + lifetimes[life - MINFIXED];
#else  /* SHORT_LIFETIME */
    return start + life * 5 * 60;
#endif /* SHORT_LIFETIME */
}

/*
 * krb_time_to_life
 *
 * Given the start date and the end date, compute the lifetime byte.
 * Round up, since we can adjust the start date backwards if we are
 * issuing the ticket to cause it to expire at the correct time.
 */
int KRB5_CALLCONV
krb_time_to_life(KRB4_32 start, KRB4_32 end)
{
    KRB4_32 dt;
#ifndef SHORT_LIFETIME
    int i;
#endif

    dt = end - start;
    if (dt <= 0)
	return 0;
#ifndef SHORT_LIFETIME
    if (dt < lifetimes[0])
	return (dt + 5 * 60 - 1) / (5 * 60);
    /* This depends on the array being ordered. */
    for (i = 0; i < NLIFETIMES; i++) {
	if (lifetimes[i] >= dt)
	    return i + MINFIXED;
    }
    return MAXFIXED;
#else  /* SHORT_LIFETIME */
    if (dt > 5 * 60 * 255)
	return 255;
    else
	return (dt + 5 * 60 - 1) / (5 * 60);
#endif /* SHORT_LIFETIME */
}
