#include "stack.h"

void jvm_StackInit(JVMStack *stack, uint32 max) {
  stack->max = max;
  stack->pos = 0;
  stack->data = (uint64*)malloc(sizeof(uint64) * max);
  stack->flags = (uint32*)malloc(sizeof(uint32) * max);
}

void jvm_StackFree(JVMStack *stack) {
  free(stack->data);
  free(stack->flags);
}

int jvm_StackMore(JVMStack *stack) {
  return stack->pos;
}

void jvm_DebugStack(JVMStack *stack) {
  int           x;

  debugf("DBGSTACK stack->pos:%u\n", stack->pos);
  for (x = stack->pos - 1; x > -1; --x)
  {
    debugf("STACK[%u]: %u:%u\n", x, stack->data[x], stack->flags[x]);
  }
}

void jvm_StackDiscardTop(JVMStack *stack) {
  stack->pos--;
}

void jvm_StackPush(JVMStack *stack, int64 value, uint32 flags) {
  stack->flags[stack->pos] = flags;
  stack->data[stack->pos] = value;
  stack->pos++;
  debugf("stack push pos:%u\n", stack->pos);
  debugf("value:%u flags:%x\n", value, flags);
  debugf("$$$\n");
  if (flags & JVM_STACK_ISOBJECTREF)
    if (value != 0)
      ((JVMObject*)value)->stackCnt++;
  jvm_DebugStack(stack);
}

void jvm_StackPop(JVMStack *stack, JVMLocal *local) {
  --stack->pos;
  local->flags = stack->flags[stack->pos];
  local->data = stack->data[stack->pos];
  if (local->flags & JVM_STACK_ISOBJECTREF)
    if (local->data != 0)
      ((JVMObject*)local->data)->stackCnt--;
  debugf("stack-pop: pos:%u\n", stack->pos);
}