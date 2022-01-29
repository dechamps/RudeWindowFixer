#include <Windows.h>

static const UINT APPBAR_MESSAGE = WM_USER;

static void RudeWindowFixer_Error(LPCWSTR text) {
	MessageBoxW(NULL, text, L"RudeWindowFixer error", MB_ICONERROR);
	exit(EXIT_FAILURE);
}

static void RudeWindowFixer_TriggerRudeWindowRecalculation(void) {
	const UINT shellhookMessage = RegisterWindowMessageW(L"SHELLHOOK");
	if (shellhookMessage == 0)
		RudeWindowFixer_Error(L"RegisterWindowMessageW(L\"SHELLHOOK\") failed");

	// Broadcast a dummy HSHELL_MONITORCHANGED message. This message will trigger CGlobalRudeWindowManager to recalculate its state.
	// According to reverse engineering, HSHELL_MONITORCHANGED is the message that is processed in the most direct, straightforward, side-effect-free manner by CGlobalRudeWindowManager.
	// TODO: this is a "shotgun" approach that broadcasts to all applications for simplicity. This presumably wakes up a ton of processes, which is inefficient. A cleaner approach could be to locate the specific window CGlobalRudeWindowManager is listening on.
	DWORD recipients = BSM_APPLICATIONS;
	if (BroadcastSystemMessage(BSF_POSTMESSAGE, &recipients, shellhookMessage, HSHELL_MONITORCHANGED, 0) < 0)
		RudeWindowFixer_Error(L"BroadcastSystemMessage() failed");
}

static LRESULT CALLBACK RudeWindowFixer_WindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	// Once CGlobalRudeWindowManager determines a fullscreen app is in use, CTray::OnRudeWindowStateChange() sends ABN_FULLSCREENAPP 1 message to all appbars.
	if (uMsg == APPBAR_MESSAGE && wParam == ABN_FULLSCREENAPP && lParam) {
		if (SetTimer(hWnd, /*nIDEvent=*/1, /*uElapse=*/50, /*lpTimerFunc=*/NULL) == 0)
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

	APPBARDATA abd;
	abd.cbSize = sizeof(abd);
	abd.hWnd = window;
	abd.uCallbackMessage = APPBAR_MESSAGE;
	if (!SHAppBarMessage(ABM_NEW, &abd))
		RudeWindowFixer_Error(L"SHAppBarMessage(ABM_NEW) failed");

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
