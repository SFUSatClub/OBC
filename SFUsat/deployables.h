/*
 * deploy.h
 *
 *  Created on: May 7, 2018
 *      Author: Richard
 *
 *      - when we start up, this task will start
 *      - a software timer will be started.
 *      - this will be compared with the RTC
 */

#ifndef SFUSAT_DEPLOYABLES_H_
#define SFUSAT_DEPLOYABLES_H_
#include "FreeRTOS.h"
#include "rtos_task.h"
#include "sfu_hardwaredefs.h"

#ifdef DEPLOY_TESTMODE			/* #defined in sfu_hwdefs */
#define DEPLOY_DELAY	120		/* 2 minutes */
#else
#define DEPLOY_DELAY	1800 	/* 30 minutes */
#endif

/* user tweakable */
#define DEPLOY_CHECK_INTERVAL 		30000		/* 30 seconds */
#define DEPLOY_MAX_BURN_SECONDS		12			/* 12 seconds */

extern TaskHandle_t deployTaskHandle;
void deploy_task(void *pvParameters);
uint16_t deploy_inhibit();		/* command to cancel deployment (for bench testing) */

#endif /* SFUSAT_DEPLOYABLES_H_ */