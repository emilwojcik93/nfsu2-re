/* The single shared "play FF effect by index" dispatcher (0x5BFEE0) is called
   from 21 distinct sites scattered through the vehicle/collision code (found
   live via Cheat Engine find_call_references). Our vtable-level hook only
   ever sees 0x5BFF18 as the caller of IDirectInputEffect::Start, because
   that's the return address inside THIS dispatcher, not whoever called it.
   This hook patches the dispatcher's own entry point directly (known static
   game address, same mkjmp() technique the rest of this project uses) to
   log the real external caller + index, so each of the 21 call sites can be
   told apart in the log.

   First version of this hook used pushad/popad to save every register
   before logging, which crashed the game on the very first control-handoff
   frame (0x5BFF0F, null effect-pointer deref) -- the extra ~20 instructions
   of overhead on this very hot, timing-sensitive dispatcher was enough to
   expose what looks like a pre-existing startup race in the stock game code
   (the array at [esi+edi*4+10] isn't fully populated yet on that very first
   call). This version only touches EAX/EDX, which are caller-volatile by
   convention -- neither the original overwritten bytes nor anything later
   in the function relies on their incoming value, so there is nothing to
   save/restore. ECX ('this'), EBX, ESI, EDI, EBP are never written, only
   read (ECX for the log, EBX by the replicated original instruction), so
   they need no protection either. Total added cost per call: 2 memory
   reads, 3 pushes, 1 call, 1 stack fixup -- vs. pushad/popad's 64 bytes of
   register traffic before. */

static void LogDispatcherCallLight(void *retaddr, void *thisObj, int index)
{
	log(buf, sprintf(buf,
		"FF Dispatch retaddr=%p this=%p index=%d",
		retaddr, thisObj, index));
}

static
__declspec(naked) void FFDispatcherHookLight()
{
	_asm {
		mov edx, [esp+8]    /* index (2nd stack arg), read before touching esp */
		mov eax, [esp]      /* caller retaddr */
		push edx
		push ecx            /* this -- plain register read, doesn't disturb it */
		push eax
		call LogDispatcherCallLight
		add esp, 12
		/* replicate the 5 bytes mkjmp overwrote: mov eax,[esp+8]; push ebx */
		mov eax, [esp+8]
		push ebx
		mov ecx, 0x5BFEE5
		jmp ecx
	}
}

static
void initHookFFDispatcherCaller()
{
	mkjmp(0x5BFEE0, &FFDispatcherHookLight);

	INIT_FUNC();
#undef INIT_FUNC
#define INIT_FUNC initHookFFDispatcherCaller
}
