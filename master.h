//
// Created by ss22 on 3/7/25.
//

#ifndef ADAPTIVEREGULATOR_MASTER_H
#define ADAPTIVEREGULATOR_MASTER_H

/* Master thread states */
#define MASTER_STATE_INITIAL   0
#define MASTER_STATE_RUNNING   1
#define MASTER_STATE_STOPPED   2

void initialize_master(void);
void deinitialize_master(void);

/* New functions to control master thread state */
void master_start_regulation(void);
void master_stop_regulation(void);
int master_get_state(void);

#endif //ADAPTIVEREGULATOR_MASTER_H
