extern int arr_q[5],maxcons;
extern int head;
extern int tail;
sid32 mutex2,mutex,finallydone;
void consumer_bb(int id, int count);
void producer_bb(int id, int count);