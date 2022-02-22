#include <Windows.h>

static UINT RudeWindowFixer_shellhookMessage;

#define HSHELL_UNDOCUMENTED_FULLSCREEN_ENTER 0x35
#define HSHELL_UNDOCUMENTED_FULLSCREEN_EXIT 0x36

static __declspec(noreturn) void RudeWindowFixer_Error(LPCWSTR text) {
	MessageBoxW(NULL, text, L"RudeWindowFixer error", MB_ICONERROR);
	exit(EXIT_FAILURE);
}

// TODO: this is a "shotgun" approach that broadcasts to all applications for simplicity. This presumably wakes up a ton of processes, which is inefficient. A cleaner approach could be to locate the specific window CGlobalRudeWindowManager is listening on.
static void RudeWindowFixer_BroadcastShellHookMessage(WPARAM wParam, LPARAM lParam) {
	DWORD recipients = BSM_APPLICATIONS;
	if (BroadcastSystemMessage(BSF_POSTMESSAGE | BSF_IGNORECURRENTTASK, &recipients, RudeWindowFixer_shellhookMessage, wParam, lParam) < 0)
		RudeWindowFixer_Error(L"BroadcastSystemMessage() failed");
}

static BOOL CALLBACK RudeWindowFixer_AdjustWindows_EnumWindowsProc(_In_ HWND window, _In_ LPARAM lParam) {
	UNREFERENCED_PARAMETER(lParam);

	if (!IsWindowVisible(window)) return TRUE;

	const wchar_t* const nonRudeProp = L"NonRudeHWND";

	const LONG_PTR extendedWindowStyles = GetWindowLongPtrW(window, GWL_EXSTYLE);
	if (!(
		// Some sneaky full screen windows (e.g. NVIDIA GeForce Overlay) have WS_EX_LAYERED | WS_EX_NOACTIVATE.
		// Others (e.g. NVIDIA GeForce Overlay DT) have WS_EX_LAYERED | WS_EX_TRANSPARENT.
		// These criteria are probably not foolproof, but it's a start.
		(extendedWindowStyles & WS_EX_LAYERED) && ((extendedWindowStyles & WS_EX_TRANSPARENT) || (extendedWindowStyles & WS_EX_NOACTIVATE)) &&
		GetPropW(window, nonRudeProp) == NULL
		)) return TRUE;

	const wchar_t* const propComment = L"NonRudeHWND was set by https://github.com/dechamps/RudeWindowFixer";
	SetPropW(window, propComment, INVALID_HANDLE_VALUE);
	SetPropW(window, nonRudeProp, INVALID_HANDLE_VALUE);

	// Just in case we set the property too late, also tell the Rude Window Manager to remove the window from its set of full screen windows.
	RudeWindowFixer_BroadcastShellHookMessage(HSHELL_UNDOCUMENTED_FULLSCREEN_EXIT, (LPARAM)window);

	return TRUE;
}

static void RudeWindowFixer_AdjustWindows(void) {
	if (EnumWindows(RudeWindowFixer_AdjustWindows_EnumWindowsProc, 0) == 0)
		RudeWindowFixer_Error(L"EnumWindows() failed");
}

static LRESULT CALLBACK RudeWindowFixer_WindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	const UINT_PTR timerId = 1;

	if (uMsg == WM_TIMER) KillTimer(hWnd, timerId);

	if (uMsg == RudeWindowFixer_shellhookMessage) {
		// This will reset the timer if it's already running, which probably makes the most sense - we only want to trigger recalculation after things have settled.
		if (SetTimer(hWnd, /*nIDEvent=*/timerId, /*uElapse=*/50, /*lpTimerFunc=*/NULL) == 0)
			RudeWindowFixer_Error(L"SetTimer() failed");
	}

	if (uMsg == WM_CREATE || uMsg == WM_TIMER) {
		// Note: we always go through and check *all* visible windows every time.
		// An alternative would be to listen for HSHELL_UNDOCUMENTED_FULLSCREEN_ENTER and only check the window that just entered full screen,
		// but that won't work with windows that become transparent *after* they become full screen (e.g. NVIDIA GeForce Overlay DT)
		RudeWindowFixer_AdjustWindows();

		// Broadcast a dummy HSHELL_MONITORCHANGED message. This message will trigger CGlobalRudeWindowManager to recalculate its state.
		// According to reverse engineering, HSHELL_MONITORCHANGED is the message that is processed in the most direct, straightforward, side-effect-free manner by CGlobalRudeWindowManager.
		// TODO: if AdjustWindows() already sent a full screen exit message, then this is redundant.
		RudeWindowFixer_BroadcastShellHookMessage(HSHELL_MONITORCHANGED, 0);
	}

	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

int APIENTRY WinMain(_In_ HINSTANCE hInst, _In_opt_  HINSTANCE hInstPrev, _In_ PSTR cmdline, _In_ int cmdshow) {
	UNREFERENCED_PARAMETER(hInstPrev);
	UNREFERENCED_PARAMETER(cmdline);
	UNREFERENCED_PARAMETER(cmdshow);

	RudeWindowFixer_shellhookMessage = RegisterWindowMessageW(L"SHELLHOOK");
	if (RudeWindowFixer_shellhookMessage == 0)
		RudeWindowFixer_Error(L"RegisterWindowMessageW() failed");

	WNDCLASSEXW windowClass = { 0 };
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = RudeWindowFixer_WindowProcedure;
	windowClass.lpszClassName = L"RudeWindowFixer";

	if (RegisterClassExW(&windowClass) == 0)
		RudeWindowFixer_Error(L"RegisterClassExW() failed");

	const HWND window = CreateWindowW(
		/*lpClassName=*/L"RudeWindowFixer",
		/*lpWindowName=*/L"RudeWindowFixer",
		/*dwStyle=*/0,
		/*X=*/CW_USEDEFAULT,
		/*Y=*/CW_USEDEFAULT,
		/*nWidth=*/CW_USEDEFAULT,
		/*nHeight=*/CW_USEDEFAULT,
		/*hWndParent=*/HWND_MESSAGE,
		/*hMenu=*/NULL,
		/*hInstance=*/hInst,
		/*lpParam=*/NULL
	);
	if (window == NULL)
		RudeWindowFixer_Error(L"CreateWindowW() failed");

	if (!RegisterShellHookWindow(window))
		RudeWindowFixer_Error(L"RegisterShellHookWindow() failed");

	for (;;)
	{
		MSG message;
		BOOL result = GetMessage(&message, NULL, 0, 0);
		if (result == -1)
			RudeWindowFixer_Error(L"GetMessage() failed");
		if (result == 0) return 0;
		DispatchMessage(&message);
	}
}
