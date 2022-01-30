#include <Windows.h>

static UINT RudeWindowFixer_shellhookMessage;

static __declspec(noreturn) void RudeWindowFixer_Error(LPCWSTR text) {
	MessageBoxW(NULL, text, L"RudeWindowFixer error", MB_ICONERROR);
	exit(EXIT_FAILURE);
}

static void RudeWindowFixer_TriggerRudeWindowRecalculation(void) {
	// Broadcast a dummy HSHELL_MONITORCHANGED message. This message will trigger CGlobalRudeWindowManager to recalculate its state.
	// According to reverse engineering, HSHELL_MONITORCHANGED is the message that is processed in the most direct, straightforward, side-effect-free manner by CGlobalRudeWindowManager.
	// TODO: this is a "shotgun" approach that broadcasts to all applications for simplicity. This presumably wakes up a ton of processes, which is inefficient. A cleaner approach could be to locate the specific window CGlobalRudeWindowManager is listening on.
	DWORD recipients = BSM_APPLICATIONS;
	if (BroadcastSystemMessage(BSF_POSTMESSAGE, &recipients, RudeWindowFixer_shellhookMessage, HSHELL_MONITORCHANGED, 0) < 0)
		RudeWindowFixer_Error(L"BroadcastSystemMessage() failed");
}

static LRESULT CALLBACK RudeWindowFixer_WindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	const UINT_PTR timerId = 1;

	if (uMsg == WM_TIMER) KillTimer(hWnd, timerId);

	if (uMsg == RudeWindowFixer_shellhookMessage) {
		// This will reset the timer if it's already running, which probably makes the most sense - we only want to trigger recalculation after things have settled.
		if (SetTimer(hWnd, /*nIDEvent=*/timerId, /*uElapse=*/50, /*lpTimerFunc=*/NULL) == 0)
			RudeWindowFixer_Error(L"SetTimer() failed");
	}

	if (uMsg == WM_CREATE || uMsg == WM_TIMER)
		RudeWindowFixer_TriggerRudeWindowRecalculation();

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
