/* FF/rumble event-mapping investigation hook.
   Steals the shared IDirectInputDevice8/IDirectInputEffect vtables (COM
   objects of the same underlying class share one vtable, so hooking via a
   throwaway keyboard device also catches the game's real gamepad device),
   then hooks CreateEffect/Start/SetParameters and logs the caller return
   address so hits can be cross-referenced against SPEED2.idc funcs.html. */

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <intrin.h>
#include "minhook/include/MinHook.h"

#pragma intrinsic(_ReturnAddress)

static const GUID k_IID_IDirectInput8A =
	{0xBF798030,0x483A,0x4DA2,{0xAA,0x99,0x5D,0x64,0xED,0x36,0x97,0x00}};
static const GUID k_GUID_SysKeyboard =
	{0x6F1D2B61,0xD5A0,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00}};

typedef HRESULT (WINAPI *DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

typedef HRESULT (STDMETHODCALLTYPE *CreateEffect_t)(
	LPDIRECTINPUTDEVICE8A, REFGUID, LPCDIEFFECT, LPDIRECTINPUTEFFECT*, LPUNKNOWN);
typedef HRESULT (STDMETHODCALLTYPE *EffectSetParameters_t)(
	LPDIRECTINPUTEFFECT, LPCDIEFFECT, DWORD);
typedef HRESULT (STDMETHODCALLTYPE *EffectStart_t)(
	LPDIRECTINPUTEFFECT, DWORD, DWORD);

static CreateEffect_t oCreateEffect = NULL;
static EffectSetParameters_t oEffectSetParameters = NULL;
static EffectStart_t oEffectStart = NULL;

static int effectHooksInstalled = 0;

static void LogDIEFFECT(const char *tag, LPCDIEFFECT eff)
{
	if (!eff) {
		log(buf, sprintf(buf, "%s: (null DIEFFECT)", tag));
		return;
	}
	log(buf, sprintf(buf,
		"%s: flags=0x%08X duration=%u gain=%u trigbtn=0x%08X cAxes=%u",
		tag, (unsigned)eff->dwFlags, (unsigned)eff->dwDuration,
		(unsigned)eff->dwGain, (unsigned)eff->dwTriggerButton,
		(unsigned)eff->cAxes));

	/* dwGain is just a device-wide scalar; the real per-hit "how hard"
	   value lives in the type-specific struct (DICONSTANTFORCE.lMagnitude /
	   DIPERIODIC.dwMagnitude / DICONDITION coefficients all start at
	   offset 0), which differs by effect type. Dump the first 3 DWORDs
	   generically instead of decoding per-type. */
	if (eff->lpvTypeSpecificParams && eff->cbTypeSpecificParams >= 4) {
		unsigned *p = (unsigned*) eff->lpvTypeSpecificParams;
		unsigned n = eff->cbTypeSpecificParams;
		log(buf, sprintf(buf,
			"%s: typespecific(%u bytes) [0]=%d [1]=%d [2]=%d",
			tag, n, p[0],
			n >= 8 ? (int)p[1] : 0,
			n >= 12 ? (int)p[2] : 0));
	}
}

static HRESULT STDMETHODCALLTYPE Hooked_EffectStart(
	LPDIRECTINPUTEFFECT self, DWORD dwIterations, DWORD dwFlags)
{
	HRESULT hr;

	hr = oEffectStart(self, dwIterations, dwFlags);
	log(buf, sprintf(buf,
		"FF Start         retaddr=%p self=%p iterations=%u flags=0x%08X hr=0x%08X",
		_ReturnAddress(), (void*)self, (unsigned)dwIterations,
		(unsigned)dwFlags, (unsigned)hr));
	return hr;
}

static HRESULT STDMETHODCALLTYPE Hooked_EffectSetParameters(
	LPDIRECTINPUTEFFECT self, LPCDIEFFECT peff, DWORD dwFlags)
{
	HRESULT hr;

	hr = oEffectSetParameters(self, peff, dwFlags);
	log(buf, sprintf(buf,
		"FF SetParameters retaddr=%p self=%p flags=0x%08X hr=0x%08X",
		_ReturnAddress(), (void*)self, (unsigned)dwFlags, (unsigned)hr));
	LogDIEFFECT("  params", peff);
	return hr;
}

static void HookEffectVtableOnce(LPDIRECTINPUTEFFECT effect)
{
	void **vtbl;

	if (effectHooksInstalled || !effect) {
		return;
	}
	vtbl = *(void ***)effect;

	if (MH_CreateHook(vtbl[7], &Hooked_EffectStart, (void**)&oEffectStart) != MH_OK) {
		log(buf, sprintf(buf, "FF: MH_CreateHook(Start) failed"));
	}
	if (MH_CreateHook(vtbl[6], &Hooked_EffectSetParameters, (void**)&oEffectSetParameters) != MH_OK) {
		log(buf, sprintf(buf, "FF: MH_CreateHook(SetParameters) failed"));
	}
	MH_EnableHook(vtbl[7]);
	MH_EnableHook(vtbl[6]);
	effectHooksInstalled = 1;
	log(buf, sprintf(buf, "FF: effect vtable hooked (Start/SetParameters)"));
}

static HRESULT STDMETHODCALLTYPE Hooked_CreateEffect(
	LPDIRECTINPUTDEVICE8A self, REFGUID rguid, LPCDIEFFECT peff,
	LPDIRECTINPUTEFFECT *ppdeff, LPUNKNOWN punkOuter)
{
	HRESULT hr;

	log(buf, sprintf(buf, "FF CreateEffect  retaddr=%p self=%p guid=%08X",
		_ReturnAddress(), (void*)self, (unsigned)rguid->Data1));
	LogDIEFFECT("  params", peff);

	hr = oCreateEffect(self, rguid, peff, ppdeff, punkOuter);

	if (SUCCEEDED(hr) && ppdeff && *ppdeff) {
		HookEffectVtableOnce(*ppdeff);
	}
	return hr;
}

static void StealDeviceVtableAndHookCreateEffect()
{
	HMODULE hDI;
	DirectInput8Create_t pCreate;
	LPDIRECTINPUT8A di = NULL;
	LPDIRECTINPUTDEVICE8A dev = NULL;
	void **vtbl;
	HRESULT hr;

	hDI = LoadLibraryA("dinput8.dll");
	if (!hDI) {
		log(buf, sprintf(buf, "FF: LoadLibrary(dinput8.dll) failed"));
		return;
	}
	pCreate = (DirectInput8Create_t) GetProcAddress(hDI, "DirectInput8Create");
	if (!pCreate) {
		log(buf, sprintf(buf, "FF: GetProcAddress(DirectInput8Create) failed"));
		return;
	}

	hr = pCreate((HINSTANCE) GetModuleHandle(NULL), DIRECTINPUT_VERSION,
		&k_IID_IDirectInput8A, (LPVOID*)&di, NULL);
	if (FAILED(hr) || !di) {
		log(buf, sprintf(buf, "FF: DirectInput8Create failed hr=0x%08X", (unsigned)hr));
		return;
	}

	hr = di->lpVtbl->CreateDevice(di, &k_GUID_SysKeyboard, &dev, NULL);
	if (FAILED(hr) || !dev) {
		log(buf, sprintf(buf, "FF: CreateDevice(keyboard) failed hr=0x%08X", (unsigned)hr));
		di->lpVtbl->Release(di);
		return;
	}

	vtbl = *(void ***)dev;
	if (MH_CreateHook(vtbl[18], &Hooked_CreateEffect, (void**)&oCreateEffect) != MH_OK) {
		log(buf, sprintf(buf, "FF: MH_CreateHook(CreateEffect) failed"));
	} else {
		MH_EnableHook(vtbl[18]);
		log(buf, sprintf(buf, "FF: device vtable hooked (CreateEffect)"));
	}

	dev->lpVtbl->Release(dev);
	di->lpVtbl->Release(di);
}

/* DllMain runs under the loader lock; LoadLibrary + DirectInput8Create in
   there can deadlock/crash the process. Do the real work from a spawned
   thread instead, after DllMain has returned. */
static DWORD WINAPI FFHookThreadProc(LPVOID unused)
{
	StealDeviceVtableAndHookCreateEffect();
	return 0;
}

static void initHookFFEventMapping()
{
	HANDLE hThread = CreateThread(NULL, 0, &FFHookThreadProc, NULL, 0, NULL);
	if (hThread) {
		CloseHandle(hThread);
	}

	INIT_FUNC();
#undef INIT_FUNC
#define INIT_FUNC initHookFFEventMapping
}
