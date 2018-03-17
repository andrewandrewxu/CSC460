#include "os.h"
#include <string.h>
#include <avr/io.h>
#include <time.h>
#include <avr/interrupt.h>
#include "active.h"

void setup_Timer(){
  //set up timer with prescaler = 64 and CTC mode
  TCCR1B |= (1 << WGM12)|(1 << CS11)|(1 << CS10);

  // initialize counter
  TCNT1 = 0;

  // initialize compare value, required delay = 10ms
  // change to 1ms as 1 TICK
  OCR1A = 2499;

  // enable compare interrupt
  TIMSK1 |= (1 << OCIE1A);

  // enable global interrupts
  // sei();
}

PID   Task_Create_System(void (*f)(void), int arg){
   if (KernelActive ) {
     Disable_Interrupt();
     Cp ->request = CREATE;
     Cp->code = f;
     Cp->arg = arg;
     Cp->taskType = "SYSTEM";
     Enter_Kernel();
     return Cp->rtnVal;
   } else { 
      /* call the RTOS function directly */
       unsigned char ttype;
       ttype = "SYSTEM";
      PD *P = Kernel_Create_Task( f , arg, SYSTEM);
       enqueue(&system_tasks, p);
       return p->rtnVal;
   }
}


PID Task_Create_RR(voidfuncptr f, int arg) {
    PD * p = Kernel_Create_Task(f, RR);
    if (p == NULL) return 0;
    p->arg = arg;
    enqueue(&rr_T, p);
    return p->rtnVal;
}


PID Task_Create_Period(voidfuncptr f, int arg, TICK period, TICK wcet, TICK offset){
    PD * p = Kernel_Create_Task(f, PERIODIC);
    if (p == NULL) return 0;
    p->arg = arg;

    p->period = period;
    p->wcet = wcet;
    p->next_start = num_ticks + offset;
    p->ticks_remaining = wcet;

    Enqueue_periodic_offset(&periodic_T, p);

    return p->rtnVal;
}

void Task_Next()
{
    Disable_Interrupt();
    Cp->state = READY;
    Cp->request = NEXT;
    Enter_Kernel();
}


int Task_GetArg() {
    return Cp->arg;
}


PID  Task_Pid(){
    return Cp->rtnVal;
}

struct PD *ProcessOf(PID id)
{
    unsigned int i;

    if(id>0){
        i=(id-1)%MAXTHREAD;
        if(Process[i].state != DEAD)
            return &(Process[i]);
    }
    return NULL;
}


void Msg_Send( PID  id, MTYPE t, unsigned int *v ){

    struct PD *receiver;
    receiver = ProcessOf(id);

    if(receiver==NULL)return;

    if(receiver->state == RECEIVEBLOCK){
        receiver->message->m_val = *v;
        receiver->message->m_type = t;
        Cp->state = REPLYBLOCK;
        enqueue(&(receiver->replies), Cp);
//        TODO
        //suspend
    }else{
        Cp->state = SENDBLOCK;
        Cp->message->m_val = *v;
        Cp->message->m_type = t;
        enqueue(&(receiver->senders), Cp);
        Dispatch();
    }
}


PID  Msg_Recv( MASK m, unsigned int *v ){
    struct ProcessDescriptor *firstSender;
    firstSender = dequeue(&(Cp->senders));
    if(firstSender == NULL){
        Cp->state = RECEIVEBLOCK;
        Dispatch();
        return;
    }

    Cp->message->m_id = firstSender->rtnVal;
    Cp->message->m_val = *v;

    if((m & firstSender->message->m_type)!=0){
        if(m=="GET"){
            deque(buffer_Q);

            reply(id, Cp->message->m_val);
        }else if(m=="PUT"){
            deposit(Cp->message->m_val, buffer);

            enqueue(buffer_Q, *v);
            reply(id, NULL);
        }else{
            return;
        }
    }else{
        return;
}



//TODO
void Msg_Rply( PID  id, unsigned int r ){
    struct PD *sender;
    sender = ProcessOf(id);
    if(sender == NULL || sender->state != REPLYBLOCK){
        return;
    }

}


//PID   Task_Create_Period(void (*f)(void), int arg, TICK period, TICK wcet, TICK offset){
//
//}


