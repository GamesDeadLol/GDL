

// These are all members of CWeenieObject

void Movement_Init();
void Movement_Shutdown();
void Movement_Think();

void Movement_UpdatePos();
void Movement_UpdateVector();
void Movement_SendUpdate(DWORD dwCell);

double _next_move_think;
Position _last_move_position;
double _last_update_pos = 0.0;