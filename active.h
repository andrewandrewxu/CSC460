#include <string.h>
#include <avr/io.h>
#include <avr/delay.h>
#include <time.h>
#include <avr/interrupt.h>
/**
 * \file active.c
 * \brief A Skeleton Implementation of an RTOS
 * 
 * \mainpage A Skeleton Implementation of a "Full-Served" RTOS Model
 * This is an example of how to implement context-switching based on a 
 * full-served model. That is, the RTOS is implemented by an independent
 * "kernel" task, which has its own stack and calls the appropriate kernel 
 * function on behalf of the user task.
 *
 * \author Dr. Mantis Cheng
 * \date 29 September 2006
 *
 * ChangeLog: Modified by Alexander M. Hoole, October 2006.
 *        -Rectified errors and enabled context switching.
 *        -LED Testing code added for development (remove later).
 *
 * \section Implementation Note
 * This example uses the ATMEL AT90USB1287 instruction set as an example
 * for implementing the context switching mechanism. 
 * This code is ready to be loaded onto an AT90USBKey.  Once loaded the 
 * RTOS scheduling code will alternate lighting of the GREEN LED light on
 * LED D2 and D5 whenever the correspoing PING and PONG tasks are running.
 * (See the file "cswitch.S" for details.)
 */

//Comment out the following line to remove debugging code from compiled version.
// #define DEBUG

typedef void (*voidfuncptr) (void);      /* pointer to void f(void) */ 

#define WORKSPACE     256
//#define MAXPROCESS   4


/*===========
  * RTOS Internal
  *===========
  */

/**
  * This internal kernel function is the context switching mechanism.
  * It is done in a "funny" way in that it consists two halves: the top half
  * is called "Exit_Kernel()", and the bottom half is called "Enter_Kernel()".
  * When kernel calls this function, it starts the top half (i.e., exit). Right in
  * the middle, "Cp" is activated; as a result, Cp is running and the kernel is
  * suspended in the middle of this function. When Cp makes a system call,
  * it enters the kernel via the Enter_Kernel() software interrupt into
  * the middle of this function, where the kernel was suspended.
  * After executing the bottom half, the context of Cp is saved and the context
  * of the kernel is restore. Hence, when this function returns, kernel is active
  * again, but Cp is not running any more. 
  * (See file "switch.S" for details.)
  */
extern void CSwitch();
extern void Exit_Kernel();    /* this is the same as CSwitch() */

/* Prototype */
void Task_Terminate(void);

/** 
  * This external function could be implemented in two ways:
  *  1) as an external function call, which is called by Kernel API call stubs;
  *  2) as an inline macro which maps the call into a "software interrupt";
  *       as for the AVR processor, we could use the external interrupt feature,
  *       i.e., INT0 pin.
  *  Note: Interrupts are assumed to be disabled upon calling Enter_Kernel().
  *     This is the case if it is implemented by software interrupt. However,
  *     as an external function call, it must be done explicitly. When Enter_Kernel()
  *     returns, then interrupts will be re-enabled by Enter_Kernel().
  */ 
extern void Enter_Kernel();

#define Disable_Interrupt()   asm volatile ("cli"::)
#define Enable_Interrupt()    asm volatile ("sei"::)


typedef struct task_queue_type {
    uint8_t len;
    struct PD * head;
    struct PD * tail;
} task_queue_t;

task_queue_t system_T;
task_queue_t periodic_T;
task_queue_t rr_T;



/**
  *  This is the set of states that a task can be in at any given time.
  */
typedef enum process_states 
{ 
   DEAD = 0, 
   READY, 
   RUNNING, 
   DELAY,
   SENDBLOCK,
   REPLYBLOCK,
   RECEIVEBLOCK
} PROCESS_STATES;

/**
  * This is the set of kernel requests, i.e., a request code for each system call.
  */
typedef enum kernel_request_type 
{
   NONE = 0,
   CREATE,
   NEXT,
   TERMINATE,
   SEND,
   RECEIVE,
   REPLY
} KERNEL_REQUEST_TYPE;

typedef enum priority_level{
    SYSTEM = 0,
    PERIODIC,
    RR
} PRIORITY_LEVELS;
/**
  * Each task is represented by a process descriptor, which contains all
  * relevant information about this task. For convenience, we also store
  * the task's stack, i.e., its workspace, in here.
  */
typedef struct MessageDescriptor{
    unsigned char m_type;
    unsigned int m_val;
    unsigned int m_rpy;
    PID m_id;
}MESSAGE;

typedef struct ProcessDescriptor 
{
   unsigned char *sp;   /* stack pointer into the "workSpace" */
   unsigned char workSpace[WORKSPACE]; 
   PROCESS_STATES state;
   voidfuncptr  code;   /* function to be executed as a task */
   KERNEL_REQUEST_TYPE request;
   int arg;
   unsigned char taskType;
   int rtnVal; /* return value from kernel request */
   unsigned int quantum;

    // The remaining number of ticks for the process
    uint8_t ticks_remaining;
    // The next tick number when to run
    uint32_t next_start;
    // The period of the process (in ticks) (only used for PERIODIC task)
    uint32_t period;
    // The worst case execution time of the process (in ticks) (only used for PERIODIC task)
    uint32_t wcet;
    // A pointer to the next item in the linked list (or NULL if none)

    struct task_queue_t      *senders;    /* queue of senders           */
    struct task_queue_t      *replies;    /* process waiting for reply  */
    struct ProcessDescriptor    *recipient; /* of my message */

    struct ProcessDescriptor* next;
    struct MESSAGE * message;

} PD;


struct ProcessQueue{
    struct ProcessDescriptor *head;
    struct ProcessDescriptor *tail;
};
/**
  * This table contains ALL process descriptors. It doesn't matter what
  * state a task is in.
  */
static PD Process[MAXTHREAD];

/**
  * The process descriptor of the currently RUNNING task.
  */
volatile static PD* Cp; 

/** 
  * Since this is a "full-served" model, the kernel is executing using its own
  * stack. We can allocate a new workspace for this kernel stack, or we can
  * use the stack of the "main()" function, i.e., the initial C runtime stack.
  * (Note: This and the following stack pointers are used primarily by the
  *   context switching code, i.e., CSwitch(), which is written in assembly
  *   language.)
  */         
volatile unsigned char *KernelSp;

/**
  * This is a "shadow" copy of the stack pointer of "Cp", the currently
  * running task. During context switching, we need to save and restore
  * it into the appropriate process descriptor.
  */
volatile unsigned char *CurrentSp;

/** index to next task to run */
volatile static unsigned int NextP;  

/** 1 if kernel has been started; 0 otherwise. */
volatile static unsigned int KernelActive;  

/** number of tasks created so far */
volatile static unsigned int Tasks; 

void Kernel_Create_Task_At( PD *p, voidfuncptr f );
static void Kernel_Create_Task( voidfuncptr f );
static void Dispatch();
static void Next_Kernel_Request();
void OS_Init();
void OS_Start();
void Task_Create( voidfuncptr f);
void Task_Next();
void Task_Terminate();
void Ping();
void Pong();