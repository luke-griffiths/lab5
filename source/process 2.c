#include "3140_concur.h"#include "realtime.h"
//Extra lines that ED said to include at the top of the filevolatile int pit1_cnt = 0;volatile int pit0_cnt = 0;
// The process_t structure definition
struct process_state {
	unsigned int *sp;
	unsigned int *orig_sp;
	int n;
	struct process_state *next;	int is_realtime;	realtime_t* start;    //when the process can begin	realtime_t* deadline; //when it must be done by, interpret this as absolute deadline
};
/* the currently running process. current_process must be NULL if no process is running,
    otherwise it must point to the process_t of the currently running process
*/
process_t * current_process = NULL; 
process_t * process_queue = NULL;
process_t * process_tail = NULL;process_t * real_unready_queue = NULL;process_t * real_ready_queue = NULL;
volatile realtime_t current_time;			//consists of seconds and milliseconds as fieldsint process_deadline_met;		//global variable to keep track of number of tasks metint process_deadline_miss;		//global variable to keep track of number of tasks missed
static process_t * pop_front_process() {
	if (!process_queue) return NULL;
	process_t *proc = process_queue;
	process_queue = proc->next;
	if (process_tail == proc) {
		process_tail = NULL;
	}
	proc->next = NULL;
	return proc;
}static process_t * pop_front_rt_process() {	if (!real_ready_queue) return NULL;	//If no queue exists, return null	process_t *proc = real_ready_queue;	//Gets the front process in the queue	real_ready_queue = proc->next;		//Moves first element to the next element	proc->next = NULL;					//Removes proc from the queue	return proc;}
static void push_tail_process(process_t *proc) {
	if (!process_queue) {
		process_queue = proc;
	}
	if (process_tail) {
		process_tail->next = proc;
	}
	process_tail = proc;
	proc->next = NULL;
}
static void process_free(process_t *proc) {
	process_stack_free(proc->orig_sp, proc->n);
	free(proc);
}//Review this code, it could definetly have errors in it, but i believe it is correct.
static void push_ready_rt_process(process_t *proc){	process_t *curr = real_ready_queue;	if (curr == NULL){	//queue is empty		real_ready_queue = proc;		real_ready_queue->next = NULL;		return;	}	int curr_proc_deadline = curr->deadline->sec * 1000 + curr->deadline->msec;	int insert_proc_deadline = proc->deadline->sec * 1000 + proc->deadline->msec;	if(insert_proc_deadline < curr_proc_deadline){		proc->next = curr;		real_ready_queue = proc;		return;	}	int next_proc_deadline;	while(curr->next != NULL){		next_proc_deadline = curr->next->deadline->sec * 1000 + curr->next->deadline->msec;		if(insert_proc_deadline < next_proc_deadline){			proc->next = curr->next;			curr->next = proc;			return;		}		curr = curr->next;	}	proc->next = NULL;	curr->next = proc;}//Review this code, it could definetly have errors in it, but i believe it is correct.static void push_unready_rt_process(process_t *proc){	process_t *curr = real_unready_queue;	if (curr == NULL){	//queue is empty		real_unready_queue = proc;		real_unready_queue->next = NULL;		return;	}	int curr_proc_deadline = curr->deadline->sec * 1000 + curr->deadline->msec;	int insert_proc_deadline = proc->deadline->sec * 1000 + proc->deadline->msec;	if(insert_proc_deadline < curr_proc_deadline){		proc->next = curr;		real_unready_queue = proc;		return;	}	int next_proc_deadline;	while(curr->next != NULL){		next_proc_deadline = curr->next->deadline->sec * 1000 + curr->next->deadline->msec;		if(insert_proc_deadline < next_proc_deadline){			proc->next = curr->next;			curr->next = proc;			return;		}		curr = curr->next;	}	proc->next = NULL;	curr->next = proc;}static void check_unready_queue(void){	process_t* first_el = real_unready_queue;	int actual_time = current_time.sec*1000 + current_time.msec;	int first_el_start = first_el->start->sec * 1000 + first_el->start->msec;	while(actual_time >= first_el_start && first_el != NULL){		process_t* temp = first_el;		first_el = first_el->next;		real_unready_queue = first_el;		push_ready_rt_process(temp);		if(first_el != NULL)			first_el_start = first_el->start->sec * 1000 + first_el->start->msec;	}}
/* Called by the runtime system to select another process.
   "cursp" = the stack pointer for the currently running process
*/
unsigned int * process_select (unsigned int * cursp) {	check_unready_queue();
	if (cursp) {
		// Suspending a process which has not yet finished, save state and make it the tail
		current_process->sp = cursp;		if(current_process->is_realtime)			push_ready_rt_process(current_process);		else			push_tail_process(current_process);
	} else {
		// Check if a process was running, free its resources if one just finished
		if (current_process) {			if(current_process->is_realtime){				int curr_deadline = current_process->deadline->sec * 1000 + current_process->deadline->msec;				int actual_time = current_time.sec * 1000 + current_time.msec;				if(curr_deadline < actual_time)					process_deadline_miss += 1;				else					process_deadline_met += 1;			}
			process_free(current_process);
		}
	}
	// Select the new current process from the front of the queue	
	current_process = NULL;
	if(real_ready_queue){		current_process = pop_front_rt_process();		if(current_process == NULL)			current_process = pop_front_process();	}	else		current_process = pop_front_process();	//This is where it should delay until a process is ready if there isn't a ready process
	if (current_process) {
		// Launch the process which was just popped off the queue		return current_process->sp;
	}	else if(!real_ready_queue && !real_unready_queue)		return NULL;	else {		while(!current_process){			check_unready_queue();			current_process = pop_front_rt_process();			int currTime = 1000*current_time.sec + current_time.msec;			while(1000*current_time.sec + current_time.msec - currTime < 100);		}		return current_process->sp;		//Need to delay here. This means that there is something that needs to happen still, butit just hasnt come yet	}
}

/* Starts up the concurrent execution */
void process_start (void) {
	SIM->SCGC6 |= SIM_SCGC6_PIT_MASK;	//Enabling clock to PIT module
	PIT->MCR = 0;
	PIT->CHANNEL[0].LDVAL = DEFAULT_SYSTEM_CLOCK / 10;
	NVIC_EnableIRQ(PIT_IRQn);
	// Don't enable the timer yet. The scheduler will do so itself	//PIT Timer 1 is to be used for the current time
	PIT->CHANNEL[1].LDVAL = DEFAULT_SYSTEM_CLOCK / 1000;	PIT->CHANNEL[1].TCTRL |= PIT_TCTRL_TEN_MASK;	//enable timer	PIT->CHANNEL[1].TCTRL |= PIT_TCTRL_TIE_MASK;	//enable interrupts	//Check over	PIT->CHANNEL[1].TFLG |= PIT_TFLG_TIF_MASK;	//Setting current time and other global variables	current_time.msec = 0;	current_time.sec = 0;	process_deadline_met = 0;	process_deadline_miss = 0;	//Setting the priorities for the interrupts. The realtime should have a higher priority (0)	//NVIC_SetPriority(SVCall_IRQn, 1);	//NVIC_SetPriority(PIT0_IRQn, 1);	//NVIC_SetPriority(PIT1_IRQn, 0);	NVIC_EnableIRQ(PIT_IRQn);	NVIC_SetPriority(SVCall_IRQn, 1);	NVIC_SetPriority(PIT_IRQn, 0);
	// Bail out fast if no processes were ever created
	if (!process_queue && !real_ready_queue && !real_unready_queue) return;
	process_begin();
}

/* Create a new process */
int process_create (void (*f)(void), int n) {
	unsigned int *sp = process_stack_init(f, n);
	if (!sp) return -1;
	process_t *proc = (process_t*) malloc(sizeof(process_t));
	if (!proc) {
		process_stack_free(sp, n);
		return -1;
	}
	proc->sp = proc->orig_sp = sp;
	proc->n = n;	proc->is_realtime = 0;	proc->start = NULL;	proc->deadline = NULL;
	push_tail_process(proc);	return 0;
}int process_rt_create(void (*f)(void), int n, realtime_t *start, realtime_t *deadline){	unsigned int *sp = process_stack_init(f, n);	if (!sp) return -1;	process_t *proc = (process_t*) malloc(sizeof(process_t));	if (!proc) {		process_stack_free(sp, n);		return -1;	}	proc->sp = proc->orig_sp = sp;	proc->n = n;	proc->is_realtime = 1;	proc->start = start;	int total_msec = (1000 * start->sec + start->msec) + (1000 * deadline->sec + deadline->msec);	deadline->sec = total_msec / 1000;	deadline->msec = total_msec % 1000;	proc->deadline = deadline;	int start_time = current_process->start->sec * 1000 + current_process->start->msec;	int actual_time = current_time.sec * 1000 + current_time.msec;	if(start_time < actual_time)		push_ready_rt_process(proc);	else		push_unready_rt_process(proc);	return 0;}void PIT1_Service(void){	if(current_time.msec >= 999){		current_time.msec = 0;		current_time.sec += 1;	}	else{		current_time.msec +=1;	}	PIT->CHANNEL[1].TFLG = 1; //Clears the timer interrupt flag}
