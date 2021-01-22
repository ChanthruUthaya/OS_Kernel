#include "hilevel.h"


pcb_t pcb[ 20 ]; pcb_t* current = NULL; int amount = 1; int pcb_pointer = 1; int last_pcb = 0; pipes propipes[20]; int pipenum = 0;


extern void     main_console();
extern uint32_t tos_console;

void updatePriorities();
int pickhighestpriority();

void dispatch( ctx_t* ctx, pcb_t* prev, pcb_t* next ) {
  char prev_pid = '?', next_pid = '?';

  if( NULL != prev) {
    if(prev->status != STATUS_TERMINATED){
    memcpy( &prev->ctx, ctx, sizeof( ctx_t ) );// preserve execution context of P_{prev}
    }
    prev_pid = '0' + prev->pid;  }
  if( NULL != next ) {
    memcpy( ctx, &next->ctx, sizeof( ctx_t ) ); // restore  execution context of P_{next}
    next_pid = '0' + next->pid;
  }

    PL011_putc( UART0, '[',      true );
    PL011_putc( UART0, prev_pid, true );
    PL011_putc( UART0, '-',      true );
    PL011_putc( UART0, '>',      true );
    PL011_putc( UART0, next_pid, true );
    PL011_putc( UART0, ']',      true );



    current = next;                             // update   executing index   to P_{next}

  return;
}

void schedule( ctx_t* ctx ) {
  int z = pickhighestpriority();

  dispatch(ctx, current, &pcb[z]); //context switch currrent proc to highest priority proc
  pcb[z].status = STATUS_EXECUTING;
  for(int x = 0; x < last_pcb + 1; x++){
    if( x != z && pcb[x].status != STATUS_TERMINATED){
      pcb[x].status = STATUS_READY;
    }
  }

  updatePriorities();
  return;
}

void updatePriorities(){
  for(int x = 0; x < last_pcb + 1; x++){
    if(pcb[x].status == STATUS_READY){
      pcb[x].priority++;
    }
  }
}
void lastPCBupdate(){
    int n = sizeof(pcb)/sizeof(pcb[0]);
    int highest = -1;
    for(int x = 0; x < n; x++ ){
        if(pcb[x].pid > 0 && pcb[x].status != STATUS_TERMINATED){
            if(x > highest){
                highest = x;
            }
        }
    }
    last_pcb = highest;
}

int pickhighestpriority(){
  int highest = -1;
  int index = -1;
  for(int x = 0; x < last_pcb + 1; x++){
    if(pcb[x].status != STATUS_TERMINATED){
      if(pcb[x].priority > highest){
        highest = pcb[x].priority;
        index = x;
      }
    }
  }
  return index;
}

int findFreespace(){
    for(int x = 0; x <last_pcb + 1; x++){
        if(pcb[x].status == STATUS_TERMINATED){
            return x;
        }
    }
    return last_pcb +1;
}

void terminateProcess(ctx_t* ctx){
    int id = ctx->gpr[0];
    for(int x = 0; x < last_pcb + 1; x++){
        if(pcb[x].pid == id && pcb[x].status != STATUS_TERMINATED){
            memset((uint32_t *)(pcb[x].topofstack - 0x00001000), 0, 0x00001000);
            pcb[x].status = STATUS_TERMINATED;
        }
    }
}

int findProcessbyID(int id){
    for(int x = 0; x < last_pcb + 1; x++){
        if(pcb[x].pid == id)
            return x;
    }
    return -1;
}

int findPipeNumbyID(int id){
    int n = sizeof(propipes)/sizeof(propipes[0]);
    for(int x = 0; x < n; x++){
        if (propipes[x].pcbread == id ){
            return (x);
        }
    }
    return -1;
}


void createChild(ctx_t* context){
    pcb_t* child = &pcb[pcb_pointer];
    pcb_t* parent = current;

    memset(child, 0, sizeof(pcb_t));
    memcpy( &child->ctx, context , sizeof(ctx_t));

    child->pid = amount+1;
    child->status = STATUS_READY;
    child->topofstack = (uint32_t)(&(tos_console) - ((uint32_t)pcb_pointer)*(0x00001000));
    child->ctx.sp = (uint32_t)(child->topofstack - (parent->topofstack - context->sp));
    child->priority = current->priority;
    child->ctx.gpr[0] = 0;

    memcpy((void *)(child->topofstack - 0x00001000), (void*)(parent->topofstack - 0x00001000), 0x00001000);
    context->gpr[0] = child->pid;

}

void hilevel_handler_rst(ctx_t* ctx){

  TIMER0->Timer1Load  = 0x00100000; // select period = 2^20 ticks ~= 1 sec
  TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
  TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
  TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
  TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

  GICC0->PMR          = 0x000000F0; // unmask all            interrupts
  GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
  GICC0->CTLR         = 0x00000001; // enable GIC interface
  GICD0->CTLR         = 0x00000001; // enable GIC distributor

  int_enable_irq();


  memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );     // initialise 0-th PCB = Console
  pcb[ 0 ].pid      = 1;  //process ID
  pcb[ 0 ].status   = STATUS_CREATED; //process status
  pcb[ 0 ].ctx.cpsr = 0x50; //processor in USR mode
  pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_console ); //console program
  pcb[ 0 ].ctx.sp   = ( uint32_t )( &tos_console ); //top of console stack
  pcb[ 0 ].topofstack = (uint32_t)(&tos_console);
  pcb[ 0 ].priority = 0; //process execution priority



  dispatch( ctx, NULL, &pcb[ 0 ] );
  return;
}


void hilevel_handler_irq(ctx_t* ctx) {

   uint32_t id = GICC0->IAR;

  // Step 4: handle the interrupt, then clear (or reset) the source.

  if( id == GIC_SOURCE_TIMER0 ) {
    schedule( ctx );TIMER0->Timer1IntClr = 0x01;
  }

  // Step 5: write the interrupt identifier to signal we're done.

  GICC0->EOIR = id;

  return;
}

void hilevel_handler_svc(ctx_t* ctx, uint32_t id) {

    switch(id){
        case 0x00 :{

            schedule(ctx);

            break;
        }
        case 0x01 :{
          int   fd = ( int   )( ctx->gpr[ 0 ] );
          char*  x = ( char* )( ctx->gpr[ 1 ] );
          int    n = ( int   )( ctx->gpr[ 2 ] );


                  for( int i = 0; i < n; i++ ) {
                      PL011_putc( UART0, x[i], true );
                   }

                  ctx->gpr[ 0 ] = n;
                  break;

        }
        case 0x02 :{

            break;
        }
        case 0x03 :{
            createChild(ctx);
            amount += 1;
            lastPCBupdate();
            //dispatch(ctx, current, &pcb[pcb_pointer]);
            pcb_pointer = findFreespace();
            break;

        }
        case 0x04 :{
            memset((uint32_t *)(current->topofstack - 0x00001000), 0, 0x00001000);
            current->status = STATUS_TERMINATED;
            lastPCBupdate();
            pcb_pointer = findFreespace();
            schedule(ctx);
            break;
        }
        case 0x05 :{
            ctx->pc = (uint32_t) ctx->gpr[0];
            ctx->cpsr = 0x50;
            break;
        }
        case 0x06 :{
            terminateProcess(ctx);
            lastPCBupdate();
            pcb_pointer = findFreespace();
            break;
        }
        case 0x08 :{ //create pipe
            int z = (int )ctx->gpr[1];
            switch(z){
                case 1 :{//parent
                    int n = (int) ctx->gpr[0];
                    propipes[pipenum].pipeid = pipenum + 1;
                    propipes[pipenum].pcbwrite = (int)current->pid;
                    propipes[pipenum].pcbread = n;
                    propipes[pipenum].buf = 0;
                    pipenum++;
                    break;
                }
                default :{
                    break;
                }
            }
        }
        case 0x09 :{//write
            int fd = ctx->gpr[0];
            int x = (int)(ctx->gpr[1]);
            propipes[findPipeNumbyID(fd)].buf = x;

            break;
        }
        case 0x0a :{//read
            int i = (current->pid);
            int x = propipes[findPipeNumbyID(i)].buf;
            propipes[findPipeNumbyID(i)].buf = 0;
            ctx->gpr[0] = x;
            break;
        }
        case 0x0b :{
            int fd = (int)(ctx->gpr[1]);
            if((propipes[findPipeNumbyID(fd)].buf) == 1 || (propipes[findPipeNumbyID(fd)].buf) == -1){
                ctx->gpr[0] = 1;
            }
            else{
                ctx->gpr[0] = 0;
            }
            break;
        }
        default :{
            break;
        }
    }
  return;
}
