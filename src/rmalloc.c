#include "rmalloc.h"
/*
	These are the internal malloc routines which are OPTIONAL, and can
	be enabled by editing conf.h and uncommenting the INTERNALMALLOC
	directive.
	
	The test case for these routines can be found by invoking make with
	the argument malloctestproxy. A executable 'malloctestproxy' will be
	produced. Do NOT run this directly. Instead, invoke the Python script
	in the file 'malloctestproxy.py'. The Python script will then invoke
	the executable and communicate with. It will then perform allocations,
	deallocations, writes, and reads to ensure it works in a general sense
	correctly. It can not catch all mistakes, but most.
	
	If you change this file it is recommened that you run the Python
	test case to ensure no major bugs are present.
*/

JVM_M_MS                g_jvm_m_ms;

void jvm_m_init() {
	g_jvm_m_ms.first = 0;
}

int jvm_m_give(void *ptr, uintptr size) {
  JVM_M_CH      *chunk;
  JVM_M_MS      *ms;
  JVM_M_PT      *pt;
  
  ms = &g_jvm_m_ms;

  debugf("give ptr:%x\n", ptr);
  
  chunk = (JVM_M_CH*)ptr;
  chunk->size = size - sizeof(JVM_M_CH);
  chunk->mutex = 0;
  // properly account for chunk and part headers
  chunk->free = size - sizeof(JVM_M_CH) - sizeof(JVM_M_PT);
  chunk->next = ms->first;
  ms->first = chunk;

  debugf("ms->first=%x\n", ms->first);

  pt = (JVM_M_PT*)((uintptr)chunk + sizeof(JVM_M_CH));
  pt->fas = chunk->free;
  return JVM_SUCCESS;
}

void *jvm_m_malloc(size) {
  JVM_M_MS              *ms;
  JVM_M_CH              *ch;
  JVM_M_PT              *pt, *_pt;
  int                   free;
  int                   rtotal;
  void                  *ret;
  
  if (!size)
    return 0;
  
  #ifdef INTERNALMALLOC_BUFCHK
  size += 4;
  #endif
  
  ms = &g_jvm_m_ms;
  //debugf("malloc ms->first=%x\n", ms->first);
  for (ch = ms->first; ch != 0; ch = ch->next) {
    // check if block has enough free
    //debugf("check ch->free=%u >= %u\n", ch->free, size);
    _pt = 0;
    if (ch->free >= size) {
      // lock the chunk
      jvm_MutexAquire(&ch->mutex);
      //debugf("mutex aquired\n");
      #ifdef INTERNALMALLOC_BUFCHK
      // corruption checker
      for (
              // grab first part for intialization
              pt = (JVM_M_PT*)((uintptr)ch + sizeof(JVM_M_CH));
              // make sure we have not gone past end of chunk
              ((uintptr)pt - (uintptr)ch) < ch->size;
              // get next part
              pt = (JVM_M_PT*)((uintptr)pt + JVM_M_SIZE(pt->fas) + sizeof(JVM_M_PT))
          )
      {
        rtotal = JVM_M_SIZE(pt->fas);
        ret = (void*)((uintptr)pt + sizeof(JVM_M_PT));
        
        if (!JVM_M_ISUSED(pt->fas))
          continue;
        
        if (((uint32*)&(((uint8*)ret)[rtotal - 4]))[0] != 0x78563412) {
          debugf("heap corruption checker failed on %llx with sig:%x size:%u\n", ret, ((uint32*)&(((uint8*)ret)[rtotal - 4]))[0], rtotal);
          exit(-3);
        }
      }
      #endif
      
      rtotal = -sizeof(JVM_M_PT);
      for (
              // grab first part for intialization
              pt = (JVM_M_PT*)((uintptr)ch + sizeof(JVM_M_CH));
              // make sure we have not gone past end of chunk
              (((uintptr)pt - (uintptr)ch)) < ch->size;
              // get next part
              pt = (JVM_M_PT*)((uintptr)pt + JVM_M_SIZE(pt->fas) + sizeof(JVM_M_PT))
          )
      {
              //debugf("size:%x looking pt=%x pt-ch=%u ch->size:%u pt->fas:%u\n", size, pt, (uintptr)pt - (uintptr)ch, ch->size, pt->fas);
              // track consecutive free blocks
              if (JVM_M_ISUSED(pt->fas)) 
              {
                rtotal = (int)-sizeof(JVM_M_PT);
                _pt = 0;
              } else {
                rtotal += JVM_M_SIZE(pt->fas) + sizeof(JVM_M_PT);
                // if 'pt' is zero then set it if not leave it
                if (!_pt)
                  _pt = pt;
              }
              //debugf("ch:%x _pt:%x pt:%x pt->fas:%x size:%x\n", ch, _pt, pt, pt->fas, size);
              //sleep(1);
              // do we have enough?
              if (rtotal >= size) {
                //  do we have enough at the end to create a new part?
                if ((rtotal - size) > (2 * sizeof(JVM_M_PT))) {
                  // create two blocks one free one used                  
                  _pt->fas = JVM_M_USED | size;
                  pt = _pt;
                  _pt = (JVM_M_PT*)((uintptr)_pt + sizeof(JVM_M_PT) + size);
                  //debugf("1A %llx\n", _pt);
                  _pt->fas = JVM_M_FREE | ((rtotal - size) - sizeof(JVM_M_PT));
                  //debugf("2A\n");
                  debugf("ret[split]: pt:%llx pt->fas:%x _pt:%x _pt->fas:%x\n", pt, pt->fas, _pt, _pt->fas);
                  //ch->free -= sizeof(JVM_M_PT) + size;
                  jvm_MutexRelease(&ch->mutex);
                  ret = (void*)((uintptr)pt + sizeof(JVM_M_PT));
                  #ifdef INTERNALMALLOC_BUFCHK
                  ((uint8*)ret)[size-4] = 0x12;
                  ((uint8*)ret)[size-3] = 0x34;
                  ((uint8*)ret)[size-2] = 0x56;
                  ((uint8*)ret)[size-1] = 0x78;
                  #endif
                  return ret;
                } else {
                  //debugf("2\n");
                  // do not bother splitting use as whole
                  debugf("ret[whole] rtotal:%x\n", rtotal);
                  _pt->fas = JVM_M_USED | rtotal;
                  //ch->free -= sizeof(JVM_M_PT) + rtotal;
                  //debugf("ret: pt[whole]:%x\n", pt);
                  jvm_MutexRelease(&ch->mutex);
                  ret = (void*)((uintptr)_pt + sizeof(JVM_M_PT));
                  #ifdef INTERNALMALLOC_BUFCHK
                  ((uint8*)ret)[rtotal-4] = 0x12;
                  ((uint8*)ret)[rtotal-3] = 0x34;
                  ((uint8*)ret)[rtotal-2] = 0x56;
                  ((uint8*)ret)[rtotal-1] = 0x78;
                  #endif
                  return ret;
                }
              }
      }
      jvm_MutexRelease(&ch->mutex);
    }
  }
  
  return 0;
}

void jvm_m_free(void *ptr) {
	JVM_M_PT		*_ptr;
	
	_ptr = (JVM_M_PT*)((uintptr)ptr - sizeof(JVM_M_PT));
	_ptr->fas = JVM_M_SIZE(_ptr->fas) | JVM_M_FREE;
}
